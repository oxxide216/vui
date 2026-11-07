#include <math.h>

#include "vui/vui.h"

#define DEFAULT_PADDING 10.0
#define BORDER_RADIUS   10.0
#define MAX_FONT_SCALE  96.0

#define BUTTON_COLOR 0x20/255.0, 0x20/255.0, 0x20/255.0, 0xff/255.0
#define FG_COLOR     0x86/255.0, 0x8a/255.0, 0xac/255.0, 0xff/255.0

bool vui_init(Vui *vui, VuiBackend *backend) {
  vui->backend = backend;

  DA_APPEND(vui->stack, (VuiWidgetStackItem) {0});

  return true;
}

void vui_begin_frame(Vui *vui) {
  for (u32 i = 0; i < vui->stack.len; ++i)
    vui->stack.items[i].pos = 0;
  vui->depth = 0;

  vui->layouts.len = 0;
  VuiLayout layout = {
    VuiLayoutKindVertical,
    0, 0, 0, DEFAULT_PADDING,
    0.0, 0.0, 0.0, 0.0,
  };
  vui->backend->get_window_size(vui->backend->data, &layout.w, &layout.h);
  DA_APPEND(vui->layouts, layout);

  vui->backend->begin_frame(vui->backend->data);
}

void vui_end_frame(Vui *vui) {
  vui->backend->end_frame(vui->backend->data);

  for (u32 i = 0; i < vui->layouts.len; ++i) {
    VuiLayout *layout = vui->layouts.items + i;

    if (layout->len == 0)
      continue;

    f32 w, h;
    switch (layout->kind) {
    case VuiLayoutKindVertical: {
      w = layout->w - layout->padding * 2.0;
      h = (layout->h - layout->padding * (layout->len + 1)) / layout->len;
    } break;

    case VuiLayoutKindHorizontal: {
      w = (layout->w - layout->padding * (layout->len + 1)) / layout->len;
      h = layout->h - layout->padding * 2.0;
    } break;
    }

    VuiWidgetStackItem *item = vui->stack.items + layout->depth;
    f32 x = layout->padding, y = layout->padding;

    for (u32 j = layout->pos; j < layout->pos + layout->len; ++j) {
      item->states.items[j].x = x;
      item->states.items[j].y = y;
      item->states.items[j].w = w;
      item->states.items[j].h = h;

      switch (layout->kind) {
      case VuiLayoutKindVertical: {
        y += h + layout->padding;
      } break;

      case VuiLayoutKindHorizontal: {
        x += w + layout->padding;
      } break;
      }
    }
  }
}

void vui_destroy(Vui *vui) {
  vui->backend->destroy(vui->backend->data);

  for (u32 i = 0; i < vui->stack.len; ++i)
    if (vui->stack.items[i].states.items)
      free(vui->stack.items[i].states.items);
  if (vui->stack.items)
    free(vui->stack.items);

  if (vui->layouts.items)
    free(vui->layouts.items);
}

static VuiWidgetState *push_widget(Vui *vui) {
  VuiWidgetStackItem *item = vui->stack.items + vui->depth;

  if (item->pos >= item->states.len)
    DA_APPEND(item->states, (VuiWidgetState) {0});

  ++vui->layouts.items[vui->layouts.len - 1].len;

  return item->states.items + item->pos++;
}

bool vui_button(Vui *vui, const char *text) {
  VuiWidgetState *state = push_widget(vui);

  vui->backend->draw_rounded_rect(vui->backend->data,
                                  BORDER_RADIUS,
                                  state->x, state->y,
                                  state->w, state->h,
                                  BUTTON_COLOR);

  f32 font_scale = fminf(state->h, MAX_FONT_SCALE);
  f32 w = vui->backend->measure_text(vui->backend->data, text, font_scale);
  f32 x = state->x + (state->w - w) * 0.5;
  f32 y = state->y + (state->h - font_scale) * 0.5;
  vui->backend->draw_text(vui->backend->data, text, font_scale, x, y, FG_COLOR);

  return state->is_released;
}
