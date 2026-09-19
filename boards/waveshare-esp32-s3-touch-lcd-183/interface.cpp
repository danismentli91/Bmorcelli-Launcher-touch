#include "idf/launcher_platform.h"
#include "powerSave.h"
#include "input_state.h"
#include <Arduino.h>
#include <interface.h>
#include <Wire.h>

using namespace waveshare183;
static InputState touchInput(true), bootInput(false);
static Debounce bootDebounce;
static bool pmicReady = false;

static bool readRegisters(uint8_t address, uint8_t reg, uint8_t *data, uint8_t count) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(address, count) != count) return false;
    for (uint8_t i = 0; i < count; ++i) data[i] = Wire.read();
    return true;
}

static bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

// CST816x: FingerNum, XposH/L, YposH/L in one coherent transaction.
// An I2C error / all-FF frame is NOT a finger release.
static bool readTouch(bool &pressed, Point &point) {
    uint8_t frame[5];
    if (!readRegisters(TOUCH_ADDR, 0x02, frame, sizeof(frame))) return false;
    if (frame[0] > 1) return false;
    pressed = frame[0] == 1;
    if (pressed) {
        Point raw{int16_t(((frame[1] & 0x0f) << 8) | frame[2]),
                  int16_t(((frame[3] & 0x0f) << 8) | frame[4])};
        if (raw.x >= 240 || raw.y >= 284) return false;
        point = rotateTouch(raw, rotation);
    }
    return true;
}

static void emitAction(Action action) {
    if (action == Action::None) return;
    // Publish one event, never simultaneous coordinate and Select events.
    if (SelPress || EscPress || NextPress || PrevPress) return;
    switch (action) {
    case Action::Select: SelPress = true; break;
    case Action::Back: EscPress = true; break;
    case Action::Next: NextPress = true; break;
    case Action::Previous: PrevPress = true; break;
    default: return;
    }
    AnyKeyPress = true;
}

void _setup_gpio() {
    pinMode(TFT_BL, OUTPUT);
    launcherGpioWrite(TFT_BL, HIGH);
    launcherGpioOutput(TFT_CS);
    launcherGpioWrite(TFT_CS, HIGH);
    launcherGpioOutput(TFT_DC);
    launcherGpioWrite(TFT_DC, HIGH);
    launcherGpioOutput(TFT_RST);
    launcherGpioWrite(TFT_RST, HIGH);
    launcherDelayMs(10);
    launcherGpioWrite(TFT_RST, LOW);
    launcherDelayMs(20);
    launcherGpioWrite(TFT_RST, HIGH);
    launcherDelayMs(120);
    launcherGpioInputPullup(SEL_BTN); // BOOT = GPIO0, active low
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setTimeOut(20);
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(200); // Official DriveBus reset recovery time
    pinMode(TOUCH_INT, INPUT_PULLUP);
    // Periodic touch + state-change reports; ignore hardware GestureID.
    // Host timing is the only authority for single vs. double tap.
    bool touchReady = writeRegister(TOUCH_ADDR, 0xFA, 0x60);
    // Do not disable auto-sleep (0xFE): some CST816T revisions return FF frames.
    uint8_t chip = 0;
    readRegisters(TOUCH_ADDR, 0xA7, &chip, 1);
    launcherConsolePrintf("CST816 init=%d id=0x%02x\n", touchReady, chip);

    // PWR goes to AXP2101 PWRON, not an ESP32 GPIO. INTSTS2 bits 3/2
    // latch short/long presses and are write-one-to-clear. Preserve rail,
    // charging and hardware power-off configuration.
    uint8_t id = 0, enabled = 0;
    if (readRegisters(0x34, 0x03, &id, 1) && (id & 0xCF) == 0x4A &&
        readRegisters(0x34, 0x41, &enabled, 1)) {
        pmicReady = writeRegister(0x34, 0x49, 0x0C) &&
                    writeRegister(0x34, 0x41, enabled | 0x0C);
    }
    launcherConsolePrintf("AXP2101 PWR input=%d\n", pmicReady);
}

void _post_setup_gpio() {}
int getBattery() { return 0; }

void _setBrightness(uint8_t brightval) {
    if (brightval == 0) { analogWrite(TFT_BL, 0); return; }
    int bl = MINBRIGHT + round(((255 - MINBRIGHT) * brightval / 100.0));
    analogWrite(TFT_BL, bl);
}

void InputHandler() {
    const uint32_t now = launcherMillis();
    bool pressed = false;
    Point point{0, 0};
    static uint32_t lastValidTouch = 0;
    if (readTouch(pressed, point)) {
        lastValidTouch = now;
        bool wakeOnly = pressed && !touchInput.down() && wakeUpScreen();
        if (pressed) wakeUpScreen();
        emitAction(touchInput.update(now, pressed, point, wakeOnly));
    } else if (!touchInput.down()) {
        // A confirmed release can finish its double-tap timer while the
        // controller sleeps. Only an active contact needs cancellation.
        emitAction(touchInput.update(now, false));
    } else if (uint32_t(now - lastValidTouch) > 100) {
        touchInput.cancel(); // discard incomplete gestures, don't invent a tap
    }

    bool bootDown = bootDebounce.update(now, launcherGpioRead(SEL_BTN) == LOW);
    bool wakeOnly = bootDown && !bootInput.down() && wakeUpScreen();
    if (bootDown) wakeUpScreen();
    emitAction(bootInput.update(now, bootDown, {0, 0}, wakeOnly));

    static uint32_t lastPmicPoll = 0;
    if (pmicReady && uint32_t(now - lastPmicPoll) >= 30) {
        lastPmicPoll = now;
        uint8_t status = 0;
        if (readRegisters(0x34, 0x49, &status, 1) && (status & 0x0C) &&
            writeRegister(0x34, 0x49, status & 0x0C)) {
            touchInput.cancel();
            bootInput.cancel();
            if (!wakeUpScreen()) emitAction(status & 0x04 ? Action::Back : Action::Select);
        }
    }
    // Never set legacy LongPress: this board emits completed gestures.
}

// Sample every 10 ms even while a UI event awaits consumption. The common
// task slows to 75 ms after AnyKeyPress and can miss quick taps.
void taskInputHandler(void *parameter) {
    uint32_t publishedAt = 0;
    while (true) {
        const uint32_t now = launcherMillis();
        checkPowerSaveTime();
        if (!AnyKeyPress || uint32_t(now - publishedAt) >= 150) resetGlobals();
        bool wasPending = AnyKeyPress;
        InputHandler();
        if (AnyKeyPress && !wasPending) publishedAt = launcherMillis();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
