#!/usr/bin/env python3
"""Fetch and prepare the pinned NES core in the private dependency directory."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REVISION = "72326d82d642c78717a111629c8e04dbae39e8d0"
REPOSITORY = "https://github.com/PeakRacing/nes"
DEFAULT_ROOT = ROOT / ".local" / "nes-deps"
SOURCES = {
    'LICENSE': 'c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4',
    'inc/nes.h': '0accb52b2352e8a2989009d05818372f303aa50afe7e5af0a0079818019f4082',
    'inc/nes_apu.h': '50c04f0b08ee997f28384674405a13e8a8f98e7fd61b28b688592b397896601d',
    'inc/nes_cpu.h': 'aec2dc4bd2e1f9f732c634d18f71241378262f7522bf8bce614285b4fda52ce2',
    'inc/nes_default.h': '5a22365b6469401d78e1d89568d6642dead1fae01774b8327549de7fd571bb74',
    'inc/nes_log.h': 'ec419e048fded8a55eb5203953444774c52a7db41b0b373bc17cceee13db9687',
    'inc/nes_mapper.h': '6a73867bcef7cc4dfef4c8c5892b0748dc6f1ea9196ab32438b0c1fd5af75313',
    'inc/nes_ppu.h': '37931f2bc7626b3393c6a511cbbd3f07e21a75bafd4d27aaee92e069d1e90655',
    'inc/nes_rom.h': '8c984a47e87346d9df70d31336ff0cc98509d86683519fb280da9ad22c957dd8',
    'inc/nes_state.h': 'fa6e704cfb534bd6e55ccb08c404f57cb9b4a4518428fb81ad154a9a4be58dc2',
    'inc/nes_test.h': 'b196326535a9619e5e0ac15c57ab920ba8b72642b0316e2958bb86b148b5eec7',
    'src/nes.c': '0e29560b6ec949286cb12af52cfcaf6920bf84b27db2a8f21394bc28868dc554',
    'src/nes_apu.c': 'da2d9c3da57df9cf785ff4de6ac1b242177e4c560e7467dd82d71fd927d10516',
    'src/nes_cpu.c': 'b40486abcb6291d2ba3c0700d882283a7b3435e9cee4ff72b8dac011aaf94597',
    'src/nes_default.c': '18b69e330be64f14631aebe5ef8d2b9617b50323794a3a281fbf4dc72781063e',
    'src/nes_mapper.c': '28a74d82becbb07fb95b9ebb18f96f7c0a5d10000c97eca22b49adec93417a32',
    'src/nes_ppu.c': '6d36e4e0872563cb29084a34981e63e7072d2db517fb48a7aeb4d051ce818478',
    'src/nes_rom.c': '6f53d02ca0b518ce9aad895b9076025a3785e9f929f30df07e96c0ff8dec5fe3',
    'src/nes_mapper/nes_mapper0.c': 'b91b1a6310c41a9519f22928d7d7fc4b1199d0f71c2e7b33678ba3a9dc632333',
    'src/nes_mapper/nes_mapper4.c': '1ddafa9d657108c32cf92aef8b3ff6a1a5c9b93460122e72e3b0f00168cd2162',
    'src/nes_mapper/nes_mapper37.c': '8ef174dbe4e4afde768e7def19d1f4b058229f6f6c10572f6cbe188b6f69700a',
}

CONFIG = """#pragma once
#define NES_ENABLE_SOUND 1
#define NES_USE_SRAM 0
#define NES_FRAME_SKIP 0
#define NES_COLOR_DEPTH 16
#define NES_COLOR_SWAP 0
#define NES_RAM_LACK 1
#define NES_USE_FS 0
#define NES_ENABLE_HEAVY_MAPPERS 0
#define NES_ENABLE_PLANE1_MAPPERS 0
#define NES_ENABLE_PLANE2_MAPPERS 0
#define NES_ENABLE_EXPANSION_AUDIO 0
#define NES_LOG_LEVEL NES_LOG_LEVEL_NONE
#define nes_log_printf(...) ((void)0)
"""


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise RuntimeError("Pinned core patch did not match exactly once: " + before[:80])
    return source.replace(before, after, 1)


def patch(relative, text):
    changed = False
    if relative == "inc/nes.h":
        text = replace_once(text, "    uint8_t nes_quit;", "    uint8_t nes_quit;\n    uint8_t host_dot_remainder;")
        text = replace_once(text, "void nes_run(nes_t* nes);", "void nes_run(nes_t* nes);\nvoid nes_run_frame(nes_t* nes);")
        changed = True
    elif relative == "inc/nes_apu.h":
        text = replace_once(text, "#define NES_APU_SAMPLE_RATE         (44100)", "#define NES_APU_SAMPLE_RATE         (24000)")
        changed = True
    elif relative == "src/nes.c":
        rgb_block = text[text.index("#if (NES_COLOR_DEPTH == 32)"):text.index("#elif (NES_COLOR_DEPTH == 16)")]
        colors = [int(value, 16) for value in re.findall(r"0xFF([0-9A-Fa-f]{6})", rgb_block)]
        if len(colors) != 64:
            raise RuntimeError("Pinned NES palette patch did not match")
        rgb565 = [((color >> 19) << 11) | (((color >> 10) & 63) << 5) | ((color >> 3) & 31) for color in colors]
        start = text.index("#if (NES_COLOR_SWAP == 0) // RGB565")
        stop = text.index("#endif /* NES_COLOR_SWAP */", start)
        rows = lambda values: "\n".join("    " + ", ".join(f"0x{value:04X}" for value in values[i:i + 16]) + "," for i in range(0, 64, 16))
        swapped = [((value & 255) << 8) | (value >> 8) for value in rgb565]
        palette = "#if (NES_COLOR_SWAP == 0) // RGB565\n" + rows(rgb565)
        palette += "\n#else // RGB565_SWAP\n" + rows(swapped) + "\n"
        text = text[:start] + palette + text[stop:]
        text = replace_once(text, "void nes_run(nes_t* nes){", "void nes_run_frame(nes_t* nes){")
        text = replace_once(text, "    nes_cpu_reset(nes);\n", "")
        text = replace_once(text, "    uint8_t dot_remainder = 0;", "    uint8_t dot_remainder = nes->host_dot_remainder;")
        text = replace_once(text, "    while(!nes->nes_quit){", "    if(!nes->nes_quit){")
        text = replace_once(text, "        nes_frame(nes);", "        nes->host_dot_remainder = dot_remainder;\n        nes_frame(nes);")
        changed = True
    elif relative == "src/nes_mapper.c":
        text, removed = re.subn(r"^\s*NES_CASE_LOAD_MAPPER\((?!0\)|4\)|37\))\d+\);[^\n]*", "", text, flags=re.MULTILINE)
        if removed < 100:
            raise RuntimeError("Pinned mapper registry patch did not match")
        changed = True
    elif relative == "src/nes_rom.c":
        start = text.index("static int nes_rom_apply_trainer(")
        end = text.index("\n}\n", start) + 3
        text = text[:start] + "#if (NES_USE_FS == 1)\n" + text[start:end] + "#endif\n" + text[end:]
        changed = True
    if changed:
        text = "/* Modified for m5-flappy: mapper 0/4/37 cooperative frames, RGB565 half buffers, 24 kHz audio. */\n" + text
    return text


def prepare(dependency_root=DEFAULT_ROOT, offline=False):
    dependency_root = Path(dependency_root).resolve()
    upstream = dependency_root / ("PeakRacing-" + REVISION)
    library = dependency_root / "PeakNES"

    def fetch(relative):
        path = upstream / relative
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != SOURCES[relative]:
            if offline:
                raise RuntimeError("Missing verified NES dependency: " + str(path))
            url = "https://raw.githubusercontent.com/PeakRacing/nes/" + REVISION + "/" + relative
            with urllib.request.urlopen(url, timeout=45) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != SOURCES[relative]:
                raise RuntimeError("NES dependency checksum mismatch: " + relative)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return relative

    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(fetch, SOURCES))

    (library / "src").mkdir(parents=True, exist_ok=True)
    for relative in SOURCES:
        if relative == "LICENSE":
            (library / "LICENSE").write_bytes((upstream / relative).read_bytes())
            continue
        destination = library / "src" / (relative[4:] if relative.startswith("inc/") else relative[4:])
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(patch(relative, (upstream / relative).read_text(encoding="utf-8")), encoding="utf-8")
    (library / "src" / "nes_conf.h").write_text(CONFIG, encoding="utf-8")
    (library / "library.properties").write_text(
        "name=PeakNES\nversion=1.0.0\nauthor=PeakRacing\nmaintainer=Ryan Grams\n"
        "sentence=Pinned portable NES core with a small memory configuration.\n"
        "paragraph=Mappers 0/4/37, RGB565 video and 24 kHz audio.\n"
        "category=Other\nurl=" + REPOSITORY + "\narchitectures=*\n", encoding="utf-8")
    receipt = {"repository": REPOSITORY, "revision": REVISION, "license": "Apache-2.0",
               "source_sha256": SOURCES, "configuration": {"sound": True, "sample_rate": 24000,
               "color_bits": 16, "half_frame_buffer": True, "mappers": [0, 4, 37]},
               "patches": ["cooperative single-frame entry and preserved fractional timing",
                           "mapper 0/4/37 registry", "24 kHz sample rate", "RGB565 palette derived from upstream RGB888 constants",
                           "file trainer helper guarded for filesystem-free builds"]}
    (library / "dependency_receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    (library / "NOTICE").write_text(
        "PeakNES uses PeakRacing/nes, Copyright PeakRacing, Apache-2.0.\n"
        "Source: " + REPOSITORY + "/tree/" + REVISION + "\n"
        "Locally modified for cooperative mapper 0/4/37 frames, RGB565 half buffers and 24 kHz audio.\n",
        encoding="utf-8")
    return library


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependency-root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--offline", action="store_true", help="Use only verified files already cached")
    args = parser.parse_args()
    print(prepare(args.dependency_root, args.offline))


if __name__ == "__main__":
    main()
