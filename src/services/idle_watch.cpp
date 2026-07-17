#include "reach/services/idle_watch.h"

#include <string.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

static const uint64_t REACH_IDLE_WATCH_POLL_MILLISECONDS = 30000;

void reach_idle_watch_state_init(reach_idle_watch_state *state, uint64_t now_milliseconds)
{
    if (state == nullptr)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->baseline_milliseconds = now_milliseconds;
    state->last_sample_milliseconds = now_milliseconds;
}

uint32_t reach_idle_watch_evaluate(reach_idle_watch_state *state,
                                   const reach_idle_watch_config *config,
                                   const reach_idle_watch_sample *sample,
                                   uint64_t poll_interval_milliseconds)
{
    if (state == nullptr || config == nullptr || sample == nullptr)
    {
        return 0;
    }

    uint64_t now = sample->now_milliseconds;
    if (now - state->last_sample_milliseconds > poll_interval_milliseconds * 3)
    {
        state->baseline_milliseconds = now;
    }
    state->last_sample_milliseconds = now;

    uint64_t since_baseline = now >= state->baseline_milliseconds
                                  ? now - state->baseline_milliseconds
                                  : 0;
    uint64_t idle = sample->input_idle_milliseconds < since_baseline
                        ? sample->input_idle_milliseconds
                        : since_baseline;

    if (idle < state->last_idle_milliseconds)
    {
        memset(state->fired, 0, sizeof(state->fired));
    }
    state->last_idle_milliseconds = idle;

    uint32_t mask = 0;
    for (size_t action = 0; action < REACH_IDLE_WATCH_ACTION_COUNT; ++action)
    {
        int32_t minutes = config->timeout_minutes[action];
        if (minutes <= 0 || state->fired[action])
        {
            continue;
        }
        uint64_t threshold = (uint64_t)minutes * 60000u;
        if (idle < threshold)
        {
            continue;
        }
        if (config->wait_awake_apps[action] && sample->awake_required)
        {
            continue;
        }
        state->fired[action] = 1;
        mask |= 1u << action;
    }
    return mask;
}

struct reach_idle_watch
{
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    reach_power_session_port power_session = {};
    reach_idle_watch_config config = {};
    reach_idle_watch_state state = {};
    int32_t stop = 0;
    int32_t started = 0;
};

static int32_t reach_idle_watch_any_timer(const reach_idle_watch_config *config)
{
    for (size_t action = 0; action < REACH_IDLE_WATCH_ACTION_COUNT; ++action)
    {
        if (config->timeout_minutes[action] > 0)
        {
            return 1;
        }
    }
    return 0;
}

static void reach_idle_watch_fire(reach_idle_watch *service, uint32_t mask)
{
    reach_power_session *session = service->power_session.session;
    const reach_power_session_ops *ops = &service->power_session.ops;
    if (mask & (1u << REACH_IDLE_WATCH_ACTION_SHUTDOWN))
    {
        if (ops->shutdown != nullptr)
        {
            (void)ops->shutdown(session);
        }
        return;
    }
    if (mask & (1u << REACH_IDLE_WATCH_ACTION_RESTART))
    {
        if (ops->restart != nullptr)
        {
            (void)ops->restart(session);
        }
        return;
    }
    if ((mask & (1u << REACH_IDLE_WATCH_ACTION_LOCK)) && ops->lock != nullptr)
    {
        (void)ops->lock(session);
    }
    if ((mask & (1u << REACH_IDLE_WATCH_ACTION_SLEEP)) && ops->sleep != nullptr)
    {
        (void)ops->sleep(session);
    }
}

static void reach_idle_watch_worker_main(reach_idle_watch *service)
{
    for (;;)
    {
        reach_idle_watch_config config = {};
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait_for(lock,
                                 std::chrono::milliseconds(REACH_IDLE_WATCH_POLL_MILLISECONDS),
                                 [service] { return service->stop != 0; });
            if (service->stop)
            {
                return;
            }
            config = service->config;
        }

        const reach_power_session_ops *ops = &service->power_session.ops;
        if (ops->now_milliseconds == nullptr || ops->input_idle_milliseconds == nullptr)
        {
            continue;
        }
        uint64_t now = ops->now_milliseconds(service->power_session.session);
        if (!reach_idle_watch_any_timer(&config))
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->state.last_sample_milliseconds = now;
            service->state.baseline_milliseconds = now;
            service->state.last_idle_milliseconds = 0;
            continue;
        }

        reach_idle_watch_sample sample = {};
        sample.now_milliseconds = now;
        if (ops->input_idle_milliseconds(service->power_session.session,
                                         &sample.input_idle_milliseconds) != REACH_OK)
        {
            continue;
        }
        if (ops->system_awake_required != nullptr &&
            ops->system_awake_required(service->power_session.session, &sample.awake_required) !=
                REACH_OK)
        {
            sample.awake_required = 0;
        }

        uint32_t mask = 0;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            mask = reach_idle_watch_evaluate(&service->state, &config, &sample,
                                             REACH_IDLE_WATCH_POLL_MILLISECONDS);
        }
        if (mask != 0)
        {
            reach_idle_watch_fire(service, mask);
        }
    }
}

reach_result reach_idle_watch_create(reach_power_session_port power_session,
                                     reach_idle_watch **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_service = nullptr;

    reach_idle_watch *service = new (std::nothrow) reach_idle_watch();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->power_session = power_session;
    uint64_t now = power_session.ops.now_milliseconds != nullptr
                       ? power_session.ops.now_milliseconds(power_session.session)
                       : 0;
    reach_idle_watch_state_init(&service->state, now);
    service->worker = std::thread(reach_idle_watch_worker_main, service);
    service->started = 1;
    *out_service = service;
    return REACH_OK;
}

void reach_idle_watch_set_config(reach_idle_watch *service, const reach_idle_watch_config *config)
{
    if (service == nullptr || config == nullptr)
    {
        return;
    }
    std::unique_lock<std::mutex> lock(service->mutex);
    service->config = *config;
    memset(service->state.fired, 0, sizeof(service->state.fired));
}

void reach_idle_watch_stop(reach_idle_watch *service)
{
    if (service == nullptr || !service->started)
    {
        return;
    }
    {
        std::unique_lock<std::mutex> lock(service->mutex);
        service->stop = 1;
    }
    service->cv.notify_all();
    if (service->worker.joinable())
    {
        service->worker.join();
    }
    service->started = 0;
}

void reach_idle_watch_destroy(reach_idle_watch *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_idle_watch_stop(service);
    delete service;
}
