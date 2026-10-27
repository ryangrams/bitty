#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
/* Include the generated core to compare its internal opacity and pixel paths. */
#include "nes.c"

int nes_draw(int a, int b, int c, int d, nes_color_t* pixels) {
    (void)a; (void)b; (void)c; (void)d; (void)pixels; return 0;
}
int nes_sound_output(uint8_t* samples, size_t count) {
    (void)samples; (void)count; return 0;
}
void nes_frame(nes_t* nes) { (void)nes; }
int nes_initex(nes_t* nes) { (void)nes; return 0; }
int nes_deinitex(nes_t* nes) { (void)nes; return 0; }

static uint32_t random_state = 0x56494445u;
static uint32_t next_random(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}
static unsigned fetch_count;
static void mapper_fetch(nes_t* nes, uint16_t address) {
    (void)nes; (void)address; ++fetch_count;
}

static void invalidate_selection(nes_t* nes) {
#if NES_SPRITE_CACHE
    nes->host_sprite_cache_valid = 0;
#else
    (void)nes;
#endif
}

static void check_sprite_selection(nes_t* nes, unsigned scanline) {
    sprite_line_t actual;
    unsigned count = 0;
    unsigned overflow = 0;
    const unsigned height = nes->nes_ppu.CTRL_H ? 16u : 8u;
    nes->nes_ppu.STATUS_O = 0;
    nes_prepare_sprite_line(nes, scanline, &actual);
    for (unsigned i = 0; i < 64; ++i) {
        const sprite_info_t sprite = nes->nes_ppu.sprite_info[i];
        const unsigned top = (unsigned)sprite.y + 1u;
        if (sprite.y >= 239u || scanline < top || scanline >= top + height) continue;
        if (count == 8u) { overflow = 1; break; }
        unsigned dy = scanline - top;
        if (sprite.flip_v) dy = height - 1u - dy;
        const unsigned base = height == 16u ? (sprite.tile_index_number & 1u) * 4u :
                              (nes->nes_ppu.CTRL_S ? 4u : 0u);
        const unsigned tile = height == 16u ? (sprite.tile_index_number & 254u) + dy / 8u :
                              sprite.tile_index_number;
        const uint8_t* pattern = nes->nes_ppu.pattern_table[base + tile / 64u] +
                                 (tile % 64u) * 16u + dy % 8u;
        assert(actual.sprite[count].sprite_id == i);
        assert(memcmp(&actual.sprite[count].sprite_info, &sprite, sizeof(sprite)) == 0);
        assert(actual.sprite[count].bit0 == pattern[0]);
        assert(actual.sprite[count].bit1 == pattern[8]);
        ++count;
    }
    assert(actual.sprite_numbers == count);
    assert(nes->nes_ppu.STATUS_O == overflow);
}

static unsigned check_sprite_cache(nes_t* nes) {
    unsigned cases = 0;
    for (unsigned trial = 0; trial < 256; ++trial) {
        for (unsigned i = 0; i < sizeof(nes->nes_ppu.oam_data); ++i)
            nes->nes_ppu.oam_data[i] = next_random() >> 24;
        invalidate_selection(nes);
        for (unsigned height = 0; height < 2; ++height) {
            /* Changing height must rebuild a valid cache without an OAM write. */
            nes->nes_ppu.CTRL_H = height;
            nes->nes_ppu.CTRL_S = trial & 1u;
            for (unsigned row = 0; row < 240; ++row) {
                check_sprite_selection(nes, row);
                ++cases;
            }
        }
        /* OAMDATA writes can replace sprite Y in the middle of a frame. */
        nes->nes_ppu.oam_addr = (trial & 63u) * 4u;
        nes_write_ppu_register(nes, 0x2004, trial);
        for (unsigned row = 0; row < 240; ++row) {
            check_sprite_selection(nes, row);
            ++cases;
        }
    }
    /* Hooked mappers retain the original scan and pattern fetch ordering. */
    nes->nes_mapper.mapper_ppu = mapper_fetch;
    nes->nes_mapper.mapper_ppu_tile_min = 0;
    nes->nes_mapper.mapper_ppu_tile_max = 255;
    for (unsigned row = 0; row < 240; ++row) {
        fetch_count = 0;
        check_sprite_selection(nes, row);
        unsigned count = 0;
        for (unsigned i = 0; i < 64; ++i) {
            const unsigned top = (unsigned)nes->nes_ppu.sprite_info[i].y + 1u;
            if (top <= 239u && row >= top && row < top + 16u && count < 8u) ++count;
        }
        assert(fetch_count == count);
        ++cases;
    }
    nes->nes_mapper.mapper_ppu = NULL;
    /* A real STA $4014 invalidates selection before wrapped OAM DMA. */
    for (unsigned i = 0; i < 256; ++i)
        nes->nes_cpu.cpu_ram[0x200u + i] = next_random() >> 24;
    nes->nes_cpu.cpu_ram[0] = 0x8D;
    nes->nes_cpu.cpu_ram[1] = 0x14;
    nes->nes_cpu.cpu_ram[2] = 0x40;
    nes->nes_cpu.PC = 0;
    nes->nes_cpu.cycles = 0;
    nes->nes_cpu.A = 2;
    nes->nes_ppu.oam_addr = 252;
    check_sprite_selection(nes, 120);
    nes_opcode(nes, 1);
#if NES_SPRITE_CACHE
    assert(!nes->host_sprite_cache_valid);
#endif
    for (unsigned i = 0; i < 256; ++i)
        assert(nes->nes_ppu.oam_data[(252u + i) & 255u] == nes->nes_cpu.cpu_ram[0x200u + i]);
    for (unsigned row = 0; row < 240; ++row) {
        check_sprite_selection(nes, row);
        ++cases;
    }
    return cases;
}

int main(void) {
    nes_t* nes = calloc(1, sizeof(*nes));
    uint8_t pattern[8192];
    nes_color_t output[256];
    uint8_t reference[256];
    for (unsigned i = 0; i < sizeof(pattern); ++i) pattern[i] = next_random() >> 24;
    for (unsigned i = 0; i < 8; ++i) nes->nes_ppu.pattern_table[i] = pattern + i * 1024;
    for (unsigned i = 0; i < 4; ++i) {
        nes->nes_ppu.name_table[i] = nes->nes_ppu.ppu_vram[i];
        for (unsigned j = 0; j < 1024; ++j) nes->nes_ppu.ppu_vram[i][j] = next_random() >> 24;
    }
    unsigned cases = 0;
    for (unsigned fine = 0; fine < 8; ++fine) {
        for (unsigned coarse = 0; coarse < 32; ++coarse) {
            for (unsigned x = 0; x < 256; ++x) {
                const uint32_t flags = next_random();
                nes->nes_ppu.x = fine;
                nes->nes_ppu.v.coarse_x = coarse;
                nes->nes_ppu.v.coarse_y = (flags >> 2) & 31;
                nes->nes_ppu.v.nametable = flags & 3;
                nes->nes_ppu.v.fine_y = (flags >> 7) & 7;
                nes->nes_ppu.CTRL_B = (flags >> 10) & 1;
                nes->nes_ppu.CTRL_H = (flags >> 11) & 1;
                nes->nes_ppu.CTRL_S = (flags >> 12) & 1;
                nes->nes_ppu.MASK_m = (flags >> 13) & 1;
                nes->nes_ppu.MASK_M = (flags >> 14) & 1;
                nes->nes_ppu.MASK_s = 1;
                nes->nes_ppu.MASK_b = 1;
                nes->nes_ppu.STATUS_S = 0;
                memset(nes->nes_ppu.oam_data, 0xff, sizeof(nes->nes_ppu.oam_data));
                sprite_info_t* sprite = &nes->nes_ppu.sprite_info[0];
                sprite->x = x;
                sprite->y = 32;
                sprite->tile_index_number = flags >> 16;
                sprite->attributes = flags >> 24;
                invalidate_selection(nes);
                const uint16_t line = 33 + ((flags >> 15) & (nes->nes_ppu.CTRL_H ? 15 : 7));
                nes_render_background_line(nes, line, output);
                memcpy(reference, nes->nes_ppu.bg_opaque, sizeof(reference));
                sprite_line_t sprites;
                nes_prepare_sprite_line(nes, line, &sprites);
                nes->nes_frame_skip_count = 1;
                nes_render_sprite_line(nes, &sprites, output);
                const uint8_t hit = nes->nes_ppu.STATUS_S;
                nes->nes_ppu.STATUS_S = 0;
                memset(nes->nes_ppu.bg_opaque, 0xA5, 256);
                nes_render_background_opacity_line(nes, line);
                for (unsigned p = x; p < x + 8 && p < 255; ++p)
                    assert(nes->nes_ppu.bg_opaque[p] == reference[p]);
                nes_render_sprite_line(nes, &sprites, output);
                assert(nes->nes_ppu.STATUS_S == hit);
                ++cases;
            }
        }
    }
    /* Side-effecting PPU fetches retain the full tile sequence. */
    nes->nes_mapper.mapper_ppu = mapper_fetch;
    nes->nes_mapper.mapper_ppu_tile_min = 0;
    nes->nes_mapper.mapper_ppu_tile_max = 255;
    fetch_count = 0;
    nes_render_background_line(nes, 33, output);
    const unsigned expected_fetches = fetch_count;
    memcpy(reference, nes->nes_ppu.bg_opaque, sizeof(reference));
    fetch_count = 0;
    memset(nes->nes_ppu.bg_opaque, 0xA5, 256);
    nes_render_background_opacity_line(nes, 33);
    assert(fetch_count == expected_fetches && fetch_count >= 32);
    for (unsigned p = 0; p < 256; ++p)
        assert((nes->nes_ppu.bg_opaque[p] != 0) == (reference[p] != 0));
    nes->nes_mapper.mapper_ppu = NULL;
    memset(nes->nes_ppu.oam_data, 0xff, sizeof(nes->nes_ppu.oam_data));
    invalidate_selection(nes);
    for (unsigned i = 0; i < 9; ++i) nes->nes_ppu.sprite_info[i].y = 32;
    sprite_line_t sprites;
    nes->nes_ppu.STATUS_O = 0;
    nes_prepare_sprite_line(nes, 33, &sprites);
    assert(nes->nes_ppu.STATUS_O && sprites.sprite_numbers == 8);
    const unsigned sprite_cases = check_sprite_cache(nes);
    free(nes);
    printf("Skipped-video opacity: %u scroll/sprite cases, mapper fetches and overflow passed\n", cases);
    printf("Sprite selection: %u scanline cases, OAM writes, DMA and height changes passed\n", sprite_cases);
    return 0;
}
