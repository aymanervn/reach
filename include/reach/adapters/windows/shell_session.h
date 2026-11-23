#ifndef REACH_ADAPTERS_WINDOWS_SHELL_SESSION_H
#define REACH_ADAPTERS_WINDOWS_SHELL_SESSION_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Windows restarts the session shell (userinit, then the logon-time Shell command) whenever
    // the first process to register a shell window exits, for any reason. Reach relies on that
    // as its only supervisor, so it claims the shell window before anything else can fail.

    // Persists this start and reports whether the recent starts form a crash loop.
    int32_t reach_windows_shell_session_record_start(void);

    // Registers a placeholder shell window so this process is the one Windows supervises.
    reach_result reach_windows_shell_session_claim(void);

    // Hands the shell role to the desktop window. Callable from any thread; destroying the
    // placeholder does not end supervision, which belongs to the process.
    reach_result reach_windows_shell_session_register(void *hwnd);

    // Permanent fallback: makes Explorer the shell for this and later sessions, starts it and
    // shows the failure notice. Blocks until the notice is dismissed or times out.
    void reach_windows_shell_session_fall_back(void);

#ifdef __cplusplus
}
#endif

#endif
