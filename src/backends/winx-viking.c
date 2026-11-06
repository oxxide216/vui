#include "vui/vui-winx-vulkan.h"

struct VuiBackendData {
  WinxWindow  *window;
  VikInstance *instance;
  VikExecutor *executor;
};

static void destroy(VuiBackendData *data) {
  free(data);
}

bool vui_winx_vulkan_backend_init(VuiBackend *backend,
                                  WinxWindow *window,
                                  VikInstance *instance,
                                  VikExecutor *executor) {
  backend->data = malloc(sizeof(*backend->data));
  backend->data->window = window;
  backend->data->instance = instance;
  backend->data->executor = executor;

  backend->destroy = destroy;

  return true;
}
