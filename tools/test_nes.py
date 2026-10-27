#!/usr/bin/env python3
"""Build and run portable NES checks using a synthetic cartridge program."""
from pathlib import Path
import shutil
import subprocess

from setup_nes import ROOT, prepare


def main():
    library = prepare()
    source = library / "src"
    output = ROOT / ".local" / "tests" / "nes"
    output.mkdir(parents=True, exist_ok=True)
    cc = shutil.which("cc") or shutil.which("gcc")
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cc or not cxx:
        raise SystemExit("Install a C and C++ compiler to run the NES checks")
    common = ["-O2", "-g", "-fsanitize=address,undefined", "-I" + str(source)]
    for divisor in (1, 3):
        case_output = output / ("video-divisor-" + str(divisor))
        case_output.mkdir(parents=True, exist_ok=True)
        case_flags = [*common, "-DNES_VIDEO_DIVISOR=" + str(divisor)]
        objects = []
        for path in sorted(source.rglob("*.c")):
            target = case_output / (path.stem + ".o")
            subprocess.run([cc, "-std=gnu11", *case_flags, "-c", str(path), "-o", str(target)], check=True)
            objects.append(target)
        executable = case_output / "nes_runtime_test"
        subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", *case_flags,
                        str(ROOT / "tests" / "nes_runtime_test.cpp"), *map(str, objects),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        if divisor == 3:
            executable = case_output / "nes_video_skip_test"
            other_objects = [str(path) for path in objects if path.name != "nes.o"]
            subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", *case_flags,
                            str(ROOT / "tests" / "nes_video_skip_test.c"), *other_objects,
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
