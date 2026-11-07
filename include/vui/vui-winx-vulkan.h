#ifndef VUI_WINX_VULKAN_H
#define VUI_WINX_VULKAN_H

#include "vui.h"
#include "winx/winx.h"
#include "winx/event.h"
#include "viking/viking.h"

bool vui_winx_vulkan_backend_init(VuiBackend *backend,
                                  WinxWindow *window,
                                  VikInstance *instance,
                                  VikExecutor *executor);
bool vui_winx_vulkan_process_event(Vui *vui, WinxEvent *event);

#endif // VUI_WINX_VULKAN_H
