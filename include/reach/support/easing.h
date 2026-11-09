#ifndef REACH_SUPPORT_EASING_H
#define REACH_SUPPORT_EASING_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_easing
    {
        REACH_EASING_EASE_IN = 0,
        REACH_EASING_EASE_OUT = 1,
        REACH_EASING_EASE_IN_OUT = 2,
        REACH_EASING_EXPONENTIAL_OUT = 3,
        REACH_EASING_SPRING = 4
    } reach_easing;

    float reach_easing_apply(float value, reach_easing easing);

#ifdef __cplusplus
}
#endif

#endif
