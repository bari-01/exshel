#include "glyph_atlas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
glyph_atlas_init(GlyphAtlas *atlas, uint32_t width, uint32_t height)
{
    if (!atlas || width == 0 || height == 0) return false;
    memset(atlas, 0, sizeof(*atlas));

    atlas->width = width;
    atlas->height = height;
    atlas->buffer = calloc(width * height, sizeof(uint8_t));
    if (!atlas->buffer) return false;

    atlas->cursor_x = 1;
    atlas->cursor_y = 1;
    atlas->row_height = 0;
    atlas->dirty = true;
    atlas->entry_count = 0;

    return true;
}

void
glyph_atlas_destroy(GlyphAtlas *atlas)
{
    if (!atlas) return;
    if (atlas->buffer) {
        free(atlas->buffer);
        atlas->buffer = NULL;
    }
    atlas->entry_count = 0;
}

bool
glyph_atlas_is_dirty(const GlyphAtlas *atlas)
{ return atlas ? atlas->dirty : false; }

void
glyph_atlas_clear_dirty(GlyphAtlas *atlas)
{
    if (atlas) atlas->dirty = false;
}

bool
glyph_atlas_get_glyph(GlyphAtlas *atlas, Font *font, uint32_t glyph_index,
    float font_size, AtlasGlyphEntry *out_entry)
{
    if (!atlas || !font || !font->face) return false;

    uint32_t size_key = (uint32_t)(font_size * 10.0f);

    for (size_t i = 0; i < atlas->entry_count; i++) {
        if (atlas->entries[i].valid &&
            atlas->entries[i].glyph_index == glyph_index &&
            atlas->entries[i].font_size_key == size_key) {
            if (out_entry) *out_entry = atlas->entries[i];
            return true;
        }
    }

    // re-raster
    FT_Set_Pixel_Sizes(font->face, 0, (uint32_t)font_size);
    if (FT_Load_Glyph(font->face, glyph_index, FT_LOAD_RENDER) != 0) {
        return false;
    }

    FT_GlyphSlot slot = font->face->glyph;
    uint32_t gw = slot->bitmap.width;
    uint32_t gh = slot->bitmap.rows;

    if (gw == 0 || gh == 0) {
        if (atlas->entry_count < GLYPH_ATLAS_CAPACITY) {
            AtlasGlyphEntry entry = {
                .glyph_index = glyph_index,
                .font_size_key = size_key,
                .u0 = 0.0f,
                .v0 = 0.0f,
                .u1 = 0.0f,
                .v1 = 0.0f,
                .width = 0.0f,
                .height = 0.0f,
                .bearing_x = slot->bitmap_left,
                .bearing_y = slot->bitmap_top,
                .valid = true,
            };
            atlas->entries[atlas->entry_count++] = entry;
            if (out_entry) *out_entry = entry;
            return true;
        }
        return false;
    }

    if (atlas->cursor_x + gw + 1 >= atlas->width) {
        atlas->cursor_x = 1;
        atlas->cursor_y += atlas->row_height + 1;
        atlas->row_height = 0;
    }

    if (atlas->cursor_y + gh + 1 >= atlas->height) {
        fprintf(
            stderr, "Glyph atlas full! (%ux%u)\n", atlas->width, atlas->height);
        return false;
    }

    uint32_t x_offset = atlas->cursor_x;
    uint32_t y_offset = atlas->cursor_y;

    for (uint32_t r = 0; r < gh; r++) {
        uint8_t *dst = atlas->buffer + (y_offset + r) * atlas->width + x_offset;
        const uint8_t *src = slot->bitmap.buffer + r * slot->bitmap.pitch;
        memcpy(dst, src, gw);
    }

    atlas->dirty = true;
    atlas->cursor_x += gw + 1;
    if (gh > atlas->row_height) { atlas->row_height = gh; }

    AtlasGlyphEntry entry = {
        .glyph_index = glyph_index,
        .font_size_key = size_key,
        .u0 = (float)x_offset / (float)atlas->width,
        .v0 = (float)y_offset / (float)atlas->height,
        .u1 = (float)(x_offset + gw) / (float)atlas->width,
        .v1 = (float)(y_offset + gh) / (float)atlas->height,
        .width = (float)gw,
        .height = (float)gh,
        .bearing_x = (float)slot->bitmap_left,
        .bearing_y = (float)slot->bitmap_top,
        .valid = true,
    };

    if (atlas->entry_count < GLYPH_ATLAS_CAPACITY) {
        atlas->entries[atlas->entry_count++] = entry;
    }

    if (out_entry) *out_entry = entry;
    return true;
}
