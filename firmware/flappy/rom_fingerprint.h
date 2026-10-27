#pragma once

#include <stddef.h>
#include <stdint.h>

namespace flappy {
namespace RomFingerprint {

inline uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc ^ 0xffffffffu;
}

inline bool tetris(const uint8_t* data, size_t length, bool zero) {
  const size_t expectedLength = zero ? 81936u : 49168u;
  const uint32_t expectedCrc = zero ? 0x66462642u : 0x37a5ec63u;
  return data && length == expectedLength && crc32(data, length) == expectedCrc;
}

}  // namespace RomFingerprint
}  // namespace flappy
