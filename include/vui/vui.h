#ifndef VUI_H
#define VUI_H

#include "shl/shl-defs.h"

typedef struct VuiBackendData VuiBackendData;

typedef void (*VuiBackendDestroyFunc)(VuiBackendData *data);

typedef struct {
  VuiBackendData        *data;
  VuiBackendDestroyFunc  destroy;
} VuiBackend;

typedef struct {
  VuiBackend *backend;
} Vui;

bool vui_init(Vui *vui, VuiBackend *backend);
void vui_destroy(Vui *vui);

#endif // VUI_H
