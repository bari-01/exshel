#include "font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
font_init(Font *font, const char *filepath)
{
    if (!font || !filepath) return false;
    memset(font, 0, sizeof(*font));

    if (FT_Init_FreeType(&font->ft_lib) != 0) {
        fprintf(stderr, "Failed to initialize FreeType\n");
        return false;
    }

    if (FT_New_Face(font->ft_lib, filepath, 0, &font->face) != 0) {
        fprintf(stderr, "Failed to load font: %s\n", filepath);
        FT_Done_FreeType(font->ft_lib);
        font->ft_lib = NULL;
        return false;
    }

    font->hb_font = hb_ft_font_create(font->face, NULL);
    if (!font->hb_font) {
        fprintf(stderr, "Failed to create HarfBuzz font\n");
        FT_Done_Face(font->face);
        FT_Done_FreeType(font->ft_lib);
        memset(font, 0, sizeof(*font));
        return false;
    }

    return true;
}

void
font_destroy(Font *font)
{
    if (!font) return;
    if (font->hb_font) {
        hb_font_destroy(font->hb_font);
        font->hb_font = NULL;
    }
    if (font->face) {
        FT_Done_Face(font->face);
        font->face = NULL;
    }
    if (font->ft_lib) {
        FT_Done_FreeType(font->ft_lib);
        font->ft_lib = NULL;
    }
}

bool
font_shape_text(Font *font, const char *text, float font_size,
    ShapedGlyph **out_glyphs, size_t *out_count, float *out_width,
    float *out_height, float *out_ascent)
{
    if (!font || !font->face || !font->hb_font || !text) return false;

    FT_Set_Pixel_Sizes(font->face, 0, (uint32_t)font_size);
    hb_ft_font_changed(font->hb_font);

    hb_buffer_t *buf = hb_buffer_create();
    if (!buf) return false;

    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(font->hb_font, buf, NULL, 0);

    unsigned int len = 0;
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, &len);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, &len);

    if (len == 0 || !info || !pos) {
        hb_buffer_destroy(buf);
        if (out_glyphs) *out_glyphs = NULL;
        if (out_count) *out_count = 0;
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
        if (out_ascent) *out_ascent = 0.0f;
        return true;
    }

    ShapedGlyph *glyphs = malloc(len * sizeof(ShapedGlyph));
    if (!glyphs) {
        hb_buffer_destroy(buf);
        return false;
    }

    float cur_x = 0.0f;
    float cur_y = 0.0f;

    for (unsigned int i = 0; i < len; i++) {
        glyphs[i].glyph_index = info[i].codepoint;
        glyphs[i].x_offset = pos[i].x_offset / 64.0f;
        glyphs[i].y_offset = pos[i].y_offset / 64.0f;
        glyphs[i].x_advance = pos[i].x_advance / 64.0f;
        glyphs[i].y_advance = pos[i].y_advance / 64.0f;

        glyphs[i].x_pos = cur_x + glyphs[i].x_offset;
        glyphs[i].y_pos = cur_y + glyphs[i].y_offset;

        cur_x += glyphs[i].x_advance;
        cur_y += glyphs[i].y_advance;
    }

    float ascent = (float)(font->face->size->metrics.ascender >> 6);
    float descent = (float)(-font->face->size->metrics.descender >> 6);
    float height = (float)(font->face->size->metrics.height >> 6);
    if (height < (ascent + descent)) { height = ascent + descent; }

    if (out_glyphs)
        *out_glyphs = glyphs;
    else
        free(glyphs);

    if (out_count) *out_count = len;
    if (out_width) *out_width = cur_x;
    if (out_height) *out_height = height;
    if (out_ascent) *out_ascent = ascent;

    hb_buffer_destroy(buf);
    return true;
}
