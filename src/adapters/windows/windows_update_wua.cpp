#include "windows_adapters_internal.h"

#include "reach/ports/windows_update.h"

#include <windows.h>
#include <shellapi.h>
#include <wuapi.h>
#include <wrl/client.h>

#include <atomic>
#include <memory>
#include <new>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

using Microsoft::WRL::ComPtr;

static const HRESULT REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED = (HRESULT)0x80240044L;
static const HRESULT REACH_WU_E_ALL_UPDATES_FAILED = (HRESULT)0x80240022L;
static const HRESULT REACH_WU_E_UH_POSTREBOOTSTILLPENDING = (HRESULT)0x80242014L;
static const HRESULT REACH_WU_E_OPERATION_IN_PROGRESS = (HRESULT)0x80240009L;
static const HRESULT REACH_WU_E_WU_DISABLED = (HRESULT)0x8024002EL;
static const HRESULT REACH_WU_E_SOURCE_ABSENT = (HRESULT)0x8024002CL;

struct reach_windows_update_adapter
{
    std::atomic<int32_t> cancel_requested;
};

struct bstr_scope
{
    BSTR value;

    bstr_scope() : value(nullptr)
    {
    }

    explicit bstr_scope(const wchar_t *text) : value(SysAllocString(text))
    {
    }

    ~bstr_scope()
    {
        if (value != nullptr)
            SysFreeString(value);
    }

    BSTR *put()
    {
        if (value != nullptr)
        {
            SysFreeString(value);
            value = nullptr;
        }
        return &value;
    }

    bstr_scope(const bstr_scope &) = delete;
    bstr_scope &operator=(const bstr_scope &) = delete;
};

struct com_scope
{
    HRESULT result;
    int32_t uninitialize;

    com_scope() : result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), uninitialize(0)
    {
        if (result == S_OK || result == S_FALSE)
            uninitialize = 1;
        else if (result == RPC_E_CHANGED_MODE)
            result = S_OK;
    }

    ~com_scope()
    {
        if (uninitialize)
            CoUninitialize();
    }
};

class search_completed_callback final : public ISearchCompletedCallback
{
  public:
    search_completed_callback()
        : references_(1), completed_(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    ~search_completed_callback()
    {
        if (completed_ != nullptr)
            CloseHandle(completed_);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
    {
        if (object == nullptr)
            return E_POINTER;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ISearchCompletedCallback)
        {
            *object = static_cast<ISearchCompletedCallback *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return (ULONG)references_.fetch_add(1) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG remaining = (ULONG)references_.fetch_sub(1) - 1;
        if (remaining == 0)
            delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ISearchJob *, ISearchCompletedCallbackArgs *) override
    {
        return completed_ != nullptr && SetEvent(completed_) ? S_OK
                                                             : HRESULT_FROM_WIN32(GetLastError());
    }

    HANDLE completed_event() const
    {
        return completed_;
    }

  private:
    std::atomic<ULONG> references_;
    HANDLE completed_;
};

static void copy_wide(uint16_t *destination, size_t capacity, const wchar_t *source)
{
    if (destination == nullptr || capacity == 0)
        return;

    size_t index = 0;
    while (source != nullptr && source[index] != 0 && index + 1 < capacity)
    {
        destination[index] = (uint16_t)source[index];
        ++index;
    }
    destination[index] = 0;
}

static void append_wide(uint16_t *destination, size_t capacity, const wchar_t *source)
{
    if (destination == nullptr || capacity == 0)
        return;

    size_t length = 0;
    while (length < capacity && destination[length] != 0)
        ++length;

    if (length >= capacity)
    {
        destination[capacity - 1] = 0;
        return;
    }

    size_t index = 0;
    while (source != nullptr && source[index] != 0 && length + 1 < capacity)
        destination[length++] = (uint16_t)source[index++];
    destination[length] = 0;
}

static void append_separator(uint16_t *destination, size_t capacity)
{
    if (destination != nullptr && capacity > 0 && destination[0] != 0)
        append_wide(destination, capacity, L", ");
}

static void set_utc_now(uint16_t *destination, size_t capacity)
{
    SYSTEMTIME time = {};
    GetSystemTime(&time);

    wchar_t text[32] = {};
    swprintf_s(text, L"%04u-%02u-%02uT%02u:%02u:%02uZ", time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond);
    copy_wide(destination, capacity, text);
}

static int32_t is_elevated(void)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return 0;

    TOKEN_ELEVATION elevation = {};
    DWORD size = 0;
    BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

static int32_t operation_succeeded(OperationResultCode code)
{
    return code == orcSucceeded || code == orcSucceededWithErrors;
}

static reach_windows_update_failure_class classify_hresult(HRESULT hr)
{
    if (hr == REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED)
        return REACH_WINDOWS_UPDATE_NOT_ELEVATED;
    if (hr == REACH_WU_E_ALL_UPDATES_FAILED)
        return REACH_WINDOWS_UPDATE_INSTALL_FAILED;
    if (hr == HRESULT_FROM_WIN32(ERROR_INSTALL_ALREADY_RUNNING) ||
        hr == REACH_WU_E_OPERATION_IN_PROGRESS)
        return REACH_WINDOWS_UPDATE_ANOTHER_OPERATION_IN_PROGRESS;
    if (hr == REACH_WU_E_WU_DISABLED || hr == REACH_WU_E_SOURCE_ABSENT)
        return REACH_WINDOWS_UPDATE_POLICY_BLOCKED;
    return REACH_WINDOWS_UPDATE_INSTALL_FAILED;
}

static HRESULT create_session_and_searcher(ComPtr<IUpdateSession> *session,
                                           ComPtr<IUpdateSearcher> *searcher)
{
    if (session == nullptr || searcher == nullptr)
        return E_INVALIDARG;

    session->Reset();
    searcher->Reset();

    HRESULT hr =
        CoCreateInstance(CLSID_UpdateSession, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                         IID_PPV_ARGS(session->ReleaseAndGetAddressOf()));
    if (FAILED(hr))
        return hr;
    if (session->Get() == nullptr)
        return E_POINTER;

    bstr_scope client_id(L"Reach Settings");
    if (client_id.value != nullptr)
        (void)(*session)->put_ClientApplicationID(client_id.value);

    hr = (*session)->CreateUpdateSearcher(searcher->ReleaseAndGetAddressOf());
    if (FAILED(hr))
        return hr;
    return searcher->Get() != nullptr ? S_OK : E_POINTER;
}

static HRESULT create_empty_update_collection(IUpdateCollection *source,
                                              ComPtr<IUpdateCollection> *out_collection)
{
    if (source == nullptr || out_collection == nullptr)
        return E_INVALIDARG;

    out_collection->Reset();
    HRESULT hr = source->Copy(out_collection->ReleaseAndGetAddressOf());
    if (FAILED(hr))
        return hr;
    if (out_collection->Get() == nullptr)
        return E_POINTER;
    return (*out_collection)->Clear();
}

static void read_string_collection(IStringCollection *collection, uint16_t *destination,
                                   size_t capacity)
{
    if (collection == nullptr || destination == nullptr || capacity == 0)
        return;

    LONG count = 0;
    if (FAILED(collection->get_Count(&count)))
        return;

    for (LONG index = 0; index < count; ++index)
    {
        bstr_scope value;
        if (SUCCEEDED(collection->get_Item(index, value.put())) && value.value != nullptr)
        {
            append_separator(destination, capacity);
            append_wide(destination, capacity, value.value);
        }
    }
}

static void read_categories(ICategoryCollection *categories, uint16_t *destination, size_t capacity)
{
    if (categories == nullptr || destination == nullptr || capacity == 0)
        return;

    LONG count = 0;
    if (FAILED(categories->get_Count(&count)))
        return;

    for (LONG index = 0; index < count; ++index)
    {
        ComPtr<ICategory> category;
        if (SUCCEEDED(categories->get_Item(index, category.GetAddressOf())) && category != nullptr)
        {
            bstr_scope name;
            if (SUCCEEDED(category->get_Name(name.put())) && name.value != nullptr)
            {
                append_separator(destination, capacity);
                append_wide(destination, capacity, name.value);
            }
        }
    }
}

static HRESULT read_update(IUpdate *update, reach_windows_update_item *item)
{
    if (update == nullptr || item == nullptr)
        return E_INVALIDARG;

    *item = {};

    bstr_scope title;
    if (SUCCEEDED(update->get_Title(title.put())) && title.value != nullptr)
        copy_wide(item->identity.title, REACH_WINDOWS_UPDATE_TEXT_CAPACITY, title.value);

    ComPtr<IUpdateIdentity> identity;
    HRESULT hr = update->get_Identity(identity.GetAddressOf());
    if (FAILED(hr))
        return hr;
    if (identity == nullptr)
        return E_POINTER;

    bstr_scope update_id;
    if (SUCCEEDED(identity->get_UpdateID(update_id.put())) && update_id.value != nullptr)
        copy_wide(item->identity.update_id, REACH_WINDOWS_UPDATE_ID_CAPACITY, update_id.value);

    LONG revision = 0;
    (void)identity->get_RevisionNumber(&revision);
    item->identity.revision_number = revision;

    ComPtr<IStringCollection> kb_ids;
    if (SUCCEEDED(update->get_KBArticleIDs(kb_ids.GetAddressOf())) && kb_ids != nullptr)
    {
        read_string_collection(kb_ids.Get(), item->identity.kb_article_ids,
                               REACH_WINDOWS_UPDATE_METADATA_CAPACITY);
    }

    ComPtr<ICategoryCollection> categories;
    if (SUCCEEDED(update->get_Categories(categories.GetAddressOf())) && categories != nullptr)
        read_categories(categories.Get(), item->categories, REACH_WINDOWS_UPDATE_METADATA_CAPACITY);

    bstr_scope msrc_severity;
    if (SUCCEEDED(update->get_MsrcSeverity(msrc_severity.put())) && msrc_severity.value != nullptr)
        copy_wide(item->msrc_severity, REACH_WINDOWS_UPDATE_SEVERITY_CAPACITY, msrc_severity.value);

    ComPtr<IStringCollection> security_bulletin_ids;
    if (SUCCEEDED(update->get_SecurityBulletinIDs(security_bulletin_ids.GetAddressOf())) &&
        security_bulletin_ids != nullptr)
    {
        read_string_collection(security_bulletin_ids.Get(), item->security_bulletin_ids,
                               REACH_WINDOWS_UPDATE_BULLETIN_CAPACITY);
    }

    VARIANT_BOOL value = VARIANT_FALSE;
    if (SUCCEEDED(update->get_IsDownloaded(&value)))
        item->downloaded = value == VARIANT_TRUE;

    value = VARIANT_FALSE;
    if (SUCCEEDED(update->get_EulaAccepted(&value)))
        item->eula_accepted = value == VARIANT_TRUE;

    ComPtr<IInstallationBehavior> behavior;
    if (SUCCEEDED(update->get_InstallationBehavior(behavior.GetAddressOf())) && behavior != nullptr)
    {
        value = VARIANT_FALSE;
        if (SUCCEEDED(behavior->get_CanRequestUserInput(&value)))
            item->can_request_user_input = value == VARIANT_TRUE;

        value = VARIANT_FALSE;
        if (SUCCEEDED(behavior->get_RequiresNetworkConnectivity(&value)))
            item->requires_network_connectivity = value == VARIANT_TRUE;

        InstallationRebootBehavior reboot_behavior = irbCanRequestReboot;
        if (SUCCEEDED(behavior->get_RebootBehavior(&reboot_behavior)) &&
            reboot_behavior != irbCanRequestReboot)
        {
            item->reboot_required_known = 1;
            item->reboot_required = reboot_behavior == irbAlwaysRequiresReboot;
        }
    }

    item->state = REACH_WINDOWS_UPDATE_DISCOVERED;
    item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_NOT_RUN;
    return S_OK;
}

static HRESULT search_pending(reach_windows_update_adapter *adapter, IUpdateSearcher *searcher,
                              ComPtr<IUpdateCollection> *updates)
{
    if (searcher == nullptr || updates == nullptr)
        return E_INVALIDARG;

    updates->Reset();

    bstr_scope criteria(L"IsInstalled=0 and IsHidden=0 and Type='Software'");
    if (criteria.value == nullptr)
        return E_OUTOFMEMORY;

    ComPtr<ISearchResult> search_result;
    HRESULT hr = S_OK;
    if (adapter == nullptr)
    {
        hr = searcher->Search(criteria.value, search_result.GetAddressOf());
    }
    else
    {
        search_completed_callback *callback_value = new (std::nothrow) search_completed_callback();
        if (callback_value == nullptr)
            return E_OUTOFMEMORY;
        ComPtr<ISearchCompletedCallback> callback;
        callback.Attach(callback_value);
        if (callback_value->completed_event() == nullptr)
            return HRESULT_FROM_WIN32(GetLastError());

        VARIANT state;
        VariantInit(&state);
        ComPtr<ISearchJob> job;
        hr = searcher->BeginSearch(criteria.value, callback.Get(), state, job.GetAddressOf());
        if (SUCCEEDED(hr) && job == nullptr)
            hr = E_POINTER;

        int32_t aborted = 0;
        while (SUCCEEDED(hr))
        {
            DWORD wait_result = WaitForSingleObject(callback_value->completed_event(), 100);
            if (wait_result == WAIT_OBJECT_0)
                break;
            if (wait_result != WAIT_TIMEOUT)
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
            if (adapter->cancel_requested.load())
            {
                (void)job->RequestAbort();
                aborted = 1;
                hr = HRESULT_FROM_WIN32(ERROR_CANCELLED);
                break;
            }
        }
        if (SUCCEEDED(hr))
            hr = searcher->EndSearch(job.Get(), search_result.GetAddressOf());
        if (!aborted && job != nullptr)
            (void)job->CleanUp();
    }
    if (FAILED(hr))
        return hr;
    if (search_result == nullptr)
        return E_POINTER;

    hr = search_result->get_Updates(updates->ReleaseAndGetAddressOf());
    if (FAILED(hr))
        return hr;
    return updates->Get() != nullptr ? S_OK : E_POINTER;
}

static reach_result scan(void *userdata, reach_windows_update_list *out_updates,
                         int32_t *out_hresult)
{
    if (out_updates == nullptr || out_hresult == nullptr)
        return REACH_INVALID_ARGUMENT;

    reach_windows_update_adapter *adapter = static_cast<reach_windows_update_adapter *>(userdata);
    if (adapter == nullptr)
        return REACH_INVALID_ARGUMENT;

    memset(out_updates, 0, sizeof(*out_updates));
    *out_hresult = 0;

    com_scope com;
    if (FAILED(com.result))
    {
        *out_hresult = (int32_t)com.result;
        return REACH_ERROR;
    }

    ComPtr<IUpdateSession> session;
    ComPtr<IUpdateSearcher> searcher;
    HRESULT hr = create_session_and_searcher(&session, &searcher);

    ComPtr<IUpdateCollection> updates;
    if (SUCCEEDED(hr))
        hr = search_pending(adapter, searcher.Get(), &updates);

    if (FAILED(hr))
    {
        *out_hresult = (int32_t)hr;
        return REACH_ERROR;
    }

    LONG count = 0;
    hr = updates->get_Count(&count);
    if (FAILED(hr))
    {
        *out_hresult = (int32_t)hr;
        return REACH_ERROR;
    }

    for (LONG index = 0; index < count && out_updates->count < REACH_WINDOWS_UPDATE_MAX_UPDATES;
         ++index)
    {
        ComPtr<IUpdate> update;
        if (SUCCEEDED(updates->get_Item(index, update.GetAddressOf())) && update != nullptr &&
            SUCCEEDED(read_update(update.Get(), &out_updates->updates[out_updates->count])))
        {
            ++out_updates->count;
        }
    }

    return REACH_OK;
}

static int32_t identity_equals(IUpdate *update, const reach_windows_update_identity *selected)
{
    if (update == nullptr || selected == nullptr)
        return 0;

    ComPtr<IUpdateIdentity> identity;
    if (FAILED(update->get_Identity(identity.GetAddressOf())) || identity == nullptr)
        return 0;

    LONG revision = 0;
    if (FAILED(identity->get_RevisionNumber(&revision)))
        return 0;

    bstr_scope id;
    if (FAILED(identity->get_UpdateID(id.put())) || id.value == nullptr)
        return 0;

    return revision == selected->revision_number &&
           wcscmp(id.value, reinterpret_cast<const wchar_t *>(selected->update_id)) == 0;
}

static int32_t pending_contains(IUpdateCollection *pending,
                                const reach_windows_update_identity *identity)
{
    if (pending == nullptr || identity == nullptr)
        return 0;

    LONG count = 0;
    if (FAILED(pending->get_Count(&count)))
        return 0;

    for (LONG index = 0; index < count; ++index)
    {
        ComPtr<IUpdate> update;
        if (SUCCEEDED(pending->get_Item(index, update.GetAddressOf())) && update != nullptr &&
            identity_equals(update.Get(), identity))
        {
            return 1;
        }
    }
    return 0;
}

static int32_t history_contains_success(IUpdateSearcher *searcher,
                                        const reach_windows_update_identity *identity,
                                        int32_t *out_hresult, int32_t *out_result_code,
                                        DATE *out_date)
{
    if (searcher == nullptr || identity == nullptr)
        return 0;

    LONG total = 0;
    if (FAILED(searcher->GetTotalHistoryCount(&total)) || total <= 0)
        return 0;

    LONG count = total < 256 ? total : 256;
    ComPtr<IUpdateHistoryEntryCollection> history;
    if (FAILED(searcher->QueryHistory(0, count, history.GetAddressOf())) || history == nullptr)
        return 0;

    LONG history_count = 0;
    if (FAILED(history->get_Count(&history_count)))
        return 0;

    for (LONG index = 0; index < history_count; ++index)
    {
        ComPtr<IUpdateHistoryEntry> entry;
        if (FAILED(history->get_Item(index, entry.GetAddressOf())) || entry == nullptr)
            continue;

        ComPtr<IUpdateIdentity> entry_identity;
        if (FAILED(entry->get_UpdateIdentity(entry_identity.GetAddressOf())) ||
            entry_identity == nullptr)
            continue;

        bstr_scope id;
        if (FAILED(entry_identity->get_UpdateID(id.put())) || id.value == nullptr)
            continue;

        LONG revision = 0;
        (void)entry_identity->get_RevisionNumber(&revision);
        if (revision != identity->revision_number ||
            wcscmp(id.value, reinterpret_cast<const wchar_t *>(identity->update_id)) != 0)
        {
            continue;
        }

        LONG hr = 0;
        OperationResultCode code = orcNotStarted;
        DATE date = 0.0;
        (void)entry->get_HResult(&hr);
        (void)entry->get_ResultCode(&code);
        (void)entry->get_Date(&date);

        if (out_hresult != nullptr)
            *out_hresult = hr;
        if (out_result_code != nullptr)
            *out_result_code = (int32_t)code;
        if (out_date != nullptr)
            *out_date = date;

        return operation_succeeded(code);
    }

    return 0;
}

static DATE current_boot_date(void)
{
    FILETIME file_time = {};
    GetSystemTimeAsFileTime(&file_time);

    ULARGE_INTEGER now = {};
    now.LowPart = file_time.dwLowDateTime;
    now.HighPart = file_time.dwHighDateTime;

    uint64_t uptime_100ns = GetTickCount64() * 10000ULL;
    if (now.QuadPart > uptime_100ns)
        now.QuadPart -= uptime_100ns;

    file_time.dwLowDateTime = now.LowPart;
    file_time.dwHighDateTime = now.HighPart;

    SYSTEMTIME boot_time = {};
    DATE boot_date = 0.0;
    if (FileTimeToSystemTime(&file_time, &boot_time))
        (void)SystemTimeToVariantTime(&boot_time, &boot_date);
    return boot_date;
}

static void verify_results(IUpdateSearcher *searcher, IUpdateCollection *pending,
                           reach_windows_update_operation_result *result)
{
    if (searcher == nullptr || pending == nullptr || result == nullptr)
        return;

    DATE boot_date = current_boot_date();

    for (size_t index = 0; index < result->per_update_result_count; ++index)
    {
        reach_windows_update_item *item = &result->per_update_results[index];
        if (item->state == REACH_WINDOWS_UPDATE_FAILED)
            continue;

        int32_t history_hresult = 0;
        int32_t history_result_code = 0;
        DATE history_date = 0.0;
        int32_t history_success = history_contains_success(
            searcher, &item->identity, &history_hresult, &history_result_code, &history_date);

        if (history_result_code != 0 || history_hresult != 0 || history_date > 0.0)
        {
            item->install_hresult = history_hresult;
            item->install_result_code = history_result_code;
        }

        int32_t still_pending = pending_contains(pending, &item->identity);
        if (item->reboot_required)
        {
            int32_t reboot_observed = history_date > 0.0 && boot_date > history_date;
            if (!reboot_observed ||
                history_hresult == (int32_t)REACH_WU_E_UH_POSTREBOOTSTILLPENDING)
            {
                item->state = REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED;
                item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_PENDING_REBOOT;
                item->failure_class = REACH_WINDOWS_UPDATE_FAILURE_NONE;
            }
            else if (history_success && !still_pending)
            {
                item->state = REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED;
                item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_SUCCEEDED;
            }
            else
            {
                item->state = REACH_WINDOWS_UPDATE_FAILED;
                item->failure_class = REACH_WINDOWS_UPDATE_VERIFICATION_FAILED;
                item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_FAILED;
            }
        }
        else if (history_success && !still_pending)
        {
            item->state = REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED;
            item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_SUCCEEDED;
        }
        else
        {
            item->state = REACH_WINDOWS_UPDATE_FAILED;
            item->failure_class = REACH_WINDOWS_UPDATE_VERIFICATION_FAILED;
            item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_FAILED;
        }
    }
}

struct pending_verification_file
{
    uint32_t magic;
    uint32_t count;
    reach_windows_update_identity updates[REACH_WINDOWS_UPDATE_MAX_UPDATES];
};

static const uint32_t pending_verification_magic = 0x52575550;

static int32_t pending_verification_path(wchar_t *path, size_t capacity)
{
    if (path == nullptr || capacity == 0)
        return 0;

    wchar_t local_app_data[MAX_PATH] = {};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return 0;

    wchar_t directory[MAX_PATH] = {};
    swprintf_s(directory, L"%s\\Reach", local_app_data);
    if (!CreateDirectoryW(directory, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;

    swprintf_s(path, capacity, L"%s\\pending-windows-updates.bin", directory);
    return 1;
}

static void save_pending_verification(const reach_windows_update_operation_result *result)
{
    if (result == nullptr)
        return;

    wchar_t path[MAX_PATH] = {};
    if (!pending_verification_path(path, MAX_PATH))
        return;

    std::unique_ptr<pending_verification_file> file(new (std::nothrow) pending_verification_file());
    if (file == nullptr)
        return;
    memset(file.get(), 0, sizeof(*file));
    file->magic = pending_verification_magic;

    for (size_t index = 0; index < result->per_update_result_count; ++index)
    {
        const reach_windows_update_item *item = &result->per_update_results[index];
        if ((item->state == REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED ||
             item->verification_status ==
                 REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_PENDING_REBOOT) &&
            file->count < REACH_WINDOWS_UPDATE_MAX_UPDATES)
        {
            file->updates[file->count++] = item->identity;
        }
    }

    if (file->count == 0)
    {
        DeleteFileW(path);
        return;
    }

    HANDLE output =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    (void)WriteFile(output, file.get(), sizeof(*file), &written, nullptr);
    CloseHandle(output);
}

static reach_result load_pending_verification(void *, reach_windows_update_identity *out_updates,
                                              size_t update_capacity, size_t *out_update_count)
{
    if (out_updates == nullptr || out_update_count == nullptr)
        return REACH_INVALID_ARGUMENT;

    *out_update_count = 0;
    if (update_capacity == 0)
        return REACH_OK;

    wchar_t path[MAX_PATH] = {};
    if (!pending_verification_path(path, MAX_PATH))
        return REACH_ERROR;

    HANDLE input = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND ? REACH_OK : REACH_ERROR;

    std::unique_ptr<pending_verification_file> file(new (std::nothrow) pending_verification_file());
    if (file == nullptr)
    {
        CloseHandle(input);
        return REACH_ERROR;
    }
    DWORD read = 0;
    BOOL ok = ReadFile(input, file.get(), sizeof(*file), &read, nullptr);
    CloseHandle(input);

    if (!ok || read != sizeof(*file) || file->magic != pending_verification_magic ||
        file->count > REACH_WINDOWS_UPDATE_MAX_UPDATES)
        return REACH_ERROR;

    size_t count = file->count < update_capacity ? file->count : update_capacity;
    for (size_t index = 0; index < count; ++index)
        out_updates[index] = file->updates[index];

    *out_update_count = count;
    return REACH_OK;
}

static void mark_active_failed(reach_windows_update_operation_result *result,
                               reach_windows_update_failure_class failure, int32_t hresult,
                               int32_t download_phase)
{
    if (result == nullptr)
        return;

    for (size_t index = 0; index < result->per_update_result_count; ++index)
    {
        reach_windows_update_item *item = &result->per_update_results[index];
        if (item->state == REACH_WINDOWS_UPDATE_FAILED)
            continue;

        item->state = REACH_WINDOWS_UPDATE_FAILED;
        item->failure_class = failure;
        if (download_phase)
            item->download_hresult = hresult;
        else
            item->install_hresult = hresult;
    }
}

static reach_result install_elevated(reach_windows_update_adapter *adapter,
                                     const reach_windows_update_identity *selected,
                                     size_t selected_count,
                                     reach_windows_update_progress_callback progress,
                                     void *progress_user,
                                     reach_windows_update_operation_result *result)
{
    if (selected == nullptr || selected_count == 0 ||
        selected_count > REACH_WINDOWS_UPDATE_MAX_UPDATES || result == nullptr)
        return REACH_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));
    copy_wide(result->operation, 32, L"Install");
    set_utc_now(result->started_utc, 32);
    result->per_update_result_count = selected_count;

    for (size_t index = 0; index < selected_count; ++index)
    {
        result->per_update_results[index].identity = selected[index];
        result->per_update_results[index].selected = 1;
        result->per_update_results[index].state = REACH_WINDOWS_UPDATE_DOWNLOADING;
    }

    if (!is_elevated())
    {
        result->failure_class = REACH_WINDOWS_UPDATE_NOT_ELEVATED;
        result->overall_download_hresult = (int32_t)REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED;
        result->overall_install_hresult = (int32_t)REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED;
        mark_active_failed(result, result->failure_class,
                           (int32_t)REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED, 1);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    com_scope com;
    if (FAILED(com.result))
    {
        result->overall_download_hresult = (int32_t)com.result;
        result->failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
        mark_active_failed(result, result->failure_class, (int32_t)com.result, 1);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    ComPtr<IUpdateSession> session;
    ComPtr<IUpdateSearcher> searcher;
    HRESULT hr = create_session_and_searcher(&session, &searcher);

    ComPtr<IUpdateCollection> pending;
    if (SUCCEEDED(hr))
        hr = search_pending(adapter, searcher.Get(), &pending);

    if (FAILED(hr))
    {
        result->overall_download_hresult = (int32_t)hr;
        result->failure_class = classify_hresult(hr);
        mark_active_failed(result, result->failure_class, (int32_t)hr, 1);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    ComPtr<IUpdateCollection> download_updates;
    hr = create_empty_update_collection(pending.Get(), &download_updates);
    if (FAILED(hr))
    {
        result->overall_download_hresult = (int32_t)hr;
        result->failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
        mark_active_failed(result, result->failure_class, (int32_t)hr, 1);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    LONG pending_count = 0;
    (void)pending->get_Count(&pending_count);

    size_t download_map[REACH_WINDOWS_UPDATE_MAX_UPDATES] = {};
    size_t download_count = 0;

    for (size_t selected_index = 0; selected_index < selected_count; ++selected_index)
    {
        reach_windows_update_item *item = &result->per_update_results[selected_index];
        int32_t found = 0;

        for (LONG pending_index = 0; pending_index < pending_count; ++pending_index)
        {
            ComPtr<IUpdate> update;
            if (FAILED(pending->get_Item(pending_index, update.GetAddressOf())) ||
                update == nullptr || !identity_equals(update.Get(), &selected[selected_index]))
            {
                continue;
            }

            found = 1;

            VARIANT_BOOL accepted = VARIANT_FALSE;
            HRESULT eula_hr = update->get_EulaAccepted(&accepted);
            if (SUCCEEDED(eula_hr) && accepted != VARIANT_TRUE)
                eula_hr = update->AcceptEula();

            if (FAILED(eula_hr))
            {
                item->state = REACH_WINDOWS_UPDATE_FAILED;
                item->failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
                item->download_hresult = (int32_t)eula_hr;
                break;
            }

            LONG added = 0;
            HRESULT add_hr = download_updates->Add(update.Get(), &added);
            if (SUCCEEDED(add_hr) && download_count < REACH_WINDOWS_UPDATE_MAX_UPDATES)
            {
                download_map[download_count++] = selected_index;
            }
            else
            {
                item->state = REACH_WINDOWS_UPDATE_FAILED;
                item->failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
                item->download_hresult = (int32_t)(FAILED(add_hr) ? add_hr : E_FAIL);
            }
            break;
        }

        if (!found)
        {
            item->state = REACH_WINDOWS_UPDATE_FAILED;
            item->failure_class = REACH_WINDOWS_UPDATE_SUPERSEDED_OR_NO_LONGER_APPLICABLE;
        }
    }

    if (download_count == 0)
    {
        result->failure_class = REACH_WINDOWS_UPDATE_SUPERSEDED_OR_NO_LONGER_APPLICABLE;
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    if (progress != nullptr)
        progress(progress_user, REACH_WINDOWS_UPDATE_PROGRESS_DOWNLOADING);

    ComPtr<IUpdateDownloader> downloader;
    hr = session->CreateUpdateDownloader(downloader.GetAddressOf());
    if (SUCCEEDED(hr) && downloader == nullptr)
        hr = E_POINTER;
    if (SUCCEEDED(hr))
        hr = downloader->put_Updates(download_updates.Get());

    ComPtr<IDownloadResult> download_result;
    if (SUCCEEDED(hr))
        hr = downloader->Download(download_result.GetAddressOf());
    if (SUCCEEDED(hr) && download_result == nullptr)
        hr = E_POINTER;

    if (FAILED(hr))
    {
        result->overall_download_hresult = (int32_t)hr;
        result->failure_class = hr == REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED
                                    ? REACH_WINDOWS_UPDATE_NOT_ELEVATED
                                    : REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
        mark_active_failed(result, result->failure_class, (int32_t)hr, 1);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    OperationResultCode download_code = orcNotStarted;
    LONG download_hresult = 0;
    (void)download_result->get_ResultCode(&download_code);
    (void)download_result->get_HResult(&download_hresult);
    result->overall_download_result_code = (int32_t)download_code;
    result->overall_download_hresult = download_hresult;

    ComPtr<IUpdateCollection> install_updates;
    hr = create_empty_update_collection(download_updates.Get(), &install_updates);
    if (FAILED(hr))
    {
        result->overall_install_hresult = (int32_t)hr;
        result->failure_class = REACH_WINDOWS_UPDATE_INSTALL_FAILED;
        mark_active_failed(result, result->failure_class, (int32_t)hr, 0);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    size_t install_map[REACH_WINDOWS_UPDATE_MAX_UPDATES] = {};
    size_t install_count = 0;

    for (size_t index = 0; index < download_count; ++index)
    {
        ComPtr<IUpdateDownloadResult> per_download;
        LONG per_hresult = 0;
        OperationResultCode per_code = orcNotStarted;

        if (SUCCEEDED(download_result->GetUpdateResult((LONG)index, per_download.GetAddressOf())) &&
            per_download != nullptr)
        {
            (void)per_download->get_HResult(&per_hresult);
            (void)per_download->get_ResultCode(&per_code);
        }

        reach_windows_update_item *item = &result->per_update_results[download_map[index]];
        item->download_hresult = per_hresult;
        item->download_result_code = (int32_t)per_code;

        if (!operation_succeeded(per_code))
        {
            item->state = REACH_WINDOWS_UPDATE_FAILED;
            item->failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
            continue;
        }

        ComPtr<IUpdate> update;
        LONG added = 0;
        HRESULT add_hr = E_POINTER;
        if (SUCCEEDED(download_updates->get_Item((LONG)index, update.GetAddressOf())) &&
            update != nullptr)
        {
            add_hr = install_updates->Add(update.Get(), &added);
        }

        if (SUCCEEDED(add_hr) && install_count < REACH_WINDOWS_UPDATE_MAX_UPDATES)
        {
            item->downloaded = 1;
            item->state = REACH_WINDOWS_UPDATE_DOWNLOADED;
            install_map[install_count++] = download_map[index];
        }
        else
        {
            item->state = REACH_WINDOWS_UPDATE_FAILED;
            item->failure_class = REACH_WINDOWS_UPDATE_INSTALL_FAILED;
            item->install_hresult = (int32_t)(FAILED(add_hr) ? add_hr : E_FAIL);
        }
    }

    if (install_count == 0)
    {
        result->failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    if (progress != nullptr)
        progress(progress_user, REACH_WINDOWS_UPDATE_PROGRESS_INSTALLING);

    for (size_t index = 0; index < install_count; ++index)
        result->per_update_results[install_map[index]].state = REACH_WINDOWS_UPDATE_INSTALLING;

    ComPtr<IUpdateInstaller> installer;
    hr = session->CreateUpdateInstaller(installer.GetAddressOf());
    if (SUCCEEDED(hr) && installer == nullptr)
        hr = E_POINTER;

    VARIANT_BOOL installer_busy = VARIANT_FALSE;
    VARIANT_BOOL reboot_before_install = VARIANT_FALSE;
    if (SUCCEEDED(hr))
        (void)installer->get_IsBusy(&installer_busy);
    if (SUCCEEDED(hr))
        (void)installer->get_RebootRequiredBeforeInstallation(&reboot_before_install);

    if (FAILED(hr))
    {
        result->overall_install_hresult = (int32_t)hr;
        result->failure_class = classify_hresult(hr);
        mark_active_failed(result, result->failure_class, (int32_t)hr, 0);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    if (installer_busy == VARIANT_TRUE || reboot_before_install == VARIANT_TRUE)
    {
        result->failure_class = installer_busy == VARIANT_TRUE
                                    ? REACH_WINDOWS_UPDATE_ANOTHER_OPERATION_IN_PROGRESS
                                    : REACH_WINDOWS_UPDATE_REBOOT_REQUIRED_BEFORE_INSTALL;
        mark_active_failed(result, result->failure_class, 0, 0);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    hr = installer->put_Updates(install_updates.Get());

    ComPtr<IInstallationResult> installation_result;
    if (SUCCEEDED(hr))
        hr = installer->Install(installation_result.GetAddressOf());
    if (SUCCEEDED(hr) && installation_result == nullptr)
        hr = E_POINTER;

    if (FAILED(hr))
    {
        result->overall_install_hresult = (int32_t)hr;
        result->failure_class = classify_hresult(hr);
        mark_active_failed(result, result->failure_class, (int32_t)hr, 0);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    OperationResultCode install_code = orcNotStarted;
    LONG install_hresult = 0;
    VARIANT_BOOL reboot_required = VARIANT_FALSE;
    (void)installation_result->get_ResultCode(&install_code);
    (void)installation_result->get_HResult(&install_hresult);
    (void)installation_result->get_RebootRequired(&reboot_required);

    result->overall_install_result_code = (int32_t)install_code;
    result->overall_install_hresult = install_hresult;
    result->overall_reboot_required = reboot_required == VARIANT_TRUE;

    for (size_t index = 0; index < install_count; ++index)
    {
        ComPtr<IUpdateInstallationResult> per_install;
        LONG per_hresult = 0;
        OperationResultCode per_code = orcNotStarted;
        VARIANT_BOOL per_reboot = VARIANT_FALSE;

        if (SUCCEEDED(
                installation_result->GetUpdateResult((LONG)index, per_install.GetAddressOf())) &&
            per_install != nullptr)
        {
            (void)per_install->get_HResult(&per_hresult);
            (void)per_install->get_ResultCode(&per_code);
            (void)per_install->get_RebootRequired(&per_reboot);
        }

        reach_windows_update_item *item = &result->per_update_results[install_map[index]];
        item->install_hresult = per_hresult;
        item->install_result_code = (int32_t)per_code;
        item->reboot_required_known = 1;
        item->reboot_required = per_reboot == VARIANT_TRUE;

        if (operation_succeeded(per_code))
        {
            item->state = item->reboot_required ? REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED
                                                : REACH_WINDOWS_UPDATE_INSTALLED_NO_REBOOT_REQUIRED;
        }
        else
        {
            item->state = REACH_WINDOWS_UPDATE_FAILED;
            item->failure_class = REACH_WINDOWS_UPDATE_INSTALL_FAILED;
        }
    }

    if (progress != nullptr)
        progress(progress_user, REACH_WINDOWS_UPDATE_PROGRESS_VERIFYING);

    ComPtr<IUpdateCollection> pending_after;
    hr = search_pending(adapter, searcher.Get(), &pending_after);
    if (FAILED(hr))
    {
        result->failure_class = REACH_WINDOWS_UPDATE_VERIFICATION_FAILED;
        for (size_t index = 0; index < result->per_update_result_count; ++index)
        {
            reach_windows_update_item *item = &result->per_update_results[index];
            if (item->state == REACH_WINDOWS_UPDATE_FAILED)
                continue;
            if (item->reboot_required)
            {
                item->state = REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED;
                item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_PENDING_REBOOT;
                item->failure_class = REACH_WINDOWS_UPDATE_FAILURE_NONE;
            }
            else
            {
                item->state = REACH_WINDOWS_UPDATE_FAILED;
                item->failure_class = REACH_WINDOWS_UPDATE_VERIFICATION_FAILED;
                item->verification_status = REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_FAILED;
            }
        }
        save_pending_verification(result);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    verify_results(searcher.Get(), pending_after.Get(), result);

    result->failure_class = REACH_WINDOWS_UPDATE_FAILURE_NONE;
    for (size_t index = 0; index < result->per_update_result_count; ++index)
    {
        if (result->per_update_results[index].state == REACH_WINDOWS_UPDATE_FAILED &&
            result->per_update_results[index].failure_class != REACH_WINDOWS_UPDATE_FAILURE_NONE)
        {
            result->failure_class = result->per_update_results[index].failure_class;
            break;
        }
    }

    save_pending_verification(result);
    set_utc_now(result->completed_utc, 32);
    return result->failure_class == REACH_WINDOWS_UPDATE_FAILURE_NONE ? REACH_OK : REACH_ERROR;
}

struct helper_request
{
    uint32_t magic;
    uint32_t count;
    reach_windows_update_identity selected[REACH_WINDOWS_UPDATE_MAX_UPDATES];
};

struct helper_response
{
    uint32_t magic;
    int32_t result;
    reach_windows_update_operation_result operation;
};

static const uint32_t helper_request_magic = 0x52575551;
static const uint32_t helper_response_magic = 0x52575552;

static int32_t write_all(const wchar_t *path, const void *data, DWORD size)
{
    if (path == nullptr || data == nullptr || size == 0)
        return 0;

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return 0;

    DWORD written = 0;
    BOOL ok = WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
    return ok && written == size;
}

static int32_t read_all(const wchar_t *path, void *data, DWORD size)
{
    if (path == nullptr || data == nullptr || size == 0)
        return 0;

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return 0;

    DWORD read = 0;
    BOOL ok = ReadFile(file, data, size, &read, nullptr);
    CloseHandle(file);
    return ok && read == size;
}

static reach_result set_install_transport_failure(const reach_windows_update_identity *selected,
                                                  size_t selected_count,
                                                  reach_windows_update_operation_result *result,
                                                  HRESULT hresult)
{
    if (result == nullptr)
        return REACH_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));
    copy_wide(result->operation, 32, L"Install");
    set_utc_now(result->started_utc, 32);
    set_utc_now(result->completed_utc, 32);

    result->failure_class = hresult == REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED
                                ? REACH_WINDOWS_UPDATE_NOT_ELEVATED
                                : REACH_WINDOWS_UPDATE_INSTALL_FAILED;
    result->overall_download_hresult = (int32_t)hresult;
    result->overall_install_hresult = (int32_t)hresult;
    result->per_update_result_count = selected_count < REACH_WINDOWS_UPDATE_MAX_UPDATES
                                          ? selected_count
                                          : REACH_WINDOWS_UPDATE_MAX_UPDATES;

    for (size_t index = 0; index < result->per_update_result_count; ++index)
    {
        if (selected != nullptr)
            result->per_update_results[index].identity = selected[index];
        result->per_update_results[index].selected = 1;
        result->per_update_results[index].state = REACH_WINDOWS_UPDATE_FAILED;
        result->per_update_results[index].failure_class = result->failure_class;
        result->per_update_results[index].download_hresult = (int32_t)hresult;
        result->per_update_results[index].install_hresult = (int32_t)hresult;
    }

    return REACH_ERROR;
}

static reach_result request_elevated_install(const reach_windows_update_identity *selected,
                                             size_t selected_count,
                                             reach_windows_update_progress_callback progress,
                                             void *progress_user,
                                             reach_windows_update_operation_result *result)
{
    if (selected == nullptr || selected_count == 0 ||
        selected_count > REACH_WINDOWS_UPDATE_MAX_UPDATES || result == nullptr)
        return REACH_INVALID_ARGUMENT;

    wchar_t temp[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, temp) == 0)
        return set_install_transport_failure(selected, selected_count, result,
                                             HRESULT_FROM_WIN32(GetLastError()));

    uint64_t tick = GetTickCount64();
    wchar_t request_path[MAX_PATH] = {};
    wchar_t response_path[MAX_PATH] = {};
    swprintf_s(request_path, L"%sreach-update-%lu-%llu.req", temp, GetCurrentProcessId(), tick);
    swprintf_s(response_path, L"%sreach-update-%lu-%llu.res", temp, GetCurrentProcessId(), tick);

    std::unique_ptr<helper_request> request(new (std::nothrow) helper_request());
    if (request == nullptr)
        return set_install_transport_failure(selected, selected_count, result, E_OUTOFMEMORY);

    request->magic = helper_request_magic;
    request->count = (uint32_t)selected_count;
    for (size_t index = 0; index < selected_count; ++index)
        request->selected[index] = selected[index];

    if (!write_all(request_path, request.get(), sizeof(*request)))
        return set_install_transport_failure(selected, selected_count, result,
                                             HRESULT_FROM_WIN32(GetLastError()));

    wchar_t helper_path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, helper_path, MAX_PATH) == 0)
    {
        DeleteFileW(request_path);
        return set_install_transport_failure(selected, selected_count, result,
                                             HRESULT_FROM_WIN32(GetLastError()));
    }

    wchar_t *slash = wcsrchr(helper_path, L'\\');
    if (slash == nullptr)
    {
        DeleteFileW(request_path);
        return set_install_transport_failure(selected, selected_count, result, E_FAIL);
    }
    wcscpy_s(slash + 1, MAX_PATH - (size_t)(slash + 1 - helper_path), L"reach_update_helper.exe");

    wchar_t parameters[MAX_PATH * 2 + 16] = {};
    swprintf_s(parameters, L"\"%s\" \"%s\"", request_path, response_path);

    if (progress != nullptr)
        progress(progress_user, REACH_WINDOWS_UPDATE_PROGRESS_DOWNLOADING);

    SHELLEXECUTEINFOW execute = {};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"runas";
    execute.lpFile = helper_path;
    execute.lpParameters = parameters;
    execute.nShow = SW_HIDE;

    if (!ShellExecuteExW(&execute))
    {
        DeleteFileW(request_path);
        return set_install_transport_failure(selected, selected_count, result,
                                             REACH_WU_E_PER_MACHINE_UPDATE_ACCESS_DENIED);
    }

    if (progress != nullptr)
        progress(progress_user, REACH_WINDOWS_UPDATE_PROGRESS_INSTALLING);

    WaitForSingleObject(execute.hProcess, INFINITE);
    CloseHandle(execute.hProcess);

    std::unique_ptr<helper_response> response(new (std::nothrow) helper_response());
    int32_t valid = response != nullptr &&
                    read_all(response_path, response.get(), sizeof(*response)) &&
                    response->magic == helper_response_magic;

    DeleteFileW(request_path);
    DeleteFileW(response_path);

    if (!valid)
        return set_install_transport_failure(selected, selected_count, result, E_FAIL);

    *result = response->operation;
    if (progress != nullptr)
        progress(progress_user, REACH_WINDOWS_UPDATE_PROGRESS_VERIFYING);
    return response->result == REACH_OK ? REACH_OK : REACH_ERROR;
}

static reach_result install(void *userdata, const reach_windows_update_identity *selected,
                            size_t selected_count, reach_windows_update_progress_callback progress,
                            void *progress_user, reach_windows_update_operation_result *result)
{
    if (selected == nullptr || selected_count == 0 ||
        selected_count > REACH_WINDOWS_UPDATE_MAX_UPDATES || result == nullptr)
        return REACH_INVALID_ARGUMENT;

    reach_windows_update_adapter *adapter = static_cast<reach_windows_update_adapter *>(userdata);
    if (adapter == nullptr)
        return REACH_INVALID_ARGUMENT;

    return is_elevated() ? install_elevated(adapter, selected, selected_count, progress,
                                            progress_user, result)
                         : request_elevated_install(selected, selected_count, progress,
                                                    progress_user, result);
}

static reach_result verify(void *userdata, const reach_windows_update_identity *installed,
                           size_t installed_count, reach_windows_update_operation_result *result)
{
    if (installed == nullptr || installed_count == 0 ||
        installed_count > REACH_WINDOWS_UPDATE_MAX_UPDATES || result == nullptr)
        return REACH_INVALID_ARGUMENT;

    reach_windows_update_adapter *adapter = static_cast<reach_windows_update_adapter *>(userdata);
    if (adapter == nullptr)
        return REACH_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));
    copy_wide(result->operation, 32, L"Verify");
    set_utc_now(result->started_utc, 32);
    result->per_update_result_count = installed_count;

    for (size_t index = 0; index < installed_count; ++index)
    {
        result->per_update_results[index].identity = installed[index];
        copy_wide(result->per_update_results[index].selected_reason,
                  REACH_WINDOWS_UPDATE_TEXT_CAPACITY, L"PostRebootVerification");
        result->per_update_results[index].state = REACH_WINDOWS_UPDATE_REBOOT_OBSERVED;
        result->per_update_results[index].reboot_required_known = 1;
        result->per_update_results[index].reboot_required = 1;
    }

    com_scope com;
    ComPtr<IUpdateSession> session;
    ComPtr<IUpdateSearcher> searcher;
    HRESULT hr = FAILED(com.result) ? com.result : create_session_and_searcher(&session, &searcher);

    ComPtr<IUpdateCollection> pending;
    if (SUCCEEDED(hr))
        hr = search_pending(adapter, searcher.Get(), &pending);

    if (FAILED(hr))
    {
        result->overall_install_hresult = (int32_t)hr;
        result->failure_class = REACH_WINDOWS_UPDATE_VERIFICATION_FAILED;
        mark_active_failed(result, result->failure_class, (int32_t)hr, 0);
        set_utc_now(result->completed_utc, 32);
        return REACH_ERROR;
    }

    verify_results(searcher.Get(), pending.Get(), result);

    if (result->per_update_result_count > 0)
    {
        result->overall_install_result_code = result->per_update_results[0].install_result_code;
        result->overall_install_hresult = result->per_update_results[0].install_hresult;
    }

    result->overall_reboot_required = 0;
    result->failure_class = REACH_WINDOWS_UPDATE_FAILURE_NONE;
    for (size_t index = 0; index < result->per_update_result_count; ++index)
    {
        reach_windows_update_item *item = &result->per_update_results[index];
        if (item->state == REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED)
            result->overall_reboot_required = 1;
        if (item->state == REACH_WINDOWS_UPDATE_FAILED &&
            result->failure_class == REACH_WINDOWS_UPDATE_FAILURE_NONE)
            result->failure_class = item->failure_class != REACH_WINDOWS_UPDATE_FAILURE_NONE
                                        ? item->failure_class
                                        : REACH_WINDOWS_UPDATE_VERIFICATION_FAILED;
    }

    save_pending_verification(result);
    set_utc_now(result->completed_utc, 32);
    return result->failure_class == REACH_WINDOWS_UPDATE_FAILURE_NONE ? REACH_OK : REACH_ERROR;
}

static void cancel(void *userdata)
{
    reach_windows_update_adapter *adapter = static_cast<reach_windows_update_adapter *>(userdata);
    if (adapter != nullptr)
        adapter->cancel_requested.store(1);
}

static void destroy(void *userdata)
{
    delete static_cast<reach_windows_update_adapter *>(userdata);
}

extern "C" reach_result reach_windows_create_windows_update(reach_windows_update_port *out_port)
{
    if (out_port == nullptr)
        return REACH_INVALID_ARGUMENT;

    *out_port = {};
    reach_windows_update_adapter *adapter = new (std::nothrow) reach_windows_update_adapter();
    if (adapter == nullptr)
        return REACH_ERROR;
    adapter->cancel_requested.store(0);

    out_port->userdata = adapter;
    out_port->scan = scan;
    out_port->install = install;
    out_port->verify = verify;
    out_port->load_pending_verification = load_pending_verification;
    out_port->cancel = cancel;
    out_port->destroy = destroy;
    return REACH_OK;
}

extern "C" int reach_windows_update_helper_run(const wchar_t *request_path,
                                               const wchar_t *response_path)
{
    if (request_path == nullptr || response_path == nullptr)
        return 2;

    std::unique_ptr<helper_request> request(new (std::nothrow) helper_request());
    std::unique_ptr<helper_response> response(new (std::nothrow) helper_response());
    if (request == nullptr || response == nullptr)
        return 2;

    response->magic = helper_response_magic;
    if (!read_all(request_path, request.get(), sizeof(*request)) ||
        request->magic != helper_request_magic || request->count == 0 ||
        request->count > REACH_WINDOWS_UPDATE_MAX_UPDATES)
    {
        return 2;
    }

    response->result = install_elevated(nullptr, request->selected, request->count, nullptr,
                                        nullptr, &response->operation);
    if (!write_all(response_path, response.get(), sizeof(*response)))
        return 3;
    return response->result == REACH_OK ? 0 : 1;
}
