#include "reach/adapters/windows/frame_wait.h"

#include <windows.h>
#include <math.h>

void *reach_windows_frame_timer_create(void)
{
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_MODIFY_STATE | SYNCHRONIZE);
    return timer != nullptr ? timer : CreateWaitableTimerW(nullptr, FALSE, nullptr);
}

void reach_windows_frame_timer_destroy(void *timer)
{
    if (timer != nullptr)
    {
        CloseHandle(timer);
    }
}

uint32_t reach_windows_frame_wait(void *timer, void *const *handles, uint32_t handle_count,
                                  uint32_t timeout_ms, double frame_deadline_seconds)
{
    if (timer != nullptr && frame_deadline_seconds >= 0.0 &&
        handle_count < MAXIMUM_WAIT_OBJECTS - 1)
    {
        LARGE_INTEGER counter = {};
        LARGE_INTEGER frequency = {};
        QueryPerformanceCounter(&counter);
        QueryPerformanceFrequency(&frequency);
        double remaining = frame_deadline_seconds - (double)counter.QuadPart / frequency.QuadPart;
        if (remaining <= 0.0)
        {
            return MsgWaitForMultipleObjectsEx(handle_count, handles, 0, QS_ALLINPUT,
                                               MWMO_INPUTAVAILABLE);
        }
        LARGE_INTEGER due = {};
        due.QuadPart = -(LONGLONG)ceil(remaining * 10000000.0);
        if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
        {
            HANDLE wait_handles[MAXIMUM_WAIT_OBJECTS] = {};
            for (uint32_t index = 0; index < handle_count; ++index)
            {
                wait_handles[index] = handles[index];
            }
            wait_handles[handle_count] = timer;
            DWORD result = MsgWaitForMultipleObjectsEx(handle_count + 1, wait_handles, INFINITE,
                                                       QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (result == WAIT_OBJECT_0 + handle_count)
            {
                return WAIT_TIMEOUT;
            }
            return result == WAIT_OBJECT_0 + handle_count + 1 ? WAIT_OBJECT_0 + handle_count
                                                              : result;
        }
    }
    if (timer != nullptr)
    {
        (void)CancelWaitableTimer(timer);
    }
    return MsgWaitForMultipleObjectsEx(handle_count, handles, timeout_ms, QS_ALLINPUT,
                                       MWMO_INPUTAVAILABLE);
}
