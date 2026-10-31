#ifndef FONT_H
#define FONT_H

#include "base/base.h"
#include "platform/platform.h"

#define FONT_GLYPH_SIZE 8
#define FONT_ATLAS_COLS 16
#define FONT_ATLAS_ROWS 8

// builds the atlas and uploads it. returns the texture (1 byte per pixel, coverage)
GpuHandle render_font_create_texture(void);

#endif
