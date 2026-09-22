#ifndef __FONT_H_
#define __FONT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

#define DEFAULT_FONT_PATH \
    "/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf"

typedef struct Font Font;

struct Font {
    FT_Library ft_lib;
    FT_Face face;
    hb_font_t *hb_font;
};

typedef struct {
    uint32_t glyph_index;
    float x_offset;
    float y_offset;
    float x_advance;
    float y_advance;
    float x_pos;
    float y_pos;
} ShapedGlyph;

bool font_init(Font *font, const char *filepath);
void font_destroy(Font *font);

bool font_shape_text(Font *font, const char *text, float font_size,
    ShapedGlyph **out_glyphs, size_t *out_count, float *out_width,
    float *out_height, float *out_ascent);

#endif // __FONT_H_
