"""Fail CI unless the single image contains the exact build outputs."""
import hashlib
from pathlib import Path

env = "waveshare-esp32-s3-touch-lcd-183"
merged = Path(f"Launcher-{env}.bin")
data = merged.read_bytes()
build = Path(".pio/build") / env
for name, offset in (("bootloader.bin", 0), ("partitions.bin", 0x8000), ("firmware.bin", 0x10000)):
    part = (build / name).read_bytes()
    assert part and data[offset:offset + len(part)] == part, f"Invalid merged {name}"
assert data[0] == 0xE9 and data[0x10000] == 0xE9, "Invalid ESP image header"
assert data[0x8000:0x8002] == b"\xaa\x50", "Invalid partition table"
assert len(data) <= 0x190000, "Merged image exceeds the Launcher test partition"
digest = hashlib.sha256(data).hexdigest()
merged.with_suffix(".bin.sha256").write_text(f"{digest}  {merged.name}\n")
print(f"Verified {merged.name}: {len(data)} bytes, SHA256 {digest}")
