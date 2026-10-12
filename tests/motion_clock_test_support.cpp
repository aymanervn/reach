#include "motion_clock_test_support.h"

#include "reach/support/motion.h"

#include <atomic>

static std::atomic<double> test_seconds{1000};

double reach_motion_now_seconds(void)
{
    return test_seconds.load(std::memory_order_relaxed);
}

void reach_test_motion_clock_advance(double seconds)
{
    if (seconds > 0)
        test_seconds.store(test_seconds.load(std::memory_order_relaxed) + seconds,
                           std::memory_order_relaxed);
}
