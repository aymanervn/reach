#include "reach/services/profile.h"

#include "reach/support/util.h"

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

struct reach_profile_service
{
    reach_user_account_port account;
    reach_image_loader_port images;
    void (*notify)(void *user);
    void *notify_user;

    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t stop = 0;
    int32_t requested = 0;
    uint16_t request_path[REACH_PROFILE_PATH_CAPACITY] = {};
    int32_t request_width = 0;
    int32_t request_height = 0;

    uint16_t loaded_path[REACH_PROFILE_PATH_CAPACITY] = {};
    int32_t loaded_width = 0;
    int32_t loaded_height = 0;

    reach_profile_snapshot snapshot = {};
    int32_t published = 0;
    std::vector<uint64_t> retired;
};

static void reach_profile_service_publish(reach_profile_service *service,
                                          const reach_profile_snapshot *snapshot,
                                          uint64_t retired_image_id)
{
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->snapshot = *snapshot;
        service->published = 1;
        if (retired_image_id != 0)
        {
            service->retired.push_back(retired_image_id);
        }
    }
    if (service->notify != nullptr)
    {
        service->notify(service->notify_user);
    }
}

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
    reach_profile_service_publish(service, &current, 0);

    for (;;)
    {
        uint16_t path[REACH_PROFILE_PATH_CAPACITY] = {};
        int32_t width = 0;
        int32_t height = 0;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock, [service] { return service->stop || service->requested; });
            if (service->stop)
            {
                return;
            }
            service->requested = 0;
            reach_copy_utf16(path, REACH_PROFILE_PATH_CAPACITY, service->request_path);
            width = service->request_width;
            height = service->request_height;
        }
        if (reach_utf16_equal(path, service->loaded_path) && width == service->loaded_width &&
            height == service->loaded_height)
        {
            continue;
        }
        uint64_t image_id = 0;
        if (path[0] != 0 && service->images.ops.load != nullptr &&
            service->images.ops.load(service->images.loader, path, width, height, &image_id) !=
                REACH_OK)
        {
            image_id = 0;
        }
        reach_copy_utf16(service->loaded_path, REACH_PROFILE_PATH_CAPACITY, path);
        service->loaded_width = width;
        service->loaded_height = height;
        uint64_t retired = current.wallpaper_image_id;
        current.wallpaper_image_id = image_id;
        reach_profile_service_publish(service, &current, retired);
    }
}

reach_result reach_profile_service_create(reach_user_account_port account,
                                          const reach_image_loader_port *images,
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
    service->images = images != nullptr ? *images : reach_image_loader_port{};
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
    if (service == nullptr)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop = 1;
    }
    service->cv.notify_all();
    if (service->thread.joinable())
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
    for (uint64_t image_id : service->retired)
    {
        reach_profile_service_release_image(service, image_id);
    }
    reach_profile_service_release_image(service, service->snapshot.wallpaper_image_id);
    if (service->account.ops.destroy != nullptr)
    {
        service->account.ops.destroy(service->account.account);
    }
    delete service;
}

void reach_profile_service_request(reach_profile_service *service, const uint16_t *wallpaper_path,
                                   int32_t width_px, int32_t height_px)
{
    if (service == nullptr)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        reach_copy_utf16(service->request_path, REACH_PROFILE_PATH_CAPACITY,
                         wallpaper_path != nullptr ? wallpaper_path : (const uint16_t *)u"");
        service->request_width = width_px;
        service->request_height = height_px;
        service->requested = 1;
    }
    service->cv.notify_all();
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

int32_t reach_profile_service_take_retired_image(reach_profile_service *service,
                                                 uint64_t *out_image_id)
{
    if (service == nullptr || out_image_id == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    if (service->retired.empty())
    {
        return 0;
    }
    *out_image_id = service->retired.back();
    service->retired.pop_back();
    return 1;
}

void reach_profile_service_release_image(reach_profile_service *service, uint64_t image_id)
{
    if (service != nullptr && image_id != 0 && service->images.ops.release != nullptr)
    {
        service->images.ops.release(service->images.loader, image_id);
    }
}
