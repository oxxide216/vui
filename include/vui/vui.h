#ifndef VUI_H
#define VUI_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

#ifndef VUI_WIDGET_TABLE_CAP
#define VUI_WIDGET_TABLE_CAP 256
#endif

#ifndef VUI_COUNTER_TABLE_CAP
#define VUI_COUNTER_TABLE_CAP 256
#endif

#define VUI_ID() (str_hash(STR_LIT(__FILE__)) + __LINE__)

#define vui_button(vui, text)        \
  vui_button_id(vui, VUI_ID(), text)
#define vui_layout_begin_vert(vui, outer_padding, inner_padding)        \
  vui_layout_begin_vert_id(vui, VUI_ID(), outer_padding, inner_padding)
#define vui_layout_begin_horz(vui, outer_padding, inner_padding)        \
  vui_layout_begin_horz_id(vui, VUI_ID(), outer_padding, inner_padding)
#define vui_label(vui, text)        \
  vui_label_id(vui, VUI_ID(), text)
#define vui_label_ex(vui, text, padding, h_align, v_align)        \
  vui_label_ex_id(vui, VUI_ID(), text, padding, h_align, v_align)

typedef struct VuiBackendData VuiBackendData;

typedef void (*VuiBackendGetWindowSizeFunc)(VuiBackendData *data, f32 *w, f32 *h);
typedef void (*VuiBackendBeginFrameFunc)(VuiBackendData *data);
typedef void (*VuiBackendDrawRectFunc)(VuiBackendData *data,
                                       f32 x, f32 y, f32 w, f32 h,
                                       f32 r, f32 g, f32 b, f32 a);
typedef void (*VuiBackendDrawRoundedRectFunc)(VuiBackendData *data, f32 radius,
                                              f32 x, f32 y, f32 w, f32 h,
                                              f32 r, f32 g, f32 b, f32 a);
typedef f32 (*VuiBackendMeasureTextFunc)(VuiBackendData *data,
                                         const char *text,
                                         f32 font_scale);
typedef void (*VuiBackendDrawTextFunc)(VuiBackendData *data, const char *text,
                                       f32 font_scale, f32 x, f32 y,
                                       f32 r, f32 g, f32 b, f32 a);
typedef void (*VuiBackendEndFrameFunc)(VuiBackendData *data);
typedef void (*VuiBackendDestroyFunc)(VuiBackendData *data);

typedef struct {
  VuiBackendData                *data;
  VuiBackendGetWindowSizeFunc    get_window_size;
  VuiBackendBeginFrameFunc       begin_frame;
  VuiBackendDrawRectFunc         draw_rect;
  VuiBackendDrawRoundedRectFunc  draw_rounded_rect;
  VuiBackendMeasureTextFunc      measure_text;
  VuiBackendDrawTextFunc         draw_text;
  VuiBackendEndFrameFunc         end_frame;
  VuiBackendDestroyFunc          destroy;
} VuiBackend;

typedef enum {
  VuiConstraintKindFill = 0,
  VuiConstraintKindFixed,
} VuiConstraintKind;

typedef struct {
  VuiConstraintKind kind;
  f32               value;
} VuiConstraint;

typedef struct VuiWidgetState VuiWidgetState;

struct VuiWidgetState {
  struct {
    u64 id;
    u32 counter;
  } key;
  f32             x, y, w, h;
  bool            is_hovered;
  bool            is_pressed;
  bool            is_released;
  u32             layout_index;
  VuiConstraint   constraint;
  VuiWidgetState *next;
};

typedef struct {
  VuiWidgetState *items[VUI_WIDGET_TABLE_CAP];
} VuiWidgetTable;

typedef struct VuiCounter VuiCounter;

struct VuiCounter {
  u64           key;
  u32           value;
  VuiCounter   *next;
};

typedef struct {
  VuiCounter *items[VUI_COUNTER_TABLE_CAP];
} VuiCounterTable;

typedef Da(VuiWidgetState *) VuiWidgetStackItem;
typedef Da(VuiWidgetStackItem) VuiWidgetStack;

typedef enum {
  VuiLayoutKindVertical = 0,
  VuiLayoutKindHorizontal,
} VuiLayoutKind;

typedef struct {
  VuiLayoutKind kind;
  u32           depth;
  u32           pos;
  u32           len;
  f32           outer_padding;
  f32           inner_padding;
  f32           x, y, w, h;
} VuiLayout;

typedef Da(VuiLayout) VuiLayouts;

typedef struct {
  VuiBackend      *backend;
  VuiWidgetTable   widget_table;
  VuiCounterTable  counter_table;
  VuiWidgetStack   stack;
  VuiLayouts       layouts;
  u32              current_layout;
  VuiConstraint    constraint;
  f32              bg_r, bg_g, bg_b, bg_a;
  f32              mouse_x;
  f32              mouse_y;
  bool             mouse_down_this_frame;
  bool             mouse_up_this_frame;
} Vui;

typedef enum {
  VuiHAlignLeft = 0,
  VuiHAlignCenter,
  VuiHAlignRight,
} VuiHAlign;

typedef enum {
  VuiVAlignTop = 0,
  VuiVAlignCenter,
  VuiVAlignBottom,
} VuiVAlign;

bool vui_init(Vui *vui, VuiBackend *backend);
void vui_begin_frame(Vui *vui);
void vui_end_frame(Vui *vui);
void vui_destroy(Vui *vui);

// Widgets
bool vui_button_id(Vui *vui, u64 id, const char *text);
void vui_layout_begin_vert_id(Vui *vui, u64 id, f32 outer_padding, f32 inner_padding);
void vui_layout_begin_horz_id(Vui *vui, u64 id, f32 outer_padding, f32 inner_padding);
void vui_layout_end(Vui *vui);
void vui_label_id(Vui *vui, u64 id, const char *text);
void vui_label_ex_id(Vui *vui, u64 id, const char *text, f32 padding,
                     VuiHAlign h_align, VuiVAlign v_align);

// Constraints for the following element
void vui_cons_fill(Vui *vui); // Default
void vui_cons_fixed(Vui *vui, f32 value);

void vui_bg_color(Vui *vui, f32 r, f32 g, f32 b, f32 a);

// These probably will be called for you by a backend
void vui_on_mouse_move(Vui *vui, f32 x, f32 y);
void vui_on_mouse_down(Vui *vui);
void vui_on_mouse_up(Vui *vui);

#endif // VUI_H
