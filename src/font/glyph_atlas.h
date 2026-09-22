#ifndef __GLYPH_ATLAS_H_
#define __GLYPH_ATLAS_H_

#include "font.h"
#include <stdbool.h>
#include <stdint.h>

#define GLYPH_ATLAS_CAPACITY 1024

typedef struct {
    uint32_t glyph_index;
    uint32_t font_size_key;
    float u0, v0, u1, v1;
    float width, height;
    float bearing_x, bearing_y;
    bool valid;
} AtlasGlyphEntry;

typedef struct GlyphAtlas GlyphAtlas;

struct GlyphAtlas {
    uint8_t *buffer;
    uint32_t width;
    uint32_t height;

    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t row_height;

    bool dirty;

    AtlasGlyphEntry entries[GLYPH_ATLAS_CAPACITY];
    size_t entry_count;
};

bool glyph_atlas_init(GlyphAtlas *atlas, uint32_t width, uint32_t height);
void glyph_atlas_destroy(GlyphAtlas *atlas);

bool glyph_atlas_get_glyph(GlyphAtlas *atlas, Font *font, uint32_t glyph_index,
    float font_size, AtlasGlyphEntry *out_entry);

bool glyph_atlas_is_dirty(const GlyphAtlas *atlas);
void glyph_atlas_clear_dirty(GlyphAtlas *atlas);

#endif // __GLYPH_ATLAS_H_
