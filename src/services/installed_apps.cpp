#include "reach/services/installed_apps.h"

#include "reach/support/util.h"

#include <condition_variable>
#include <mutex>
#include <memory>
#include <new>
#include <thread>

// WIP: Keep Open and Options backend paths wired while Settings exposes only Uninstall.

struct reach_installed_apps_service
{
    reach_installed_apps_port port;
    reach_app_launcher_port launcher;
    void (*notify)(void *user);
    void *notify_user;
    std::thread thread;
    mutable std::mutex mutex;
    std::condition_variable cv;
    int32_t stop = 0;
    int32_t pending = 0;
    int32_t in_flight = 0;
    int32_t completed = 0;
    reach_installed_apps_command pending_command = REACH_INSTALLED_APPS_COMMAND_NONE;
    size_t pending_index = 0;
    reach_installed_apps_snapshot snapshot = {};
};

static void reach_installed_apps_keep_manageable_packages(reach_installed_app_list *apps)
{
    if (apps == nullptr)
    {
        return;
    }
    size_t write = 0;
    for (size_t read = 0; read < apps->count; ++read)
    {
        const reach_installed_app *entry = &apps->entries[read];
        if (entry->kind == REACH_INSTALLED_APP_PACKAGED && entry->can_uninstall)
        {
            apps->entries[write++] = *entry;
        }
    }
    apps->count = write;
}

static void reach_installed_apps_service_thread_main(reach_installed_apps_service *service)
{
    if (service->port.ops.thread_attach != nullptr)
    {
        service->port.ops.thread_attach(service->port.apps);
    }

    for (;;)
    {
        reach_installed_apps_command command = REACH_INSTALLED_APPS_COMMAND_NONE;
        size_t index = 0;
        reach_installed_app entry = {};
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock, [service]() { return service->stop || service->pending; });
            if (service->stop)
            {
                break;
            }
            command = service->pending_command;
            index = service->pending_index;
            if (index < service->snapshot.apps.count)
            {
                entry = service->snapshot.apps.entries[index];
            }
            service->pending = 0;
            service->in_flight = 1;
        }

        int32_t succeeded = 1;
        if (command == REACH_INSTALLED_APPS_COMMAND_OPEN)
        {
            reach_app_launch_request request = {};
            reach_copy_utf16(request.path, 260, entry.launch_path);
            reach_copy_utf16(request.app_user_model_id, 260, entry.app_user_model_id);
            succeeded = index < REACH_INSTALLED_APP_MAX_ENTRIES &&
                        service->launcher.ops.launch != nullptr &&
                        service->launcher.ops.launch(service->launcher.launcher, &request,
                                                     nullptr) == REACH_OK;
        }
        else if (command == REACH_INSTALLED_APPS_COMMAND_UNINSTALL)
        {
            succeeded = index < REACH_INSTALLED_APP_MAX_ENTRIES &&
                        service->port.ops.uninstall != nullptr &&
                        service->port.ops.uninstall(service->port.apps, &entry) == REACH_OK;
        }
        else if (command == REACH_INSTALLED_APPS_COMMAND_MANAGE)
        {
            succeeded = index < REACH_INSTALLED_APP_MAX_ENTRIES &&
                        service->port.ops.manage != nullptr &&
                        service->port.ops.manage(service->port.apps, &entry) == REACH_OK;
        }

        std::unique_ptr<reach_installed_app_list> apps(new (std::nothrow)
                                                           reach_installed_app_list());
        if (apps == nullptr || service->port.ops.enumerate == nullptr ||
            service->port.ops.enumerate(service->port.apps, apps.get()) != REACH_OK)
        {
            succeeded = command == REACH_INSTALLED_APPS_COMMAND_REFRESH ? 0 : succeeded;
        }
        else
        {
            reach_installed_apps_keep_manageable_packages(apps.get());
        }

        void (*notify)(void *) = nullptr;
        void *notify_user = nullptr;
        {
            std::lock_guard<std::mutex> lock(service->mutex);
            if (apps != nullptr)
            {
                service->snapshot.apps = *apps;
            }
            else
            {
                memset(&service->snapshot.apps, 0, sizeof(service->snapshot.apps));
            }
            service->snapshot.completed_command = command;
            service->snapshot.completed_index = index;
            service->snapshot.command_succeeded = succeeded;
            service->completed = 1;
            service->in_flight = 0;
            notify = service->notify;
            notify_user = service->notify_user;
        }
        if (notify != nullptr)
        {
            notify(notify_user);
        }
    }

    if (service->port.ops.thread_detach != nullptr)
    {
        service->port.ops.thread_detach(service->port.apps);
    }
}

reach_result reach_installed_apps_service_create(reach_installed_apps_port port,
                                                 reach_app_launcher_port launcher,
                                                 void (*notify)(void *user), void *notify_user,
                                                 reach_installed_apps_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_service = nullptr;
    reach_installed_apps_service *service = new (std::nothrow) reach_installed_apps_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->port = port;
    service->launcher = launcher;
    service->notify = notify;
    service->notify_user = notify_user;
    try
    {
        service->thread = std::thread(reach_installed_apps_service_thread_main, service);
    }
    catch (...)
    {
        delete service;
        return REACH_ERROR;
    }
    *out_service = service;
    return REACH_OK;
}

void reach_installed_apps_service_destroy(reach_installed_apps_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop = 1;
        service->pending = 0;
    }
    service->cv.notify_one();
    if (service->thread.joinable())
    {
        service->thread.join();
    }
    if (service->port.ops.destroy != nullptr)
    {
        service->port.ops.destroy(service->port.apps);
    }
    if (service->launcher.ops.destroy != nullptr)
    {
        service->launcher.ops.destroy(service->launcher.launcher);
    }
    delete service;
}

static void reach_installed_apps_service_submit(reach_installed_apps_service *service,
                                                reach_installed_apps_command command, size_t index)
{
    if (service == nullptr)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        if (service->pending || service->in_flight)
        {
            return;
        }
        service->pending_command = command;
        service->pending_index = index;
        service->pending = 1;
    }
    service->cv.notify_one();
}

void reach_installed_apps_service_refresh(reach_installed_apps_service *service)
{
    reach_installed_apps_service_submit(service, REACH_INSTALLED_APPS_COMMAND_REFRESH, 0);
}

void reach_installed_apps_service_open(reach_installed_apps_service *service, size_t index)
{
    reach_installed_apps_service_submit(service, REACH_INSTALLED_APPS_COMMAND_OPEN, index);
}

void reach_installed_apps_service_uninstall(reach_installed_apps_service *service, size_t index)
{
    reach_installed_apps_service_submit(service, REACH_INSTALLED_APPS_COMMAND_UNINSTALL, index);
}

void reach_installed_apps_service_manage(reach_installed_apps_service *service, size_t index)
{
    reach_installed_apps_service_submit(service, REACH_INSTALLED_APPS_COMMAND_MANAGE, index);
}

int32_t reach_installed_apps_service_take(reach_installed_apps_service *service,
                                          reach_installed_apps_snapshot *out_snapshot)
{
    if (service == nullptr || out_snapshot == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    if (!service->completed)
    {
        return 0;
    }
    *out_snapshot = service->snapshot;
    service->completed = 0;
    return 1;
}

int32_t reach_installed_apps_service_pending(const reach_installed_apps_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    return service->pending || service->in_flight || service->completed;
}
