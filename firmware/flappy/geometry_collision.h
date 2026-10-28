#pragma once
// Adapted from Famidash collision.h, commit 0df680d0e02f0ccb438f1224d162a0ab073b7204.
// Copyright 2023-2024 TFDSoft. SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdint.h>

namespace flappy {

// Point predicates from the pinned upstream collision.h, independent of artwork.
struct GeometryCollisionMask {
  static uint16_t lowBits(int count) { return count >= 16 ? 0xffff : count <= 0 ? 0 : uint16_t((1u << count) - 1); }
  static uint16_t span(int left, int right) {
    if (left < 0) left = 0;
    if (right > 15) right = 15;
    return right < left ? 0 : uint16_t(lowBits(right + 1) & ~lowBits(left));
  }
  static bool fullSolid(uint8_t c) { return c == 7 || c == 9 || c == 0x1f; }

  static uint16_t hazardRow(uint8_t c, int y) {
    if (y < 0 || y > 15) return 0;
    const uint16_t left = y >= 6 && y <= 8 ? 0x003f : 0;
    const uint16_t right = y >= 6 && y <= 8 ? 0xfc00 : 0;
    const uint16_t top = y < 6 ? 0x00e0 : 0;
    const uint16_t bottom = y > 10 ? 0x00e0 : 0;
    switch (c) {
      case 1: return right;
      case 2: return left;
      case 3: return top;
      case 4: case 0x3b: case 0x3d: return bottom;
      case 8: return y >= 4 && y < 12 ? 0x01f0 : 0;
      case 0x28: return y >= 8 ? 0x003c : 0;
      case 0x29: return y >= 8 ? 0x1c00 : 0;
      case 0x2a: return y >= 8 ? 0x3c3c : 0;
      case 0x2b: case 0x2d: case 0x30: return y < 8 ? 0x003c : 0;
      case 0x2c: case 0x2e: case 0x31: return y < 8 ? 0x1c00 : 0;
      case 0x2f: case 0x32: return y < 8 ? 0x3c3c : 0;
      case 0x33: return uint16_t(top | right);
      case 0x34: return uint16_t(top | left);
      case 0x35: return uint16_t(bottom | right);
      case 0x36: return uint16_t(bottom | left);
      case 0x3c: return y < 8 ? 0x0780 : 0;
      default: return 0;
    }
  }

  static uint16_t solidRow(uint8_t c, int y) {
    if (y < 0 || y > 15) return 0;
    switch (c) {
      case 5: case 0x3b: case 0x3d: return y < 8 ? 0xffff : 0;
      case 6: case 0x2d: case 0x2e: case 0x2f: case 0x3c: return y >= 8 ? 0xffff : 0;
      case 7: case 9: case 0x1f: return 0xffff;
      case 0x0b: return uint16_t(0xffffu << (15 - y));
      case 0x0c: return lowBits(y + 1);
      case 0x0d: return y < 8 ? 0 : uint16_t(0xffffu << (2 * (15 - y)));
      case 0x0e: return lowBits(2 * y + 2);
      case 0x0f: return y >= 7 ? 0xffff : uint16_t(0xffffu << (2 * (7 - y)));
      case 0x10: return y < 8 ? 0 : lowBits(2 * (y - 8) + 2);
      case 0x11: return uint16_t(0xffffu << (8 + (16 - y) / 2));
      case 0x12: return lowBits(y / 2 + 1);
      case 0x13: return uint16_t(0xffffu << ((16 - y) / 2));
      case 0x14: return uint16_t(0xff | (lowBits(y / 2 + 1) << 8));
      case 0x15: return uint16_t(0xffffu << y);
      case 0x16: return lowBits(16 - y);
      case 0x17: return solidRow(0x0f, 15 - y);
      case 0x18: return solidRow(0x0d, 15 - y);
      case 0x19: return solidRow(0x10, 15 - y);
      case 0x1a: return solidRow(0x0e, 15 - y);
      case 0x1b: return solidRow(0x11, 15 - y);
      case 0x1c: return solidRow(0x12, 15 - y);
      case 0x1d: return solidRow(0x13, 15 - y);
      case 0x1e: return solidRow(0x14, 15 - y);
      case 0x20: return y < 8 ? 0x00ff : 0;
      case 0x21: return y < 8 ? 0xff00 : 0;
      case 0x22: case 0x2b: return y >= 8 ? 0x00ff : 0;
      case 0x23: case 0x2c: return y >= 8 ? 0xff00 : 0;
      case 0x24: return 0x00ff;
      case 0x25: return 0xff00;
      case 0x26: return y < 8 ? 0x00ff : 0xff00;
      case 0x27: return y < 8 ? 0xff00 : 0x00ff;
      case 0x37: return y < 8 ? 0xff00 : 0xffff;
      case 0x38: return y < 8 ? 0x00ff : 0xffff;
      case 0x39: return y < 8 ? 0xffff : 0xff00;
      case 0x3a: return y < 8 ? 0xffff : 0x00ff;
      default: return 0;
    }
  }
};

}  // namespace flappy
