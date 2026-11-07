#include <math.h>

#include "vui/vui.h"

#define DEFAULT_OUTER_PADDING 10.0
#define DEFAULT_INNER_PADDING 10.0
#define BORDER_RADIUS         15.0
#define MAX_FONT_SCALE        96.0

#define BUTTON_BASE_COLOR    0x20/255.0, 0x20/255.0, 0x20/255.0, 0xff/255.0
#define BUTTON_HOVERED_COLOR 0x24/255.0, 0x24/255.0, 0x24/255.0, 0xff/255.0
#define BUTTON_ACTIVE_COLOR  0x28/255.0, 0x28/255.0, 0x28/255.0, 0xff/255.0
#define FG_COLOR             0x86/255.0, 0x8a/255.0, 0xac/255.0, 0xff/255.0

bool vui_init(Vui *vui, VuiBackend *backend) {
  vui->backend = backend;

  DA_APPEND(vui->stack, (VuiWidgetStackItem) {0});

  return true;
}

void vui_begin_frame(Vui *vui) {
  for (u32 i = 0; i < vui->stack.len; ++i)
    vui->stack.items[i].pos = 0;

  vui->layouts.len = 0;
  VuiLayout layout = {
    VuiLayoutKindVertical,
    0, 0, 0,
    DEFAULT_OUTER_PADDING,
    DEFAULT_INNER_PADDING,
    0.0, 0.0, 0.0, 0.0,
    vui->children_constraint,
  };
  vui->backend->get_window_size(vui->backend->data, &layout.w, &layout.h);
  DA_APPEND(vui->layouts, layout);
  vui->current_layout = 0;

  vui_ccons_fill(vui);

  vui->backend->begin_frame(vui->backend->data);
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
      switch (item->states.items[j].constraint.kind) {
      case VuiConstraintKindFill: {
        ++fills_count;
      } break;

      case VuiConstraintKindFixed: {
        fixed_space += item->states.items[j].constraint.value;
      } break;
      }
    }

    f32 w, h;
    switch (layout->kind) {
    case VuiLayoutKindVertical: {
      w = layout->w - layout->outer_padding * 2.0;
      h = (layout->h -
           layout->outer_padding * 2.0 -
           layout->inner_padding * (layout->len - 1) -
           fixed_space) / fills_count;
    } break;

    case VuiLayoutKindHorizontal: {
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
      item->states.items[j].x = x;
      item->states.items[j].y = y;

      switch (item->states.items[j].constraint.kind) {
      case VuiConstraintKindFill: {
        item->states.items[j].w = w;
        item->states.items[j].h = h;
      } break;

      case VuiConstraintKindFixed: {
        switch (layout->kind) {
        case VuiLayoutKindVertical: {
          item->states.items[j].w = w;
          item->states.items[j].h = item->states.items[j].constraint.value;
        } break;

        case VuiLayoutKindHorizontal: {
          item->states.items[j].w = item->states.items[j].constraint.value;
          item->states.items[j].h = h;
        } break;
        }
      } break;
      }

      if (item->states.items[j].layout_index != (u32) -1) {
        VuiLayout *other_layout =
          vui->layouts.items + item->states.items[j].layout_index;
        other_layout->x = item->states.items[j].x;
        other_layout->y = item->states.items[j].y;
        other_layout->w = item->states.items[j].w;
        other_layout->h = item->states.items[j].h;
      }

      switch (layout->kind) {
      case VuiLayoutKindVertical: {
        y += item->states.items[j].h + layout->inner_padding;
      } break;

      case VuiLayoutKindHorizontal: {
        x += item->states.items[j].w + layout->inner_padding;
      } break;
      }
    }
  }

  vui->mouse_down_this_frame = false;
  vui->mouse_up_this_frame = false;
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
  VuiLayout *current = vui->layouts.items + vui->current_layout;
  VuiWidgetStackItem *item = vui->stack.items + current->depth;

  if (item->pos++ >= item->states.len)
    DA_APPEND(item->states, (VuiWidgetState) {0});

  VuiWidgetState *result = item->states.items + current->pos + current->len++;

  result->is_hovered =
    vui->mouse_x >= result->x && vui->mouse_x <= result->x + result->w &&
    vui->mouse_y >= result->y && vui->mouse_y <= result->y + result->h;
  result->is_released = false;
  result->layout_index = (u32) -1;
  result->constraint = vui->constraint;

  vui->constraint = vui->children_constraint;

  return result;
}

bool vui_button(Vui *vui, const char *text) {
  VuiWidgetState *state = push_widget(vui);

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

  f32 font_scale = fminf(state->h, MAX_FONT_SCALE);
  f32 w = vui->backend->measure_text(vui->backend->data, text, font_scale);
  f32 x = state->x + (state->w - w) * 0.5;
  f32 y = state->y + (state->h - font_scale) * 0.5;
  vui->backend->draw_text(vui->backend->data, text, font_scale, x, y, FG_COLOR);

  return state->is_released;
}

static void vui_layout_begin(Vui *vui,
                             f32 outer_padding,
                             f32 inner_padding,
                             VuiLayoutKind kind) {
  VuiWidgetState *state = push_widget(vui);
  VuiLayout *current = vui->layouts.items + vui->current_layout;

  VuiLayout new = {
    kind,
    current->depth + 1, 0, 0,
    outer_padding, inner_padding,
    state->x, state->y, state->w, state->h,
    vui->children_constraint,
  };

  vui_ccons_fill(vui);

  if (new.depth >= vui->stack.len)
    DA_APPEND(vui->stack, (VuiWidgetStackItem) {0});
  else
    new.pos = vui->stack.items[vui->stack.len - 1].pos;

  vui->current_layout = vui->layouts.len;
  state->layout_index = vui->current_layout;
  DA_APPEND(vui->layouts, new);
}

void vui_layout_begin_vert(Vui *vui, f32 outer_padding, f32 inner_padding) {
  vui_layout_begin(vui, outer_padding, inner_padding, VuiLayoutKindVertical);
}

void vui_layout_begin_horz(Vui *vui, f32 outer_padding, f32 inner_padding) {
  vui_layout_begin(vui, outer_padding, inner_padding, VuiLayoutKindHorizontal);
}

void vui_layout_end(Vui *vui) {
  u32 depth = vui->layouts.items[vui->current_layout].depth;
  while (vui->current_layout > 0 &&
         vui->layouts.items[vui->current_layout].depth >= depth)
    --vui->current_layout;
}

void vui_cons_fill(Vui *vui) {
  vui->constraint = (VuiConstraint) { VuiConstraintKindFill, 0.0 };
}

void vui_cons_fixed(Vui *vui, f32 value) {
  vui->constraint = (VuiConstraint) { VuiConstraintKindFixed, value };
}

void vui_ccons_fill(Vui *vui) {
  vui->children_constraint = (VuiConstraint) { VuiConstraintKindFill, 0.0 };
}

void vui_ccons_fixed(Vui *vui, f32 value) {
  vui->children_constraint = (VuiConstraint) { VuiConstraintKindFixed, value };
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
