#include "reach/services/profile.h"

#include "reach/support/util.h"

#include <mutex>
#include <new>
#include <thread>

struct reach_profile_service
{
    reach_user_account_port account;
    void (*notify)(void *user);
    void *notify_user;

    std::thread thread;
    std::mutex mutex;
    reach_profile_snapshot snapshot = {};
    int32_t published = 0;
};

static void reach_profile_service_main(reach_profile_service *service)
{
    reach_profile_snapshot current = {};
    if (service->account.ops.query != nullptr)
    {
        reach_user_account_info info = {};
        if (service->account.ops.query(service->account.account, &info) == REACH_OK)
        {
            reach_copy_utf16(current.display_name, REACH_USER_ACCOUNT_NAME_CAPACITY,
                             info.display_name[0] != 0 ? info.display_name : info.user_name);
            current.picture_image_id = info.picture_icon_id;
        }
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->snapshot = current;
        service->published = 1;
    }
    if (service->notify != nullptr)
    {
        service->notify(service->notify_user);
    }
}

reach_result reach_profile_service_create(reach_user_account_port account,
                                          void (*notify)(void *user), void *notify_user,
                                          reach_profile_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_service = nullptr;
    reach_profile_service *service = new (std::nothrow) reach_profile_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->account = account;
    service->notify = notify;
    service->notify_user = notify_user;
    try
    {
        service->thread = std::thread(reach_profile_service_main, service);
    }
    catch (...)
    {
        delete service;
        return REACH_ERROR;
    }
    *out_service = service;
    return REACH_OK;
}

void reach_profile_service_stop(reach_profile_service *service)
{
    if (service != nullptr && service->thread.joinable())
    {
        service->thread.join();
    }
}

void reach_profile_service_destroy(reach_profile_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_profile_service_stop(service);
    if (service->account.ops.destroy != nullptr)
    {
        service->account.ops.destroy(service->account.account);
    }
    delete service;
}

int32_t reach_profile_service_take(reach_profile_service *service,
                                   reach_profile_snapshot *out_snapshot)
{
    if (service == nullptr || out_snapshot == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    if (!service->published)
    {
        return 0;
    }
    service->published = 0;
    *out_snapshot = service->snapshot;
    return 1;
}
