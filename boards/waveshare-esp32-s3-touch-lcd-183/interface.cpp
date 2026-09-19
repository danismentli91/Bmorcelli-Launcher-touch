#include "idf/launcher_platform.h"
#include "powerSave.h"
#include <Arduino.h>
#include <interface.h>
#include <Wire.h>

/*
 * Waveshare ESP32-S3-Touch-LCD-1.83
 * Official pin map: LCD DC=4 CS=5 SCK=6 MOSI=7 RST=38 BL=40,
 * I2C SDA=15 SCL=14, touch RST=39 INT=13, BOOT=GPIO0.
 * Touch gesture support is added in the next pass after CST816 integration.
 */
static bool touchPressed() {
    Wire.beginTransmission(0x15);
    Wire.write(0x02); // FingerNum register
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(0x15, 1) != 1) return false;
    return (Wire.read() & 0x0F) > 0;
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
    launcherGpioInputPullup(SEL_BTN);
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    pinMode(TOUCH_INT, INPUT_PULLUP);
}

void _post_setup_gpio() {}
int getBattery() { return 0; }

void _setBrightness(uint8_t brightval) {
    if (brightval == 0) { analogWrite(TFT_BL, 0); return; }
    int bl = MINBRIGHT + round(((255 - MINBRIGHT) * brightval / 100.0));
    analogWrite(TFT_BL, bl);
}

void InputHandler(void) {
    static unsigned long tm = 0;
    constexpr unsigned long debounceMs = 75;
    if (launcherMillis() - tm < debounceMs && !LongPress) return;
    checkPowerSaveTime();

    static bool wasDown = false;
    static unsigned long downAt = 0;
    static unsigned long lastRelease = 0;
    static bool pendingNext = false;
    static unsigned long pendingAt = 0;
    static uint8_t clicks = 0;
    constexpr unsigned long selectMs = 550;
    constexpr unsigned long backMs = 1200;
    constexpr unsigned long doubleMs = 300;

    if (pendingNext && launcherMillis() - pendingAt > doubleMs) {
        NextPress = true;
        pendingNext = false;
    }

    // Touch: tap=Select, double tap=Back. A touch also wakes a sleeping screen.
    static bool touchWasDown = false;
    static unsigned long lastTap = 0;
    bool touchDown = touchPressed();
    if (touchDown && !touchWasDown) {
        touchWasDown = true;
        AnyKeyPress = true;
        if (wakeUpScreen()) return;
    }
    if (!touchDown && touchWasDown) {
        touchWasDown = false;
        unsigned long now = launcherMillis();
        if (now - lastTap <= doubleMs) { EscPress = true; lastTap = 0; }
        else { SelPress = true; lastTap = now; }
        AnyKeyPress = true;
    }

    bool down = launcherGpioRead(SEL_BTN) == LOW;
    if (down && !wasDown) {
        wasDown = true; downAt = launcherMillis(); tm = launcherMillis();
        AnyKeyPress = true; LongPress = false;
        if (wakeUpScreen()) return;
    }
    if (down) {
        AnyKeyPress = true;
        if (launcherMillis() - downAt >= selectMs) LongPress = true;
        return;
    }
    if (wasDown) {
        wasDown = false;
        unsigned long held = launcherMillis() - downAt;
        if (launcherMillis() - lastRelease > doubleMs) clicks = 0;
        if (held >= backMs) { EscPress = true; pendingNext = false; }
        else if (held >= selectMs) { SelPress = true; pendingNext = false; }
        else {
            clicks++; lastRelease = launcherMillis();
            if (clicks >= 2) { PrevPress = true; clicks = 0; pendingNext = false; }
            else { pendingNext = true; pendingAt = launcherMillis(); }
        }
        AnyKeyPress = true; LongPress = false;
    }
}
