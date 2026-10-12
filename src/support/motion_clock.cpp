#include "reach/support/motion.h"

#include <chrono>

double reach_motion_now_seconds(void)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
