#!/usr/bin/env python3
"""Build and run portable NES checks using a synthetic cartridge program."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess

from setup_nes import DEFAULT_ROOT, ROOT, prepare


def firmware_function(source, signature):
    start = source.index(signature)
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    depth = 0
    for token in tokens.finditer(source, start):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:token.end()] + "\n"
    raise ValueError("The firmware function has no closing brace: " + signature)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependency-root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    library = prepare(args.dependency_root, args.offline)
    source = library / "src"
    output = ROOT / ".local" / "tests" / "nes"
    output.mkdir(parents=True, exist_ok=True)
    firmware = (ROOT / "firmware" / "flappy" / "flappy.ino").read_text()
    (output / "generated_nes_launcher.h").write_text(
        firmware_function(firmware, "void handlePlayGameRequest() {") +
        firmware_function(firmware, "void handleHomePlayRequests(bool coopStart) {") +
        firmware_function(firmware, "void prepareNativeHomePlay() {"))
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
        if divisor == 1:
            executable = case_output / "nes_launcher_test"
            subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror", *case_flags,
                            "-I" + str(output), str(ROOT / "tests" / "nes_launcher_test.cpp"),
                            *map(str, objects), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
        executable = case_output / "nes_timing_test"
        subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", *case_flags,
                        str(ROOT / "tests" / "nes_timing_test.cpp"), *map(str, objects),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        executable = case_output / "nes_prg_cache_test"
        subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", *case_flags,
                        str(ROOT / "tests" / "nes_prg_cache_test.cpp"), *map(str, objects),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        if divisor == 1:
            executable = case_output / "nes_cpu_idle_test"
            subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", *case_flags,
                            str(ROOT / "tests" / "nes_cpu_idle_test.cpp"), *map(str, objects),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
            trace_output = case_output / "tracing"
            trace_output.mkdir(exist_ok=True)
            trace_cpu = trace_output / "nes_cpu.o"
            subprocess.run([cc, "-std=gnu11", *case_flags, "-DNES_TEST_MODE=1", "-c",
                            str(source / "nes_cpu.c"), "-o", str(trace_cpu)], check=True)
            trace_objects = [trace_cpu if path.name == "nes_cpu.o" else path for path in objects]
            executable = trace_output / "nes_cpu_idle_test"
            subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", *case_flags, "-DNES_TEST_MODE=1",
                            str(ROOT / "tests" / "nes_cpu_idle_test.cpp"), *map(str, trace_objects),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
            executable = case_output / "nes_apu_pcm_test"
            subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror", *case_flags,
                            str(ROOT / "tests" / "nes_apu_pcm_test.c"), str(source / "nes_apu.c"),
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
