#include "widget.h"
#include <stdlib.h>
#include <string.h>

void
render_list_init(RenderList *list)
{
    if (!list) return;
    list->commands = NULL;
    list->count = 0;
    list->capacity = 0;
}

void
render_list_clear(RenderList *list)
{
    if (!list) return;
    list->count = 0;
}

void
render_list_free(RenderList *list)
{
    if (!list) return;
    if (list->commands) {
        free(list->commands);
        list->commands = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}

void
render_list_push_rect(RenderList *list, const RectCommand *cmd)
{
    if (!list || !cmd) return;

    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
        RenderCommand *new_cmds =
            realloc(list->commands, new_cap * sizeof(RenderCommand));
        if (!new_cmds) return;
        list->commands = new_cmds;
        list->capacity = new_cap;
    }

    list->commands[list->count].type = CMD_RECT;
    list->commands[list->count].rect = *cmd;
    list->count++;
}

void
render_list_push_glyph(RenderList *list, const GlyphCommand *cmd)
{
    if (!list || !cmd) return;

    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
        RenderCommand *new_cmds =
            realloc(list->commands, new_cap * sizeof(RenderCommand));
        if (!new_cmds) return;
        list->commands = new_cmds;
        list->capacity = new_cap;
    }

    list->commands[list->count].type = CMD_GLYPH;
    list->commands[list->count].glyph = *cmd;
    list->count++;
}

void
widget_init(Widget *w)
{
    if (!w) return;
    memset(w, 0, sizeof(*w));
}

void
widget_add_child(Widget *parent, Widget *child)
{
    if (!parent || !child) return;

    child->parent = parent;
    child->next_sibling = NULL;

    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        Widget *curr = parent->first_child;
        while (curr->next_sibling) {
            curr = curr->next_sibling;
        }
        curr->next_sibling = child;
    }
}

void
widget_remove_child(Widget *parent, Widget *child)
{
    if (!parent || !child || child->parent != parent) return;

    if (parent->first_child == child) {
        parent->first_child = child->next_sibling;
    } else {
        Widget *curr = parent->first_child;
        while (curr && curr->next_sibling != child) {
            curr = curr->next_sibling;
        }
        if (curr) { curr->next_sibling = child->next_sibling; }
    }

    child->parent = NULL;
    child->next_sibling = NULL;
}

void
widget_layout(Widget *w)
{
    if (!w) return;

    if (w->layout) { w->layout(w); }

    for (Widget *child = w->first_child; child; child = child->next_sibling) {
        widget_layout(child);
    }
}

static void
vbox_layout(Widget *w)
{
    VBox *vb = (VBox *)w;
    float y = 0.0f;

    for (Widget *child = w->first_child; child; child = child->next_sibling) {
        child->x = 0.0f;
        child->y = y;

        y += child->height + vb->spacing;
    }
}

void
vbox_init(VBox *box, float spacing)
{
    if (!box) return;
    widget_init(&box->base);
    box->spacing = spacing;
    box->base.layout = vbox_layout;
}

static void
hbox_layout(Widget *w)
{
    HBox *hb = (HBox *)w;
    float x = 0.0f;

    for (Widget *child = w->first_child; child; child = child->next_sibling) {
        child->x = x;
        child->y = 0.0f;

        x += child->width + hb->spacing;
    }
}

void
hbox_init(HBox *box, float spacing)
{
    if (!box) return;
    widget_init(&box->base);
    box->spacing = spacing;
    box->base.layout = hbox_layout;
}

static void
widget_render_internal(
    Widget *w, RenderList *list, float parent_x, float parent_y)
{
    if (!w || !list) return;

    w->abs_x = parent_x + w->x;
    w->abs_y = parent_y + w->y;

    if (w->render) { w->render(w, list); }

    for (Widget *child = w->first_child; child; child = child->next_sibling) {
        widget_render_internal(child, list, w->abs_x, w->abs_y);
    }
}

void
widget_render(Widget *w, RenderList *list)
{ widget_render_internal(w, list, 0.0f, 0.0f); }

static void
rectangle_widget_render(Widget *w, RenderList *list)
{
    if (!w || !list) return;

    RectangleWidget *rw = (RectangleWidget *)w;

    RectCommand cmd = {
        .x = w->abs_x,
        .y = w->abs_y,
        .width = w->width,
        .height = w->height,
        .color = rw->color,
        .radius = rw->radius,
    };

    render_list_push_rect(list, &cmd);
}

void
rectangle_widget_init(RectangleWidget *rw, float x, float y, float width,
    float height, uint32_t color, float radius)
{
    if (!rw) return;

    widget_init(&rw->base);
    rw->base.x = x;
    rw->base.y = y;
    rw->base.width = width;
    rw->base.height = height;
    rw->base.render = rectangle_widget_render;

    rw->color = color;
    rw->radius = radius;
}

static void
text_widget_layout(Widget *w)
{
    if (!w) return;
    TextWidget *tw = (TextWidget *)w;
    if (!tw->text || !tw->font) return;

    font_shape_text(tw->font, tw->text, tw->font_size, NULL, NULL, &w->width,
        &w->height, NULL);
}

static void
text_widget_render(Widget *w, RenderList *list)
{
    if (!w || !list) return;
    TextWidget *tw = (TextWidget *)w;
    if (!tw->text || !tw->font || !tw->atlas) return;

    ShapedGlyph *glyphs = NULL;
    size_t count = 0;
    float ascent = 0.0f;

    if (!font_shape_text(tw->font, tw->text, tw->font_size, &glyphs, &count,
            NULL, NULL, &ascent)) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        AtlasGlyphEntry entry;
        if (glyph_atlas_get_glyph(tw->atlas, tw->font, glyphs[i].glyph_index,
                tw->font_size, &entry)) {
            if (entry.width > 0.0f && entry.height > 0.0f) {
                GlyphCommand gcmd = {
                    .x = w->abs_x + glyphs[i].x_pos + entry.bearing_x,
                    .y = w->abs_y + ascent - entry.bearing_y,
                    .width = entry.width,
                    .height = entry.height,
                    .u0 = entry.u0,
                    .v0 = entry.v0,
                    .u1 = entry.u1,
                    .v1 = entry.v1,
                    .color = tw->color,
                };
                render_list_push_glyph(list, &gcmd);
            }
        }
    }

    if (glyphs) free(glyphs);
}

void
text_widget_init(TextWidget *tw, const char *text, Font *font,
    GlyphAtlas *atlas, float font_size, uint32_t color)
{
    if (!tw) return;

    widget_init(&tw->base);
    tw->text = text;
    tw->font = font;
    tw->atlas = atlas;
    tw->font_size = font_size;
    tw->color = color;
    tw->base.layout = text_widget_layout;
    tw->base.render = text_widget_render;
    tw->base.flags |= WIDGET_TYPE_TEXT;
}

GlyphAtlas *
widget_find_atlas(Widget *w)
{
    if (!w) return NULL;
    if (w->flags & WIDGET_TYPE_TEXT) {
        TextWidget *tw = (TextWidget *)w;
        if (tw->atlas) return tw->atlas;
    }
    for (Widget *child = w->first_child; child; child = child->next_sibling) {
        GlyphAtlas *a = widget_find_atlas(child);
        if (a) return a;
    }
    return NULL;
}
