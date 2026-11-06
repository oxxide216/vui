#include "vui/vui.h"

bool vui_init(Vui *vui, VuiBackend *backend) {
  vui->backend = backend;

  return true;
}

void vui_destroy(Vui *vui) {
  vui->backend->destroy(vui->backend->data);
}
