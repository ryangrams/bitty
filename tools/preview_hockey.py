#!/usr/bin/env python3
"""Render native Hockey Queen frames and comparison sheets from portable source."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
NAMES = ("Rainbow Court", "Unicorn Road", "Narwhal Ice", "Jellyfish Reef",
         "Butterfly Meadow", "Candy Castle", "Potato Patch", "Throne Room")


def sheet(frames, columns, destination):
    pad, gap, label_height = 24, 24, 32
    rows = (len(frames) + columns - 1) // columns
    image = Image.new("RGB", (columns * 270 + (columns - 1) * gap + pad * 2,
                              rows * 512 + (rows - 1) * gap + pad * 2), (16, 17, 22))
    font = ImageFont.load_default(size=18)
    draw = ImageDraw.Draw(image)
    for i, (name, frame) in enumerate(frames):
        x, y = pad + (i % columns) * (270 + gap), pad + (i // columns) * (512 + gap)
        draw.text((x, y), name, fill=(232, 235, 242), font=font)
        image.paste(frame.resize((270, 480), Image.Resampling.NEAREST), (x, y + label_height))
    image.save(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".local/hockey-queen/preview")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which("c++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("A C++11 compiler is required")
    with tempfile.TemporaryDirectory(dir=output) as tmp:
        temporary = Path(tmp)
        executable = temporary / "render-hockey"
        subprocess.run([compiler, "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests/hockey_game_test.cpp"), "-o", str(executable)], check=True)
        subprocess.run([str(executable), str(temporary / "frame")], check=True)
        frames = {}
        for path in temporary.glob("*.ppm"):
            name = path.stem.removeprefix("frame-")
            with Image.open(path) as source:
                frame = source.convert("RGB")
            assert frame.size == (135, 240)
            frame.save(output / (name + ".png"))
            frames[name] = frame
        sheet([(name, frames[f"map-{i}"]) for i, name in enumerate(NAMES)], 4, output / "maps.png")
        sheet([("Queen stick", frames["map-1"]), ("Peasant stick", frames["guest-1"]),
               ("Clock home", frames["clock"])], 3, output / "two-sticks-and-clock.png")
    print("Host-rendered frames saved in", output)


if __name__ == "__main__":
    main()
