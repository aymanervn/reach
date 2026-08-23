#ifndef REACH_CORE_LIMITS_H
#define REACH_CORE_LIMITS_H

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_MAX_PINNED_APPS 96
#define REACH_MAX_OPEN_WINDOWS 96
#define REACH_MAX_DOCK_RUNNING_APPS 96
#define REACH_MAX_DOCK_ITEMS (REACH_MAX_PINNED_APPS + REACH_MAX_DOCK_RUNNING_APPS)

#ifdef __cplusplus
}
#endif

#endif
