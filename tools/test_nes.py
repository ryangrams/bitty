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
    objects = []
    for path in sorted(source.rglob("*.c")):
        target = output / (path.stem + ".o")
        subprocess.run([cc, "-std=gnu11", *common, "-c", str(path), "-o", str(target)], check=True)
        objects.append(str(target))
    executable = output / "nes_runtime_test"
    subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", *common,
                    str(ROOT / "tests" / "nes_runtime_test.cpp"), *objects,
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
