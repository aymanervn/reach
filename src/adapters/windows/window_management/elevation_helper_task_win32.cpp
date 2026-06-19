#include "elevation_helper_task_win32.h"

#include <windows.h>
#include <sddl.h>
#include <taskschd.h>

#include <vector>

static const wchar_t *REACH_ELEVATION_HELPER_TASK_NAME = L"reach-helper";

struct reach_task_scheduler_session
{
    ITaskService *service;
    ITaskFolder *root;
    int32_t com_initialized;
};

static void reach_task_scheduler_close(reach_task_scheduler_session *session)
{
    if (session == nullptr)
    {
        return;
    }
    if (session->root != nullptr)
    {
        session->root->Release();
    }
    if (session->service != nullptr)
    {
        session->service->Release();
    }
    if (session->com_initialized)
    {
        CoUninitialize();
    }
    *session = {};
}

static reach_result reach_task_scheduler_open(reach_task_scheduler_session *session)
{
    if (session == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *session = {};

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        session->com_initialized = 1;
    }
    else if (hr != RPC_E_CHANGED_MODE)
    {
        return REACH_ERROR;
    }

    hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService,
                          reinterpret_cast<void **>(&session->service));
    if (FAILED(hr) || session->service == nullptr)
    {
        reach_task_scheduler_close(session);
        return REACH_ERROR;
    }

    VARIANT empty = {};
    VariantInit(&empty);
    hr = session->service->Connect(empty, empty, empty, empty);
    if (FAILED(hr))
    {
        reach_task_scheduler_close(session);
        return REACH_ERROR;
    }

    BSTR root_path = SysAllocString(L"\\");
    if (root_path == nullptr)
    {
        reach_task_scheduler_close(session);
        return REACH_ERROR;
    }
    hr = session->service->GetFolder(root_path, &session->root);
    SysFreeString(root_path);
    if (FAILED(hr) || session->root == nullptr)
    {
        reach_task_scheduler_close(session);
        return REACH_ERROR;
    }
    return REACH_OK;
}

reach_result reach_elevation_helper_current_user_id(wchar_t *user_id, size_t user_id_count)
{
    if (user_id == nullptr || user_id_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    user_id[0] = 0;

    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return REACH_ERROR;
    }

    DWORD needed = 0;
    (void)GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    std::vector<BYTE> buffer(needed);
    int32_t ok =
        needed > 0 && GetTokenInformation(token, TokenUser, buffer.data(), needed, &needed);
    CloseHandle(token);
    if (!ok)
    {
        return REACH_ERROR;
    }

    TOKEN_USER *token_user = reinterpret_cast<TOKEN_USER *>(buffer.data());
    wchar_t *sid = nullptr;
    if (token_user == nullptr || token_user->User.Sid == nullptr ||
        !ConvertSidToStringSidW(token_user->User.Sid, &sid))
    {
        return REACH_ERROR;
    }

    size_t length = wcslen(sid);
    if (length + 1 > user_id_count)
    {
        LocalFree(sid);
        return REACH_ERROR;
    }
    wcscpy_s(user_id, user_id_count, sid);
    LocalFree(sid);
    return REACH_OK;
}

static int32_t reach_elevation_helper_registered_task_valid(IRegisteredTask *task,
                                                            const wchar_t *helper_path,
                                                            const wchar_t *user_id)
{
    if (task == nullptr || helper_path == nullptr || helper_path[0] == 0 || user_id == nullptr ||
        user_id[0] == 0)
    {
        return 0;
    }

    ITaskDefinition *definition = nullptr;
    IPrincipal *principal = nullptr;
    ITriggerCollection *triggers = nullptr;
    IActionCollection *actions = nullptr;
    IAction *action = nullptr;
    IExecAction *exec = nullptr;
    BSTR action_path = nullptr;
    BSTR principal_user_id = nullptr;
    TASK_RUNLEVEL_TYPE run_level = TASK_RUNLEVEL_LUA;
    TASK_LOGON_TYPE logon_type = TASK_LOGON_NONE;
    LONG trigger_count = 0;
    LONG action_count = 0;

    HRESULT hr = task->get_Definition(&definition);
    if (SUCCEEDED(hr))
    {
        hr = definition->get_Principal(&principal);
    }
    if (SUCCEEDED(hr))
    {
        hr = principal->get_RunLevel(&run_level);
    }
    if (SUCCEEDED(hr))
    {
        hr = principal->get_UserId(&principal_user_id);
    }
    if (SUCCEEDED(hr))
    {
        hr = principal->get_LogonType(&logon_type);
    }
    if (SUCCEEDED(hr))
    {
        hr = definition->get_Triggers(&triggers);
    }
    if (SUCCEEDED(hr))
    {
        hr = triggers->get_Count(&trigger_count);
    }
    if (SUCCEEDED(hr))
    {
        hr = definition->get_Actions(&actions);
    }
    if (SUCCEEDED(hr))
    {
        hr = actions->get_Count(&action_count);
    }
    if (SUCCEEDED(hr) && action_count == 1)
    {
        hr = actions->get_Item(1, &action);
    }
    else if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        hr = action->QueryInterface(IID_IExecAction, reinterpret_cast<void **>(&exec));
    }
    if (SUCCEEDED(hr))
    {
        hr = exec->get_Path(&action_path);
    }

    int32_t valid = SUCCEEDED(hr) && trigger_count == 0 && run_level == TASK_RUNLEVEL_HIGHEST &&
                    logon_type == TASK_LOGON_INTERACTIVE_TOKEN && principal_user_id != nullptr &&
                    _wcsicmp(principal_user_id, user_id) == 0 && action_path != nullptr &&
                    _wcsicmp(action_path, helper_path) == 0;

    SysFreeString(action_path);
    SysFreeString(principal_user_id);
    if (exec != nullptr)
    {
        exec->Release();
    }
    if (action != nullptr)
    {
        action->Release();
    }
    if (actions != nullptr)
    {
        actions->Release();
    }
    if (triggers != nullptr)
    {
        triggers->Release();
    }
    if (principal != nullptr)
    {
        principal->Release();
    }
    if (definition != nullptr)
    {
        definition->Release();
    }
    return valid;
}

reach_result reach_elevation_helper_task_register(const wchar_t *helper_path,
                                                  const wchar_t *user_id)
{
    if (helper_path == nullptr || helper_path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_task_scheduler_session session = {};
    if (reach_task_scheduler_open(&session) != REACH_OK)
    {
        return REACH_ERROR;
    }

    ITaskDefinition *task = nullptr;
    HRESULT hr = session.service->NewTask(0, &task);
    if (FAILED(hr) || task == nullptr)
    {
        reach_task_scheduler_close(&session);
        return REACH_ERROR;
    }

    wchar_t current_user_id[192] = {};
    if ((user_id == nullptr || user_id[0] == 0) &&
        reach_elevation_helper_current_user_id(current_user_id, 192) != REACH_OK)
    {
        task->Release();
        reach_task_scheduler_close(&session);
        return REACH_ERROR;
    }
    const wchar_t *task_user_id = user_id != nullptr && user_id[0] != 0 ? user_id : current_user_id;

    IPrincipal *principal = nullptr;
    ITaskSettings *settings = nullptr;
    IActionCollection *actions = nullptr;
    IAction *action = nullptr;
    IExecAction *exec = nullptr;
    IRegisteredTask *registered = nullptr;
    VARIANT empty = {};
    VariantInit(&empty);

    hr = task->get_Principal(&principal);
    if (SUCCEEDED(hr))
    {
        BSTR principal_user_id = SysAllocString(task_user_id);
        if (principal_user_id == nullptr)
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            hr = principal->put_UserId(principal_user_id);
            SysFreeString(principal_user_id);
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    }
    if (SUCCEEDED(hr))
    {
        hr = principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
    }
    if (SUCCEEDED(hr))
    {
        hr = task->get_Settings(&settings);
    }
    if (SUCCEEDED(hr))
    {
        hr = settings->put_AllowDemandStart(VARIANT_TRUE);
    }
    if (SUCCEEDED(hr))
    {
        hr = settings->put_StartWhenAvailable(VARIANT_TRUE);
    }
    if (SUCCEEDED(hr))
    {
        hr = settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    }
    if (SUCCEEDED(hr))
    {
        hr = settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    }
    if (SUCCEEDED(hr))
    {
        hr = settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
    }
    if (SUCCEEDED(hr))
    {
        BSTR execution_limit = SysAllocString(L"PT0S");
        if (execution_limit == nullptr)
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            hr = settings->put_ExecutionTimeLimit(execution_limit);
            SysFreeString(execution_limit);
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = task->get_Actions(&actions);
    }
    if (SUCCEEDED(hr))
    {
        hr = actions->Create(TASK_ACTION_EXEC, &action);
    }
    if (SUCCEEDED(hr))
    {
        hr = action->QueryInterface(IID_IExecAction, reinterpret_cast<void **>(&exec));
    }
    if (SUCCEEDED(hr))
    {
        BSTR path = SysAllocString(helper_path);
        if (path == nullptr)
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            hr = exec->put_Path(path);
            SysFreeString(path);
        }
    }
    if (SUCCEEDED(hr))
    {
        BSTR task_name = SysAllocString(REACH_ELEVATION_HELPER_TASK_NAME);
        BSTR registered_user_id = SysAllocString(task_user_id);
        VARIANT user_variant = {};
        VARIANT password = {};
        VariantInit(&user_variant);
        VariantInit(&password);
        if (task_name == nullptr || registered_user_id == nullptr)
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            user_variant.vt = VT_BSTR;
            user_variant.bstrVal = registered_user_id;
            hr = session.root->RegisterTaskDefinition(
                task_name, task, TASK_CREATE_OR_UPDATE, user_variant, password,
                TASK_LOGON_INTERACTIVE_TOKEN, empty, &registered);
        }
        SysFreeString(task_name);
        SysFreeString(registered_user_id);
    }

    if (registered != nullptr)
    {
        registered->Release();
    }
    if (exec != nullptr)
    {
        exec->Release();
    }
    if (action != nullptr)
    {
        action->Release();
    }
    if (actions != nullptr)
    {
        actions->Release();
    }
    if (settings != nullptr)
    {
        settings->Release();
    }
    if (principal != nullptr)
    {
        principal->Release();
    }
    task->Release();
    reach_task_scheduler_close(&session);
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_elevation_helper_task_run(void)
{
    reach_task_scheduler_session session = {};
    if (reach_task_scheduler_open(&session) != REACH_OK)
    {
        return REACH_ERROR;
    }

    BSTR task_name = SysAllocString(REACH_ELEVATION_HELPER_TASK_NAME);
    IRegisteredTask *task = nullptr;
    HRESULT hr = task_name != nullptr ? session.root->GetTask(task_name, &task) : E_OUTOFMEMORY;
    SysFreeString(task_name);

    IRunningTask *running = nullptr;
    if (SUCCEEDED(hr) && task != nullptr)
    {
        VARIANT empty = {};
        VariantInit(&empty);
        hr = task->Run(empty, &running);
    }

    if (running != nullptr)
    {
        running->Release();
    }
    if (task != nullptr)
    {
        task->Release();
    }
    reach_task_scheduler_close(&session);
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

int32_t reach_elevation_helper_task_valid(const wchar_t *helper_path)
{
    if (helper_path == nullptr || helper_path[0] == 0)
    {
        return 0;
    }

    reach_task_scheduler_session session = {};
    if (reach_task_scheduler_open(&session) != REACH_OK)
    {
        return 0;
    }

    BSTR task_name = SysAllocString(REACH_ELEVATION_HELPER_TASK_NAME);
    IRegisteredTask *task = nullptr;
    HRESULT hr = task_name != nullptr ? session.root->GetTask(task_name, &task) : E_OUTOFMEMORY;
    SysFreeString(task_name);
    wchar_t user_id[192] = {};
    int32_t valid = SUCCEEDED(hr) &&
                    reach_elevation_helper_current_user_id(user_id, 192) == REACH_OK &&
                    reach_elevation_helper_registered_task_valid(task, helper_path, user_id);
    if (task != nullptr)
    {
        task->Release();
    }
    reach_task_scheduler_close(&session);
    return valid;
}

reach_result reach_elevation_helper_task_unregister(void)
{
    reach_task_scheduler_session session = {};
    if (reach_task_scheduler_open(&session) != REACH_OK)
    {
        return REACH_ERROR;
    }

    BSTR task_name = SysAllocString(REACH_ELEVATION_HELPER_TASK_NAME);
    HRESULT hr = task_name != nullptr ? session.root->DeleteTask(task_name, 0) : E_OUTOFMEMORY;
    SysFreeString(task_name);
    reach_task_scheduler_close(&session);
    return SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ? REACH_OK : REACH_ERROR;
}
