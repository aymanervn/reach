#ifndef REACH_ADAPTERS_WINDOWS_MOUSE_HOOK_THREAD_WIN32_H
#define REACH_ADAPTERS_WINDOWS_MOUSE_HOOK_THREAD_WIN32_H

#include "reach/support/util.h"

#include <windows.h>

/* Low-level mouse hooks are dispatched on the thread that installed them, and Windows drops a
   packet the thread does not service within LowLevelHooksTimeout (300 ms by default). Hooking
   from the UI thread therefore makes every mouse event in the session wait behind whatever
   that thread is doing. These helpers run the hooks on a dedicated thread that only pumps
   messages, so a hook proc must stay cheap but no longer competes with the frame loop.

   Install and remove are synchronous, run the Win32 calls on the owning thread (required for
   UnhookWindowsHookEx), and must be called from one thread. Never hold a lock a hook proc can
   take across them. */

typedef struct reach_mouse_hook reach_mouse_hook;

reach_result reach_windows_install_mouse_hook(HOOKPROC proc, reach_mouse_hook **out_hook);

void reach_windows_remove_mouse_hook(reach_mouse_hook *hook);

#endif
