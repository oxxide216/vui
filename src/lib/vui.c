#include <math.h>

#include "vui/vui.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

#define DEFAULT_LAYOUT_OUTER_PADDING 10.0
#define DEFAULT_LAYOUT_INNER_PADDING 10.0
#define DEFAULT_LABEL_PADDING        10.0
#define BUTTON_PADDING_PERC          0.2
#define BORDER_RADIUS                15.0
#define MAX_FONT_SCALE               96.0

#define BG_COLOR             0x18/255.0, 0x18/255.0, 0x18/255.0, 0xff/255.0
#define FG_COLOR             0x86/255.0, 0x8a/255.0, 0xac/255.0, 0xff/255.0
#define BUTTON_BASE_COLOR    0x24/255.0, 0x24/255.0, 0x24/255.0, 0xff/255.0
#define BUTTON_HOVERED_COLOR 0x28/255.0, 0x28/255.0, 0x28/255.0, 0xff/255.0
#define BUTTON_ACTIVE_COLOR  0x2c/255.0, 0x2c/255.0, 0x2c/255.0, 0xff/255.0

bool vui_init(Vui *vui, VuiBackend *backend) {
  vui->backend = backend;

  DA_APPEND(vui->stack, (VuiWidgetStackItem) {0});

  return true;
}

static void ct_reset_counters(VuiCounterTable *ct) {
  for (u32 i = 0; i < VUI_COUNTER_TABLE_CAP; ++i) {
    VuiCounter *counter = ct->items[i];
    while (counter) {
      counter->value = 0;
      counter = counter->next;
    }
  }
}

void vui_begin_frame(Vui *vui) {
  for (u32 i = 0; i < vui->stack.len; ++i)
    vui->stack.items[i].len = 0;

  ct_reset_counters(&vui->counter_table);

  vui->layouts.len = 0;
  VuiLayout layout = {
    VuiLayoutKindVertical,
    0, 0, 0,
    DEFAULT_LAYOUT_OUTER_PADDING,
    DEFAULT_LAYOUT_INNER_PADDING,
    0.0, 0.0, 0.0, 0.0,
  };
  vui->backend->get_window_size(vui->backend->data, &layout.w, &layout.h);
  DA_APPEND(vui->layouts, layout);
  vui->current_layout = 0;

  vui->backend->begin_frame(vui->backend->data);
  vui->backend->draw_rect(vui->backend->data,
                          layout.x, layout.y,
                          layout.w, layout.h,
                          vui->bg_r, vui->bg_g,
                          vui->bg_b, vui->bg_a);

  vui_bg_color(vui, 0.0, 0.0, 0.0, 0.0);
}

void vui_end_frame(Vui *vui) {
  vui->backend->end_frame(vui->backend->data);

  for (u32 i = 0; i < vui->layouts.len; ++i) {
    VuiLayout *layout = vui->layouts.items + i;

    if (layout->len == 0)
      continue;

    VuiWidgetStackItem *item = vui->stack.items + layout->depth;

    u32 fills_count = 0;
    f32 fixed_space = 0.0;

    for (u32 j = layout->pos; j < layout->pos + layout->len; ++j) {
      switch (item->items[j]->constraint.kind) {
      case VuiConstraintKindFill: {
        ++fills_count;
      } break;

      case VuiConstraintKindFixed: {
        fixed_space += item->items[j]->constraint.value;
      } break;
      }
    }

    f32 w, h;
    switch (layout->kind) {
    case VuiLayoutKindVertical: {
      w = layout->w - layout->outer_padding * 2.0;
      if (fills_count == 0)
        h = 0.0;
      else
        h = (layout->h -
             layout->outer_padding * 2.0 -
             layout->inner_padding * (layout->len - 1) -
             fixed_space) / fills_count;
    } break;

    case VuiLayoutKindHorizontal: {
      if (fills_count == 0)
        w = 0.0;
      else
        w = (layout->w -
             layout->outer_padding * 2.0 -
             layout->inner_padding * (layout->len - 1) -
             fixed_space) / fills_count;
      h = layout->h - layout->outer_padding * 2.0;
    } break;
    }

    f32 x = layout->x + layout->outer_padding;
    f32 y = layout->y + layout->outer_padding;

    for (u32 j = layout->pos; j < layout->pos + layout->len; ++j) {
      item->items[j]->x = x;
      item->items[j]->y = y;

      switch (item->items[j]->constraint.kind) {
      case VuiConstraintKindFill: {
        item->items[j]->w = w;
        item->items[j]->h = h;
      } break;

      case VuiConstraintKindFixed: {
        switch (layout->kind) {
        case VuiLayoutKindVertical: {
          item->items[j]->w = w;
          item->items[j]->h = item->items[j]->constraint.value;
        } break;

        case VuiLayoutKindHorizontal: {
          item->items[j]->w = item->items[j]->constraint.value;
          item->items[j]->h = h;
        } break;
        }
      } break;
      }

      if (item->items[j]->layout_index != (u32) -1) {
        VuiLayout *other_layout =
          vui->layouts.items + item->items[j]->layout_index;
        other_layout->x = item->items[j]->x;
        other_layout->y = item->items[j]->y;
        other_layout->w = item->items[j]->w;
        other_layout->h = item->items[j]->h;
      }

      switch (layout->kind) {
      case VuiLayoutKindVertical: {
        y += item->items[j]->h + layout->inner_padding;
      } break;

      case VuiLayoutKindHorizontal: {
        x += item->items[j]->w + layout->inner_padding;
      } break;
      }
    }
  }

  vui_bg_color(vui, BG_COLOR);

  vui->mouse_down_this_frame = false;
  vui->mouse_up_this_frame = false;
}

static void wt_destroy(VuiWidgetTable *wt) {
  for (u32 i = 0; i < VUI_WIDGET_TABLE_CAP; ++i) {
    VuiWidgetState *state = wt->items[i];
    while (state) {
      VuiWidgetState *next = state->next;
      free(state);
      state = next;
    }
  }
}

static void ct_destroy(VuiCounterTable *ct) {
  for (u32 i = 0; i < VUI_COUNTER_TABLE_CAP; ++i) {
    VuiCounter *counter = ct->items[i];
    while (counter) {
      VuiCounter *next = counter->next;
      free(counter);
      counter = next;
    }
  }
}

void vui_destroy(Vui *vui) {
  vui->backend->destroy(vui->backend->data);

  wt_destroy(&vui->widget_table);
  ct_destroy(&vui->counter_table);

  for (u32 i = 0; i < vui->stack.len; ++i)
    if (vui->stack.items[i].items)
      free(vui->stack.items[i].items);
  if (vui->stack.items)
    free(vui->stack.items);

  if (vui->layouts.items)
    free(vui->layouts.items);
}

static u32 *ct_get(VuiCounterTable *ct, u64 id) {
  VuiCounter **result = ct->items + id % VUI_COUNTER_TABLE_CAP;

  while (*result) {
    if ((*result)->key == id)
      return &(*result)->value;

    result = &(*result)->next;
  }

  *result = malloc(sizeof(**result));
  (*result)->key = id;
  (*result)->value = 0;
  (*result)->next = NULL;

  return &(*result)->value;
}

static VuiWidgetState *wt_get_inc(VuiWidgetTable *wt, u64 id, u32 counter) {
  VuiWidgetState **result = wt->items + id % VUI_WIDGET_TABLE_CAP;

  while (*result) {
    if ((*result)->key.id == id && (*result)->key.counter == counter)
      return *result;

    result = &(*result)->next;
  }

  *result = malloc(sizeof(**result));
  memset(*result, 0, sizeof(**result));
  (*result)->key.id = id;
  (*result)->key.counter = counter;

  return *result;
}

static VuiWidgetState *push_widget(Vui *vui, u64 id) {
  VuiLayout *current = vui->layouts.items + vui->current_layout;
  VuiWidgetStackItem *item = vui->stack.items + current->depth;

  u32 *counter = ct_get(&vui->counter_table, id);
  VuiWidgetState *result = wt_get_inc(&vui->widget_table, id, *counter);
  ++*counter;

  DA_APPEND(*item, result);

  ++current->len;

  result->is_hovered =
    vui->mouse_x >= result->x && vui->mouse_x <= result->x + result->w &&
    vui->mouse_y >= result->y && vui->mouse_y <= result->y + result->h;
  result->is_released = false;
  result->layout_index = (u32) -1;
  result->constraint = vui->constraint;

  vui_cons_fill(vui);

  vui->backend->draw_rect(vui->backend->data,
                          result->x, result->y,
                          result->w, result->h,
                          vui->bg_r, vui->bg_g,
                          vui->bg_b, vui->bg_a);

  vui_bg_color(vui, 0.0, 0.0, 0.0, 0.0);

  return result;
}

static f32 fit_text(Vui *vui, const char *text, f32 w, f32 h) {
  const f32 max_diff = 5.0;

  f32 result = fminf(h, MAX_FONT_SCALE);
  f32 min = 0.0, max = result * 2.0;

  for (u32 i = 0; i < 25; ++i) {
    f32 measured =
      vui->backend->measure_text(vui->backend->data, text, result);
    if (w - measured > max_diff) {
      min = result;
      result = (result + max) * 0.5;
    } else if (w - measured < -max_diff) {
      max = result;
      result = (result + min) * 0.5;
    } else {
      break;
    }
  }

  return result;
}

bool vui_button_id(Vui *vui, u64 id, const char *text) {
  VuiWidgetState *state = push_widget(vui, id);

  if (state->is_hovered) {
    if (vui->mouse_down_this_frame)
      state->is_pressed = true;

    if (state->is_pressed && vui->mouse_up_this_frame) {
      state->is_pressed = false;
      state->is_released = true;
    }
  } else {
    state->is_pressed = false;
  }

  if (state->is_pressed)
    vui->backend->draw_rounded_rect(vui->backend->data,
                                    BORDER_RADIUS,
                                    state->x, state->y,
                                    state->w, state->h,
                                    BUTTON_ACTIVE_COLOR);
  else if (state->is_hovered)
    vui->backend->draw_rounded_rect(vui->backend->data,
                                    BORDER_RADIUS,
                                    state->x, state->y,
                                    state->w, state->h,
                                    BUTTON_HOVERED_COLOR);
  else
    vui->backend->draw_rounded_rect(vui->backend->data,
                                    BORDER_RADIUS,
                                    state->x, state->y,
                                    state->w, state->h,
                                    BUTTON_BASE_COLOR);

  f32 font_scale = fit_text(vui, text,
                            state->w * (1.0 - BUTTON_PADDING_PERC * 2.0),
                            state->h * (1.0 - BUTTON_PADDING_PERC * 2.0));
  f32 w = vui->backend->measure_text(vui->backend->data, text, font_scale);
  f32 x = state->x + (state->w - w) * 0.5;
  f32 y = state->y + (state->h - font_scale) * 0.5;
  vui->backend->draw_text(vui->backend->data, text, font_scale, x, y, FG_COLOR);

  return state->is_released;
}

static void vui_layout_begin(Vui *vui, u64 id,
                             f32 outer_padding,
                             f32 inner_padding,
                             VuiLayoutKind kind) {
  VuiWidgetState *state = push_widget(vui, id);
  VuiLayout *current = vui->layouts.items + vui->current_layout;

  VuiLayout new = {
    kind,
    current->depth + 1, 0, 0,
    outer_padding, inner_padding,
    state->x, state->y, state->w, state->h,
  };

  if (new.depth >= vui->stack.len)
    DA_APPEND(vui->stack, (VuiWidgetStackItem) {0});
  else
    new.pos = vui->stack.items[vui->stack.len - 1].len;

  vui->current_layout = vui->layouts.len;
  state->layout_index = vui->current_layout;
  DA_APPEND(vui->layouts, new);
}

void vui_layout_begin_vert_id(Vui *vui, u64 id, f32 outer_padding, f32 inner_padding) {
  vui_layout_begin(vui, id, outer_padding, inner_padding, VuiLayoutKindVertical);
}

void vui_layout_begin_horz_id(Vui *vui, u64 id, f32 outer_padding, f32 inner_padding) {
  vui_layout_begin(vui, id, outer_padding, inner_padding, VuiLayoutKindHorizontal);
}

void vui_layout_end(Vui *vui) {
  u32 depth = vui->layouts.items[vui->current_layout].depth;
  while (vui->current_layout > 0 &&
         vui->layouts.items[vui->current_layout].depth >= depth)
    --vui->current_layout;
}

void vui_label_id(Vui *vui, u64 id, const char *text) {
  vui_label_ex_id(vui, id, text, DEFAULT_LABEL_PADDING,
                  VuiHAlignLeft, VuiVAlignCenter);
}

void vui_label_ex_id(Vui *vui, u64 id, const char *text, f32 padding,
                  VuiHAlign h_align, VuiVAlign v_align) {
  VuiWidgetState *state = push_widget(vui, id);

  f32 font_scale = fit_text(vui, text,
                            state->w - padding * 2.0,
                            state->h - padding * 2.0);
  f32 x, y;

  switch (h_align) {
  case VuiHAlignLeft: {
    x = state->x + padding;
  } break;

  case VuiHAlignCenter: {
    f32 w = vui->backend->measure_text(vui->backend->data, text, font_scale);
    x = state->x + (state->w - w) * 0.5;
  } break;

  case VuiHAlignRight: {
    f32 w = vui->backend->measure_text(vui->backend->data, text, font_scale);
    x = state->x + state->w - w - padding;
  } break;
  }

  switch (v_align) {
  case VuiVAlignTop: {
    y = state->y + padding;
  } break;

  case VuiVAlignCenter: {
    y = state->y + (state->h - font_scale) * 0.5;
  } break;

  case VuiVAlignBottom: {
    y = state->y + state->h - font_scale - padding;
  } break;
  }

  vui->backend->draw_text(vui->backend->data, text, font_scale, x, y, FG_COLOR);
}

void vui_cons_fill(Vui *vui) {
  vui->constraint = (VuiConstraint) { VuiConstraintKindFill, 0.0 };
}

void vui_cons_fixed(Vui *vui, f32 value) {
  vui->constraint = (VuiConstraint) { VuiConstraintKindFixed, value };
}

void vui_bg_color(Vui *vui, f32 r, f32 g, f32 b, f32 a) {
  vui->bg_r = r;
  vui->bg_g = g;
  vui->bg_b = b;
  vui->bg_a = a;
}

void vui_on_mouse_move(Vui *vui, f32 x, f32 y) {
  vui->mouse_x = x;
  vui->mouse_y = y;
}

void vui_on_mouse_down(Vui *vui) {
  vui->mouse_down_this_frame = true;
}

void vui_on_mouse_up(Vui *vui) {
  vui->mouse_up_this_frame = true;
}
