#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef M5FLAP_USB_LIBRARY
// Views borrow mapped flash. Unload the emulator before changing or erasing it.
namespace FlappyRom { static const uint8_t* data = nullptr; static size_t length = 0; static const char* title = "NES"; }
namespace TetrisRom { static const uint8_t* data = nullptr; static size_t length = 0; static const char* title = "Tetris"; }
namespace TetrisZeroRom { static const uint8_t* data = nullptr; static size_t length = 0; static const char* title = "Tetris Zero"; }
namespace GeometryRom { static const uint8_t* data = nullptr; static size_t length = 0; static const char* title = "Geometry Dash"; }
#else
#include "generated_rom.h"
#include "generated_tetris_rom.h"
#include "generated_zero_rom.h"
#include "generated_geometry_rom.h"
#endif
