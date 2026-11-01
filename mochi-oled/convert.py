"""Convert the author's GIF samples to 128x64 OLED MCH1 animations."""

from pathlib import Path
import struct

from PIL import Image


SOURCE = Path(__file__).resolve().parent.parent / "mochi-source" / "some gif (not enough)"
OUTPUT = Path(__file__).resolve().parent / "data"


def packbits(data: bytes) -> bytes:
    result = bytearray()
    i = 0
    while i < len(data):
        run = 1
        while i + run < len(data) and data[i + run] == data[i] and run < 128:
            run += 1
        if run >= 3:
            result.extend((257 - run, data[i]))
            i += run
            continue
        start = i
        i += run
        while i < len(data) and i - start < 128:
            run = 1
            while i + run < len(data) and data[i + run] == data[i] and run < 128:
                run += 1
            if run >= 3 or i - start + run > 128:
                break
            i += run
        result.append(i - start - 1)
        result.extend(data[start:i])
    return bytes(result)


def frame_bytes(frame: Image.Image) -> bytes:
    # The source is 480x304; center-cropping to 480x240 fits 128x64 exactly.
    image = frame.convert("RGB").crop((0, 32, 480, 272)).resize(
        (128, 64), Image.Resampling.LANCZOS
    ).convert("L")
    pixels = image.load()
    result = bytearray(1024)
    for y in range(64):
        for x in range(128):
            if pixels[x, y] >= 96:
                result[(y // 8) * 128 + x] |= 1 << (y % 8)
    return bytes(result)


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for source in sorted(SOURCE.glob("*.gif")):
        gif = Image.open(source)
        destination = OUTPUT / f"{source.stem}.mch"
        with destination.open("wb") as out:
            out.write(struct.pack("<4sHHH", b"MCH1", 128, 64, gif.n_frames))
            for index in range(gif.n_frames):
                gif.seek(index)
                compressed = packbits(frame_bytes(gif))
                duration = max(20, gif.info.get("duration", 70))
                out.write(struct.pack("<HH", duration, len(compressed)))
                out.write(compressed)
        print(f"{destination.name}: {gif.n_frames} frames, {destination.stat().st_size} bytes")


if __name__ == "__main__":
    main()
