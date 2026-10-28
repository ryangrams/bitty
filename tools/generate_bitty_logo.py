#!/usr/bin/env python3
"""Rasterize the bitty.games Archivo wordmark at each LCD size."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = None

def wordmark(size):
    font = ImageFont.truetype(str(FONT), size)
    text = "bitty."
    tracking = -.07 * size
    bounds = font.getbbox(text)
    height = bounds[3] - bounds[1]
    positions, x = [], 0
    for char in text:
        positions.append(x)
        x += font.getlength(char) + tracking
    width = int(positions[-1] + font.getbbox(text[-1])[2] + .999)
    image = Image.new("RGB", (width, height), (16,17,22))
    draw = ImageDraw.Draw(image)
    for char, x in zip(text, positions):
        draw.text((round(x), -bounds[1]), char, font=font,
                  fill=(180,160,255) if char == "." else (240,240,243))
    pixels = [((r & 248) << 8) | ((g & 252) << 3) | (b >> 3) for r,g,b in (image.get_flattened_data() if hasattr(image,"get_flattened_data") else image.getdata())]
    return width, height, pixels

def generate(font):
    global FONT
    FONT = font
    # Seven coverage levels per ink retain the website's two foreground colors.
    background = (16,17,22)
    palette = [background] + [tuple(round(a+(b-a)*level/7) for a,b in zip(background,ink))
        for ink in [(240,240,243),(180,160,255)] for level in range(1,8)]
    values = [((r & 248) << 8) | ((g & 252) << 3) | (b >> 3) for r,g,b in palette]
    lines = ['#pragma once', '#include <stdint.h>', '',
             '// Archivo wordmarks from the bitty.games logo. See LICENSES/Archivo-OFL-1.1.txt.',
             '// Rebuild with tools/generate_bitty_logo.py. Draw these pixels at native size.',
             'namespace flappy { namespace BittyLogo {',
             'constexpr uint16_t Palette[] = {'+','.join(f'0x{p:04x}' for p in values)+'};']
    for name, size in [('Splash',28),('Header',11)]:
        width, height, pixels = wordmark(size)
        encoded = ''
        for value in pixels:
            rgb = ((value >> 8) & 248, (value >> 3) & 252, (value << 3) & 248)
            index = min(range(len(palette)),key=lambda i: sum((a-b)**2 for a,b in zip(rgb,palette[i])))
            encoded += '0123456789abcde'[index]
        lines += [f'constexpr int {name}Width = {width}, {name}Height = {height};',
                  f'constexpr char {name}Pixels[] =']
        lines += ['  "'+encoded[i:i+width]+'"' for i in range(0,len(encoded),width)]
        lines += [';']
    lines += ['} } // namespace flappy::BittyLogo', '']
    (ROOT/'firmware/flappy/bitty_logo.h').write_text('\n'.join(lines))

if __name__ == '__main__':
    import argparse, hashlib, tempfile, urllib.request
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font', type=Path, help='Local copy of the website Archivo WOFF2')
    args = parser.parse_args()
    expected = 'e89dde42d213d6b19102b6b5ad72a12f27f1d36feb666678ef1802f66774beeb'
    if args.font:
        data = args.font.read_bytes()
    else:
        with urllib.request.urlopen('https://raw.githubusercontent.com/ryangrams/bitty-website/7517263e1b7a8836485be5bf1aead9660165ac06/assets/fonts/archivo.woff2',timeout=30) as response:
            data = response.read(256*1024)
    if hashlib.sha256(data).hexdigest() != expected:
        raise SystemExit('The Archivo font differs from the reviewed website font.')
    with tempfile.NamedTemporaryFile(suffix='.woff2') as temporary:
        temporary.write(data); temporary.flush()
        generate(Path(temporary.name))
