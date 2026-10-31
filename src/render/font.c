#include "base/base.h"
#include "platform/platform.h"
#include "render/font.h"

#define FONT_ATLAS_W (FONT_ATLAS_COLS * FONT_GLYPH_SIZE)
#define FONT_ATLAS_H (FONT_ATLAS_ROWS * FONT_GLYPH_SIZE)

// 8x8 glyphs, one byte per row, top bit = leftmost pixel.
// only the glyphs we use so far. everything else is blank.
static const u8 render_font_glyphs[128][8] = {
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['r'] = {0x00, 0x00, 0x6E, 0x73, 0x60, 0x60, 0x60, 0x00},
    ['d'] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
};

static u8 render_font_atlas[FONT_ATLAS_W * FONT_ATLAS_H]; // 1 byte per pixel, 255 = ink

static void render_font_build_atlas(void) {
    for (i32 c = 0; c < 128; c++) {
        i32 gx = (c % FONT_ATLAS_COLS) * FONT_GLYPH_SIZE;
        i32 gy = (c / FONT_ATLAS_COLS) * FONT_GLYPH_SIZE;
        for (i32 row = 0; row < FONT_GLYPH_SIZE; row++) {
            for (i32 col = 0; col < FONT_GLYPH_SIZE; col++) {
                i32 on = (render_font_glyphs[c][row] >> (7 - col)) & 1;
                render_font_atlas[(gy + row) * FONT_ATLAS_W + gx + col] = on ? 255 : 0;
            }
        }
    }
}

GpuHandle render_font_create_texture(void) {
    render_font_build_atlas();
    GpuHandle texture = plat_gpu_create_texture(
        FONT_ATLAS_W, FONT_ATLAS_H, GPU_FORMAT_R8, GPU_TEXTURE_BINDING | GPU_TEXTURE_COPY_DST);
    plat_gpu_write_texture(texture, render_font_atlas, FONT_ATLAS_W, FONT_ATLAS_H, FONT_ATLAS_W);
    return texture;
}
