#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "shl/shl-log.h"
#include "winx/winx.h"
#include "winx/event.h"
#include "viking/viking.h"
#include "vui/vui.h"
#include "vui/vui-winx-vulkan.h"

i32 main(void) {
  Winx *winx = winx_init();
  WinxWindow *window = winx_init_window(winx, STR_LIT("VUI Example"),
                                        1600, 900,
                                        WinxGraphicsModeVulkan,
                                        NULL);
  window->target_fps = winx_get_refresh_rate(window);

  VikInstance *instance = vik_make_instance(window, VikRequestFlagsNone, false);
  VikExecutor *executor = vik_make_executor(instance);

  VuiBackend vui_backend = {0};
  if (!vui_winx_vulkan_backend_init(&vui_backend, window,
                                    instance, executor)) {
    ERROR("Could not initialize VUI backend\n");
    return 1;
  }


  Vui vui = {0};
  if (!vui_init(&vui, &vui_backend)) {
    ERROR("Could not initialize VUI\n");
    return 1;
  }

  bool is_running = true;

  while (is_running) {
    WinxEvent event;
    while ((event = winx_get_event(window, false)).kind != WinxEventKindNone) {
      is_running = event.kind != WinxEventKindQuit;
      vui_winx_vulkan_process_event(&vui, &event);
      if (!is_running)
        break;
    }

    if (!vik_begin_frame(executor, 0.0, 0.0, 0.0, 1.0)) {
      winx_draw(window);
      continue;
    }

    vui_begin_frame(&vui);
      vui_cons_fixed(&vui, 50.0);
      vui_label(&vui, "Example App");
      vui_layout_begin_vert(&vui, 0.0, 10.0);
        if (vui_button(&vui, "0"))
          puts("0");
        if (vui_button(&vui, "1"))
          puts("1");
      vui_layout_end(&vui);
      vui_cons_fixed(&vui, 50.0);
      vui_layout_begin_horz(&vui, 0.0, 5.0);
        if (vui_button(&vui, "2"))
          puts("2");
        if (vui_button(&vui, "3"))
          puts("3");
      vui_layout_end(&vui);
    vui_end_frame(&vui);

    vik_end_frame(executor);

    winx_draw(window);
  }

  vui_destroy(&vui);

  vik_delete_executor(executor);
  vik_delete_instance(instance);

  winx_destroy_window(window);
  winx_cleanup(winx);

  return 0;
}
