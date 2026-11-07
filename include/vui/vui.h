#ifndef VUI_H
#define VUI_H

#include "shl/shl-defs.h"

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
  VuiWidgetKindButton = 0,
  VuiWidgetKindsCount,
} VuiWidgetKind;

typedef struct {
  f32  x, y, w, h;
  bool is_released;
} VuiWidgetState;

typedef struct {
  Da(VuiWidgetState) states;
  u32                 pos;
} VuiWidgetStackItem;

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
  f32           padding;
  f32           x, y, w, h;
} VuiLayout;

typedef Da(VuiLayout) VuiLayouts;

typedef struct {
  VuiBackend     *backend;
  VuiWidgetStack  stack;
  u32             depth;
  VuiLayouts      layouts;
} Vui;

bool vui_init(Vui *vui, VuiBackend *backend);
void vui_begin_frame(Vui *vui);
void vui_end_frame(Vui *vui);
void vui_destroy(Vui *vui);

bool vui_button(Vui *vui, const char *text);

#endif // VUI_H
