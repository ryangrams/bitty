#!/usr/bin/env python3
"""Generate project-created starter art for a distributable firmware build."""
from pathlib import Path
import re
from PIL import Image, ImageDraw
from generate_assets import sprite_header, collision_header

ROOT = Path(__file__).resolve().parents[1]


def generate(output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    font = (ROOT / 'firmware/flappy/menu_font.h').read_text()
    rows = [list(map(int, row.split(','))) for row in re.findall(r'\{([0-9,]+)\},', font)]
    if len(rows) != 95 or any(len(row) != 12 for row in rows):
        raise ValueError('Reviewed menu font format changed')
    clear = (0, 0, 0, 0)
    ink, white, gold, cyan = '#162338', '#eaf5e9', '#f6c75b', '#56ccc2'

    def text(image, words, x, y, color=white):
        draw = ImageDraw.Draw(image)
        for i, char in enumerate(words):
            for sy, row in enumerate(rows[ord(char) - 32]):
                for sx in range(6):
                    if row & (1 << (5 - sx)):
                        draw.point((x + i * 6 + sx, y + sy), fill=color)

    assets = []

    def add(name, size, painter):
        image = Image.new('RGBA', size, clear)
        painter(image, ImageDraw.Draw(image))
        assets.append((name, image.resize((size[0] * 2, size[1] * 2), Image.Resampling.NEAREST)))

    def background(image, draw):
        draw.rectangle((0, 0, 143, 255), fill='#223550')
        for y in range(0, 256, 16):
            for x in range(0, 144, 16):
                if (x // 16 + y // 16) % 3 == 0:
                    draw.rectangle((x + 1, y + 1, x + 14, y + 14), outline='#293e58')
        for x in range(7, 144, 23):
            draw.rectangle((x, 182 - x % 19, x + 14, 255), fill='#304e62')

    add('background', (144, 256), background)
    add('ground', (168, 56), lambda im, d: (d.rectangle((0, 0, 167, 55), fill=ink), d.rectangle((0, 0, 167, 3), fill=cyan)))
    for name, top in [('pipe', False), ('pipeTop', True)]:
        def pillar(im, d, top=top):
            d.rectangle((3, 0, 22, 159), fill='#3e6676', outline=ink)
            d.rectangle((6, 0, 8, 159), fill='#6e9c9b')
            y = 153 if top else 0
            d.rectangle((0, y, 25, y + 6), fill=gold, outline=ink)
        add(name, (26, 160), pillar)
    for frame, name in enumerate(('birdUp', 'birdMid', 'birdDown')):
        def flyer(im, d, frame=frame):
            d.rectangle((3, 3, 13, 10), fill=cyan, outline=ink)
            d.rectangle((5, 1, 12, 5), fill=white, outline=ink)
            d.rectangle((10, 2, 11, 3), fill=ink)
            d.rectangle((13, 6, 16, 8), fill=gold)
            d.rectangle((0, 4 + frame, 4, 6 + frame), fill=white, outline=ink)
            d.point((1, 8 + frame), fill=gold)
        add(name, (17, 12), flyer)
    for name, size, words in [('title', (89, 24), 'BITTY FLAP'), ('getReady', (98, 31), 'READY?'),
                              ('tapInstruction', (57, 49), 'TAP M5'), ('gameOver', (102, 27), 'GAME OVER'),
                              ('replay', (58, 35), 'PLAY'), ('newBest', (16, 7), 'NEW')]:
        def label(im, d, words=words, size=size):
            d.rectangle((0, 0, size[0] - 1, size[1] - 1), fill=ink)
            text(im, words, max(0, (size[0] - len(words) * 6) // 2), max(-2, (size[1] - 12) // 2), gold)
        add(name, size, label)
    def scoreboard(im, d):
        d.rectangle((0, 0, 118, 62), fill=ink, outline=cyan)
        text(im, 'SCORE', 65, 3); text(im, 'BEST', 71, 29)
    add('scoreboard', (119, 63), scoreboard)
    for prefix, size in [('digit', (12, 18)), ('smallDigit', (6, 7)), ('panelDigit', (8, 10))]:
        for digit in range(10):
            def number(im, d, digit=digit, size=size):
                glyph = Image.new('RGBA', (6, 12), clear)
                text(glyph, str(digit), 0, 0, gold)
                glyph = glyph.crop((0, 2, 6, 10))
                if size == (12, 18):
                    glyph = glyph.resize((12, 16), Image.Resampling.NEAREST)
                im.alpha_composite(glyph, (0, max(0, (size[1] - glyph.height) // 2)))
            add(prefix + str(digit), size, number)
    for i, color in enumerate((cyan, white, gold, '#a26d54')):
        add('medal' + str(i), (22, 22), lambda im, d, color=color: d.ellipse((1, 1, 20, 20), fill=color, outline=ink))
    (output / 'generated_assets.h').write_text(sprite_header(assets))
    (output / 'generated_collision.h').write_text(collision_header(assets))
    (output / 'generated_sounds.h').write_text('''#pragma once
#include <stdint.h>
namespace FlappySounds {
struct Sound { const int16_t* samples; uint32_t length; uint32_t sampleRate; };
static const Sound wing = {nullptr,0,22050}, hit = {nullptr,0,22050}, die = {nullptr,0,22050}, point = {nullptr,0,22050};
}
''')
    return output


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    generate(parser.parse_args().output)
