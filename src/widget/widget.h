#ifndef __WIDGET_H_
#define __WIDGET_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Widget Widget;
typedef struct RenderList RenderList;
typedef struct UIEvent UIEvent;

struct UIEvent {
    int type;
    float x, y;
};

#include "../font/font.h"
#include "../font/glyph_atlas.h"

typedef struct {
    float x, y;
    float width, height;
    uint32_t color;
    float radius;
} RectCommand;

typedef struct {
    float x, y;
    float width, height;
    float u0, v0, u1, v1;
    uint32_t color;
} GlyphCommand;

typedef enum {
    CMD_RECT = 0,
    CMD_GLYPH = 1,
} CommandType;

typedef struct {
    CommandType type;
    union {
        RectCommand rect;
        GlyphCommand glyph;
    };
} RenderCommand;

struct RenderList {
    RenderCommand *commands;
    size_t count;
    size_t capacity;
};

void render_list_init(RenderList *list);
void render_list_clear(RenderList *list);
void render_list_free(RenderList *list);
void render_list_push_rect(RenderList *list, const RectCommand *cmd);
void render_list_push_glyph(RenderList *list, const GlyphCommand *cmd);

#define WIDGET_TYPE_TEXT (1 << 0)

struct Widget {
    Widget *parent;
    Widget *first_child;
    Widget *next_sibling;

    float x, y;
    float width, height;

    float abs_x, abs_y;

    uint32_t flags;

    void (*layout)(Widget *);
    void (*render)(Widget *, RenderList *);
    bool (*event)(Widget *, const UIEvent *);

    void *userdata;
};

void widget_init(Widget *w);
void widget_add_child(Widget *parent, Widget *child);
void widget_remove_child(Widget *parent, Widget *child);
void widget_layout(Widget *w);
void widget_render(Widget *w, RenderList *list);
GlyphAtlas *widget_find_atlas(Widget *w);

typedef struct {
    Widget base;
    float spacing;
} VBox;

void vbox_init(VBox *box, float spacing);

typedef struct {
    Widget base;
    float spacing;
} HBox;

void hbox_init(HBox *box, float spacing);

typedef struct {
    Widget base;
    uint32_t color;
    float radius;
} RectangleWidget;

void rectangle_widget_init(RectangleWidget *rw, float x, float y, float width,
    float height, uint32_t color, float radius);

typedef struct {
    Widget base;
    const char *text;
    Font *font;
    GlyphAtlas *atlas;
    float font_size;
    uint32_t color;
} TextWidget;

void text_widget_init(TextWidget *tw, const char *text, Font *font,
    GlyphAtlas *atlas, float font_size, uint32_t color);

#endif // __WIDGET_H_
