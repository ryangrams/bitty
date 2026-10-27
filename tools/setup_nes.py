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
    'src/nes_mapper/nes_mapper1.c': '428b240400a254001b05c98ed642588f2e1dcafd4541cd32a1a58a305f8f677d',
    'src/nes_mapper/nes_mapper4.c': '1ddafa9d657108c32cf92aef8b3ff6a1a5c9b93460122e72e3b0f00168cd2162',
    'src/nes_mapper/nes_mapper37.c': '8ef174dbe4e4afde768e7def19d1f4b058229f6f6c10572f6cbe188b6f69700a',
    'src/nes_mapper/nes_mapper118.c': 'eb823a5eb1b5a49160c365a0d389815e40f77037e7214453708fd72fcdcd123f',
}

CONFIG = """#pragma once
#define NES_ENABLE_SOUND 1
#define NES_USE_SRAM 0
#ifndef NES_VIDEO_DIVISOR
#ifdef ARDUINO
#define NES_VIDEO_DIVISOR 3
#else
#define NES_VIDEO_DIVISOR 1
#endif
#endif
#if NES_VIDEO_DIVISOR < 1 || NES_VIDEO_DIVISOR > 255
#error "NES_VIDEO_DIVISOR must be between 1 and 255"
#endif
#define NES_FRAME_SKIP (NES_VIDEO_DIVISOR - 1)
#ifndef NES_CPU_O3
#define NES_CPU_O3 1
#endif
#ifndef NES_SPRITE_CACHE
#define NES_SPRITE_CACHE 1
#endif
#ifndef NES_IRAM_FRAME
#if defined(ARDUINO_ARCH_ESP32) && defined(__XTENSA__)
#define NES_IRAM_FRAME 1
#else
#define NES_IRAM_FRAME 0
#endif
#endif
#define NES_COLOR_DEPTH 16
#define NES_COLOR_SWAP 0
#define NES_RAM_LACK 1
#ifndef NES_VIDEO_ROWS
#define NES_VIDEO_ROWS 8
#endif
#if NES_VIDEO_ROWS < 1 || NES_VIDEO_ROWS > 240 || (240 % NES_VIDEO_ROWS) != 0
#error "NES_VIDEO_ROWS must be a positive divisor of 240"
#endif
#define NES_USE_FS 0
#define NES_ENABLE_HEAVY_MAPPERS 0
#define NES_ENABLE_PLANE1_MAPPERS 0
#define NES_ENABLE_PLANE2_MAPPERS 0
#define NES_ENABLE_EXPANSION_AUDIO 0
#define NES_LOG_LEVEL NES_LOG_LEVEL_NONE
#define nes_log_printf(...) ((void)0)
"""

SKIPPED_SPRITE0_OPACITY = """/* Without mapper PPU fetch side effects, only sprite 0 can consume opacity
 * during a skipped frame. Evaluate its eight current pixels and retain the
 * original tile walk below for mappers that need every pattern fetch. */
static int nes_skipped_sprite0_opacity(nes_t* nes, uint16_t scanline) {
    if (nes->nes_mapper.mapper_ppu || nes->nes_mapper.mapper_exram) return 0;
    const sprite_info_t* sprite = &nes->nes_ppu.sprite_info[0];
    const uint16_t top = (uint16_t)sprite->y + 1u;
    const uint8_t height = nes->nes_ppu.CTRL_H ? 16u : 8u;
    if (!nes->nes_ppu.MASK_s || nes->nes_ppu.STATUS_S || sprite->y >= 0xEFu ||
        scanline < top || scanline >= top + height) return 1;
    const uint16_t row = (uint16_t)nes->nes_ppu.v.coarse_y << 5u;
    const uint8_t dy = nes->nes_ppu.v.fine_y;
    const uint8_t base = nes->nes_ppu.CTRL_B ? 4u : 0u;
    uint16_t screen_x = sprite->x;
    for (uint8_t i = 0; i < 8u && screen_x < 255u; ++i, ++screen_x) {
        if (screen_x < 8u && !nes->nes_ppu.MASK_m) {
            nes->nes_ppu.bg_opaque[screen_x] = 0;
            continue;
        }
        const uint16_t fine_x = screen_x + nes->nes_ppu.x;
        const uint16_t tile_x = nes->nes_ppu.v.coarse_x + (fine_x >> 3u);
        const uint8_t nt = nes->nes_ppu.v.nametable ^ ((tile_x >> 5u) & 1u);
        const uint8_t pattern = nes->nes_ppu.name_table[nt][row + (tile_x & 31u)];
        const uint8_t* tile = nes->nes_ppu.pattern_table[base + (pattern >> 6u)] +
                              ((uint16_t)(pattern & 63u) << 4u);
        const uint8_t occupied = tile[dy] | tile[dy + 8u];
        nes->nes_ppu.bg_opaque[screen_x] = (occupied >> (7u - (fine_x & 7u))) & 1u;
    }
    return 1;
}

"""

SPRITE_SELECTION_CACHE = """#if NES_SPRITE_CACHE
/* OAM writes invalidate this selection table. Pattern fetches still run once
 * per selected sprite on every scanline, including frames without video. */
static void nes_cache_sprite_selection(nes_t* nes, uint8_t height) {
    nes_memset(nes->host_sprite_count, 0, sizeof(nes->host_sprite_count));
    for (uint8_t i = 0; i < 64u; ++i) {
        const uint8_t y = nes->nes_ppu.sprite_info[i].y;
        if (y >= 0xEFu) continue;
        const uint16_t top = (uint16_t)y + 1u;
        const uint16_t end = top + height < NES_HEIGHT ? top + height : NES_HEIGHT;
        for (uint16_t row = top; row < end; ++row) {
            const uint8_t count = nes->host_sprite_count[row];
            if (count < 8u) nes->host_sprite_indices[row][count] = i;
            if (count < 9u) nes->host_sprite_count[row] = count + 1u;
        }
    }
    nes->host_sprite_cache_height = height;
    nes->host_sprite_cache_valid = 1;
}
#endif

"""


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise RuntimeError("Pinned core patch did not match exactly once: " + before[:80])
    return source.replace(before, after, 1)


def patch(relative, text):
    changed = False
    if relative == "inc/nes.h":
        text = replace_once(text, "    uint8_t nes_quit;", """    uint8_t nes_quit;
    uint8_t host_dot_remainder;
#if NES_SPRITE_CACHE
    uint8_t host_sprite_cache_valid;
    uint8_t host_sprite_cache_height;
    uint8_t host_sprite_count[NES_HEIGHT];
    uint8_t host_sprite_indices[NES_HEIGHT][8];
#endif""")
        text = replace_once(text, "void nes_run(nes_t* nes);", "void nes_run(nes_t* nes);\nvoid nes_run_frame(nes_t* nes);")
        changed = True
    elif relative == "inc/nes_apu.h":
        text = replace_once(text, "#define NES_APU_SAMPLE_RATE         (44100)", "#define NES_APU_SAMPLE_RATE         (24000)")
        changed = True
    elif relative == "inc/nes_default.h":
        text = replace_once(text, "#define NES_DRAW_SIZE           (NES_WIDTH * NES_HEIGHT / 2)",
                            "#define NES_DRAW_SIZE           (NES_WIDTH * NES_VIDEO_ROWS)")
        changed = True
    elif relative == "inc/nes_test.h":
        text = replace_once(text, "#if defined(NES_TEST_MODE) && (NES_TEST_MODE == 1)",
                            "#if (defined(NES_TEST_MODE) && (NES_TEST_MODE == 1)) || "
                            "(defined(NES_TEST_PROFILE) && (NES_TEST_PROFILE == 1))")
        changed = True
    elif relative == "src/nes.c":
        text = replace_once(text, '#include "nes.h"', '''#include "nes.h"
#if defined(ARDUINO_ARCH_ESP32) && defined(__XTENSA__) && NES_IRAM_FRAME
#define NES_FRAME_CODE __attribute__((section(".iram1.nes_video")))
#else
#define NES_FRAME_CODE
#endif''')
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
        text = replace_once(text, "void nes_run(nes_t* nes){", "void NES_FRAME_CODE nes_run_frame(nes_t* nes){")
        text = replace_once(text, "static inline void nes_draw_background_tile(",
                            "static inline void NES_FRAME_CODE nes_draw_background_tile(")
        text = replace_once(text, "    nes_cpu_reset(nes);\n", "")
        text = replace_once(text, "    uint8_t dot_remainder = 0;", "    uint8_t dot_remainder = nes->host_dot_remainder;")
        text = replace_once(text, "    while(!nes->nes_quit){", "    if(!nes->nes_quit){")
        text = replace_once(text, "        nes_frame(nes);", "        nes->host_dot_remainder = dot_remainder;\n        nes_frame(nes);")
        text = replace_once(text, "static void nes_render_background_opacity_line(nes_t* nes){",
                            SKIPPED_SPRITE0_OPACITY +
                            "static void nes_render_background_opacity_line(nes_t* nes, uint16_t scanline){\n"
                            "    if (nes_skipped_sprite0_opacity(nes, scanline)) return;")
        text = replace_once(text, "nes_render_background_opacity_line(nes);",
                            "nes_render_background_opacity_line(nes, nes->scanline);")
        text = replace_once(text, "static void nes_prepare_sprite_line(nes_t* nes,uint16_t scanline,sprite_line_t* sprite_line){",
                            SPRITE_SELECTION_CACHE +
                            "static void nes_prepare_sprite_line(nes_t* nes,uint16_t scanline,sprite_line_t* sprite_line){")
        text = replace_once(text, """    sprite_line->sprite_numbers = 0;

    for (uint8_t i = 0; i < 64; i++){""", """    sprite_line->sprite_numbers = 0;
#if NES_SPRITE_CACHE
    const uint8_t cached = scanline < NES_HEIGHT && !nes->nes_mapper.mapper_ppu &&
                           !nes->nes_mapper.mapper_render_screen;
    if (!cached) nes->host_sprite_cache_valid = 0;
    else if (!nes->host_sprite_cache_valid || nes->host_sprite_cache_height != sprite_size)
        nes_cache_sprite_selection(nes, sprite_size);
    const uint8_t candidate_count = cached ? nes->host_sprite_count[scanline] : 64u;
    for (uint8_t candidate = 0; candidate < candidate_count; ++candidate) {
        if (cached && candidate == 8u) {
            nes->nes_ppu.STATUS_O = 1;
            break;
        }
        const uint8_t i = cached ? nes->host_sprite_indices[scanline][candidate] : candidate;
#else
    for (uint8_t i = 0; i < 64; i++){
#endif""")
        text = replace_once(text, "        nes->nes_ppu.STATUS_O = 0;", """        nes->nes_ppu.STATUS_O = 0;
#if NES_SPRITE_CACHE
        nes->host_sprite_cache_valid = 0;
#endif""")
        if text.count("nes->scanline%(NES_HEIGHT/2) * NES_WIDTH") != 3:
            raise RuntimeError("Pinned NES strip pointer patch did not match")
        text = text.replace("nes->scanline%(NES_HEIGHT/2) * NES_WIDTH",
                            "nes->scanline%NES_VIDEO_ROWS * NES_WIDTH")
        draw_start = text.index("                if (nes->scanline == NES_HEIGHT/2-1){")
        draw_stop = text.index("\n            }\n#endif", draw_start)
        text = text[:draw_start] + """                if ((nes->scanline + 1u) % NES_VIDEO_ROWS == 0u) {
                    NES_PROF_BEGIN(nes, NES_PROF_DRAW);
                    nes_draw(0, nes->scanline + 1u - NES_VIDEO_ROWS,
                             NES_WIDTH - 1, nes->scanline, nes->nes_draw_data);
                    NES_PROF_END(nes, NES_PROF_DRAW);
                }""" + text[draw_stop:]
        changed = True
    elif relative == "src/nes_mapper.c":
        text, removed = re.subn(r"^\s*NES_CASE_LOAD_MAPPER\((?!0\)|1\)|4\)|37\)|118\))\d+\);[^\n]*", "", text, flags=re.MULTILINE)
        if removed < 100:
            raise RuntimeError("Pinned mapper registry patch did not match")
        changed = True
    elif relative == "src/nes_cpu.c":
        text = replace_once(text, '#include "nes.h"', '''#include "nes.h"
/* The ESP32 SDK disables switch tables globally for IRAM-safe driver code.
 * This interpreter executes from flash and benefits from direct opcode dispatch. */
#if defined(__XTENSA__) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("jump-tables", "tree-switch-conversion")
#if NES_CPU_O3
#pragma GCC optimize ("O3")
#endif
#endif''')
        text = replace_once(text, "            else if (address == 0x4014){", """            else if (address == 0x4014){
#if NES_SPRITE_CACHE
                nes->host_sprite_cache_valid = 0;
#endif""")
        changed = True
    elif relative == "src/nes_ppu.c":
        text = replace_once(text, "            nes->nes_ppu.oam_data[nes->nes_ppu.oam_addr++] = data;", """#if NES_SPRITE_CACHE
            nes->host_sprite_cache_valid = 0;
#endif
            nes->nes_ppu.oam_data[nes->nes_ppu.oam_addr++] = data;""")
        changed = True
    elif relative == "src/nes_mapper/nes_mapper118.c":
        start = text.index("/* https://www.nesdev.org/wiki/INES_Mapper_118")
        stop = text.index("typedef struct", start)
        text = text[:start] + """/* TxSROM routes CHR A17 to CIRAM A10. The active CHR register for
 * each nametable slot selects one of the two physical CIRAM pages.
 * https://www.nesdev.org/wiki/INES_Mapper_118 */
""" + text[stop:]
        text = replace_once(text, "    uint8_t prg_bank_count;", "    uint16_t prg_bank_count;")
        text = replace_once(text, "    uint8_t chr_bank_count;", "    uint16_t chr_bank_count;")
        text = replace_once(text, "    uint8_t last  = (uint8_t)(m->prg_bank_count - 1u);",
                            "    uint16_t last  = m->prg_bank_count - 1u;")
        text = replace_once(text, "    uint8_t slast = (uint8_t)(m->prg_bank_count - 2u);",
                            "    uint16_t slast = m->prg_bank_count - 2u;")
        start = text.index("    if (m->chr_bank_count == 0u) return;")
        stop = text.index("    if (chr_mode == 0u) {", start)
        text = text[:start] + """    if (nes->nes_rom.four_screen == 0u) {
        for (uint8_t slot = 0; slot < 4u; ++slot) {
            const uint8_t reg = chr_mode ? (uint8_t)(slot + 2u) : (uint8_t)(slot >> 1u);
            const uint8_t page = m->bank_values[reg] >> 7u;
            nes->nes_ppu.name_table[slot] = nes->nes_ppu.ppu_vram[page];
            nes->nes_ppu.name_table_mirrors[slot] = nes->nes_ppu.name_table[slot];
        }
    }
    if (m->chr_bank_count == 0u) return;
    const uint8_t r0 = m->bank_values[0];
    const uint8_t r1 = m->bank_values[1];
""" + text[stop:]
        text = replace_once(text, "m->prg_bank_count = (uint8_t)", "m->prg_bank_count = (uint16_t)")
        text = replace_once(text, "m->chr_bank_count = (uint8_t)", "m->chr_bank_count = (uint16_t)")
        text = replace_once(text, "    mapper118_update_banks(nes);\n}", """    if (nes->nes_rom.sram == NULL) {
        nes->nes_rom.sram = (uint8_t*)nes_malloc(SRAM_SIZE);
        if (nes->nes_rom.sram != NULL) nes_memset(nes->nes_rom.sram, 0, SRAM_SIZE);
    }
    mapper118_update_banks(nes);
}""")
        text = replace_once(text, "    nes->nes_mapper.mapper_hsync  = nes_mapper_hsync;",
                            "    nes->nes_mapper.mapper_hsync  = nes_mapper_hsync;\n"
                            "    nes->nes_mapper.mapper_state_reapply = mapper118_update_banks;")
        changed = True
    elif relative == "src/nes_rom.c":
        start = text.index("static int nes_rom_apply_trainer(")
        end = text.index("\n}\n", start) + 3
        text = text[:start] + "#if (NES_USE_FS == 1)\n" + text[start:end] + "#endif\n" + text[end:]
        changed = True
    if changed:
        text = "/* Modified for m5-flappy: cooperative frames, mapper118 CIRAM/SRAM, RGB565 strips, 24 kHz audio. */\n" + text
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
        "paragraph=Mappers 0/1/4/37/118, RGB565 strip video and 24 kHz audio.\n"
        "category=Other\nurl=" + REPOSITORY + "\narchitectures=*\n", encoding="utf-8")
    receipt = {"repository": REPOSITORY, "revision": REVISION, "license": "Apache-2.0",
               "source_sha256": SOURCES, "configuration": {"sound": True, "sample_rate": 24000,
               "color_bits": 16, "video_rows_default": 8,
               "video_divisor_arduino_default": 2, "video_divisor_desktop_default": 1,
               "iram_frame_xtensa_arduino_default": True,
               "sprite_selection_cache_default": True,
               "mappers": [0, 1, 4, 37, 118]},
               "patches": ["cooperative single-frame entry and preserved fractional timing",
                           "mapper 0/1/4/37/118 registry", "24 kHz sample rate", "RGB565 palette derived from upstream RGB888 constants",
                           "file trainer helper guarded for filesystem-free builds",
                           "NES_VIDEO_DIVISOR skips video work while preserving emulated clocks",
                           "skipped-frame opacity limited to sprite 0 when mapper fetches have no side effects",
                           "NES_VIDEO_ROWS bounded RGB565 strip buffer and synchronous video callbacks",
                           "mapper118 per-slot CHR-controlled CIRAM, 16-bit bank counts and 8 KiB SRAM",
                           "profile hook declarations available without full test-mode logging",
                           "Xtensa opcode switch tables enabled for the flash-resident CPU interpreter",
                           "optional NES_IRAM_FRAME places the scanline loop and tile helper in instruction RAM",
                           "optional NES_CPU_O3 optimizes the CPU source independently of the IRAM frame loop",
                           "NES_SPRITE_CACHE stores scanline OAM selection with write, DMA, height and frame invalidation"]}
    (library / "dependency_receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    (library / "NOTICE").write_text(
        "PeakNES uses PeakRacing/nes, Copyright PeakRacing, Apache-2.0.\n"
        "Source: " + REPOSITORY + "/tree/" + REVISION + "\n"
        "Locally modified for cooperative mapper 0/1/4/37/118 frames, RGB565 strips and 24 kHz audio.\n"
        "NES_VIDEO_DIVISOR controls video refresh; CPU, APU, sprite status and mapper timing run every frame.\n",
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
