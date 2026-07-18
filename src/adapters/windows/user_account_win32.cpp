#include "windows_adapters_internal.h"

#include "reach/ports/user_account.h"
#include "windows_icon_handle_internal.h"

#include <lm.h>
#include <sddl.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <new>

#define SECURITY_WIN32
#include <security.h>

struct reach_user_account
{
    int32_t queried;
    reach_user_account_info info;
};

struct reach_user_account_com_scope
{
    HRESULT hr;
    int32_t uninitialize;

    reach_user_account_com_scope()
        : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), uninitialize(SUCCEEDED(hr) ? 1 : 0)
    {
    }

    ~reach_user_account_com_scope()
    {
        if (uninitialize)
        {
            CoUninitialize();
        }
    }
};

static void reach_user_account_read_user_name(uint16_t *out_name, DWORD capacity)
{
    out_name[0] = 0;
    DWORD count = capacity;
    if (!GetUserNameW(reinterpret_cast<wchar_t *>(out_name), &count))
    {
        out_name[0] = 0;
    }
}

typedef BOOLEAN(WINAPI *reach_get_user_name_ex_fn)(EXTENDED_NAME_FORMAT, LPWSTR, PULONG);

static void reach_user_account_read_display_name(uint16_t *out_name, DWORD capacity)
{
    out_name[0] = 0;
    HMODULE secur32 = LoadLibraryW(L"secur32.dll");
    if (secur32 != nullptr)
    {
        reach_get_user_name_ex_fn get_user_name_ex = reinterpret_cast<reach_get_user_name_ex_fn>(
            GetProcAddress(secur32, "GetUserNameExW"));
        if (get_user_name_ex != nullptr)
        {
            ULONG count = capacity;
            if (!get_user_name_ex(NameDisplay, reinterpret_cast<wchar_t *>(out_name), &count))
            {
                out_name[0] = 0;
            }
        }
        FreeLibrary(secur32);
    }
    if (out_name[0] == 0)
    {
        reach_user_account_read_user_name(out_name, capacity);
    }
}

static int32_t reach_user_account_token_is_admin(HANDLE token)
{
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (!AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators))
    {
        return 0;
    }
    BOOL member = FALSE;
    if (!CheckTokenMembership(token, administrators, &member))
    {
        member = FALSE;
    }
    FreeSid(administrators);
    return member ? 1 : 0;
}

static int32_t reach_user_account_is_administrator(void)
{
    if (reach_user_account_token_is_admin(nullptr))
    {
        return 1;
    }

    HANDLE process_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &process_token))
    {
        return 0;
    }

    int32_t admin = 0;
    TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
    DWORD size = 0;
    if (GetTokenInformation(process_token, TokenElevationType, &elevation_type,
                            sizeof(elevation_type), &size) &&
        elevation_type == TokenElevationTypeLimited)
    {
        TOKEN_LINKED_TOKEN linked = {};
        if (GetTokenInformation(process_token, TokenLinkedToken, &linked, sizeof(linked), &size))
        {
            HANDLE impersonation = nullptr;
            if (DuplicateToken(linked.LinkedToken, SecurityIdentification, &impersonation))
            {
                admin = reach_user_account_token_is_admin(impersonation);
                CloseHandle(impersonation);
            }
            CloseHandle(linked.LinkedToken);
        }
    }
    CloseHandle(process_token);
    return admin;
}

static int32_t reach_user_account_sid_string(wchar_t *out_sid, DWORD capacity)
{
    out_sid[0] = 0;
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return 0;
    }

    int32_t copied = 0;
    BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE] = {};
    DWORD size = 0;
    if (GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &size))
    {
        TOKEN_USER *user = reinterpret_cast<TOKEN_USER *>(buffer);
        wchar_t *sid_string = nullptr;
        if (ConvertSidToStringSidW(user->User.Sid, &sid_string))
        {
            copied = wcscpy_s(out_sid, capacity, sid_string) == 0;
            LocalFree(sid_string);
        }
    }
    CloseHandle(token);
    return copied;
}

static int32_t reach_user_account_picture_path(wchar_t *out_path, DWORD capacity)
{
    out_path[0] = 0;
    wchar_t sid[184] = {};
    if (!reach_user_account_sid_string(sid, 184))
    {
        return 0;
    }

    wchar_t key_path[260] = {};
    if (wcscpy_s(key_path, 260,
                 L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AccountPicture\\Users\\") != 0 ||
        wcscat_s(key_path, 260, sid) != 0)
    {
        return 0;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path, 0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return 0;
    }

    static const wchar_t *values[] = {L"Image192", L"Image240", L"Image208", L"Image96",
                                      L"Image64",  L"Image48",  L"Image40",  L"Image32"};
    int32_t found = 0;
    for (const wchar_t *value : values)
    {
        DWORD type = 0;
        DWORD bytes = capacity * sizeof(wchar_t);
        if (RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<BYTE *>(out_path),
                             &bytes) == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ) && bytes >= sizeof(wchar_t))
        {
            out_path[capacity - 1] = 0;
            if (out_path[0] != 0)
            {
                found = 1;
                break;
            }
        }
        out_path[0] = 0;
    }
    RegCloseKey(key);
    return found;
}

static uint64_t reach_user_account_load_picture(void)
{
    wchar_t path[260] = {};
    if (!reach_user_account_picture_path(path, 260))
    {
        return 0;
    }

    IShellItemImageFactory *factory = nullptr;
    if (FAILED(SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&factory))) ||
        factory == nullptr)
    {
        return 0;
    }

    SIZE size = {};
    size.cx = 192;
    size.cy = 192;
    HBITMAP bitmap = nullptr;
    HRESULT hr = factory->GetImage(size, SIIGBF_BIGGERSIZEOK, &bitmap);
    factory->Release();
    if (FAILED(hr) || bitmap == nullptr)
    {
        return 0;
    }
    return reach_windows_icon_id_from_hbitmap(bitmap);
}

static reach_result reach_user_account_query(reach_user_account *account,
                                             reach_user_account_info *out_info)
{
    if (account == nullptr || out_info == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!account->queried)
    {
        reach_user_account_com_scope com_scope;
        reach_user_account_read_display_name(account->info.display_name,
                                             REACH_USER_ACCOUNT_NAME_CAPACITY);
        reach_user_account_read_user_name(account->info.user_name,
                                          REACH_USER_ACCOUNT_NAME_CAPACITY);
        account->info.is_administrator = reach_user_account_is_administrator();
        account->info.picture_icon_id = reach_user_account_load_picture();
        account->queried = 1;
    }
    *out_info = account->info;
    return account->info.display_name[0] != 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_user_account_verify_password(reach_user_account *account,
                                                       const uint16_t *password,
                                                       int32_t *out_valid)
{
    if (account == nullptr || password == nullptr || out_valid == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_valid = 1;

    reach_user_account_info info = {};
    if (reach_user_account_query(account, &info) != REACH_OK || info.user_name[0] == 0)
    {
        return REACH_ERROR;
    }

    HANDLE token = nullptr;
    if (LogonUserW(reinterpret_cast<const wchar_t *>(info.user_name), L".",
                   reinterpret_cast<const wchar_t *>(password), LOGON32_LOGON_NETWORK,
                   LOGON32_PROVIDER_DEFAULT, &token))
    {
        CloseHandle(token);
        return REACH_OK;
    }
    if (GetLastError() == ERROR_LOGON_FAILURE)
    {
        *out_valid = 0;
    }
    return REACH_OK;
}

typedef DWORD(WINAPI *reach_net_user_change_password_fn)(LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR);

static reach_result reach_user_account_change_password(
    reach_user_account *account, const uint16_t *current_password, const uint16_t *new_password,
    reach_user_account_password_status *out_status)
{
    if (account == nullptr || current_password == nullptr || new_password == nullptr ||
        out_status == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_status = REACH_USER_ACCOUNT_PASSWORD_FAILED;

    reach_user_account_info info = {};
    if (reach_user_account_query(account, &info) != REACH_OK || info.user_name[0] == 0)
    {
        return REACH_ERROR;
    }

    HMODULE netapi32 = LoadLibraryW(L"netapi32.dll");
    if (netapi32 == nullptr)
    {
        return REACH_ERROR;
    }
    reach_net_user_change_password_fn change_password =
        reinterpret_cast<reach_net_user_change_password_fn>(
            GetProcAddress(netapi32, "NetUserChangePassword"));
    DWORD status = ERROR_PROC_NOT_FOUND;
    if (change_password != nullptr)
    {
        status = change_password(nullptr, reinterpret_cast<const wchar_t *>(info.user_name),
                                 reinterpret_cast<const wchar_t *>(current_password),
                                 reinterpret_cast<const wchar_t *>(new_password));
    }
    FreeLibrary(netapi32);

    if (status == NERR_Success)
    {
        *out_status = REACH_USER_ACCOUNT_PASSWORD_CHANGED;
        return REACH_OK;
    }
    if (status == ERROR_INVALID_PASSWORD || status == NERR_BadPassword)
    {
        *out_status = REACH_USER_ACCOUNT_PASSWORD_WRONG_CURRENT;
    }
    else if (status == NERR_PasswordTooShort || status == NERR_PasswordHistConflict ||
             status == NERR_PasswordTooRecent || status == ERROR_ACCOUNT_RESTRICTION)
    {
        *out_status = REACH_USER_ACCOUNT_PASSWORD_POLICY;
    }
    return REACH_ERROR;
}

static void reach_user_account_destroy(reach_user_account *account)
{
    if (account == nullptr)
    {
        return;
    }
    if (account->info.picture_icon_id != 0)
    {
        reach_windows_icon_id_release(account->info.picture_icon_id);
    }
    delete account;
}

reach_result reach_windows_create_user_account(reach_user_account_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};

    reach_user_account *account = new (std::nothrow) reach_user_account();
    if (account == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->account = account;
    out_port->ops.query = reach_user_account_query;
    out_port->ops.verify_password = reach_user_account_verify_password;
    out_port->ops.change_password = reach_user_account_change_password;
    out_port->ops.destroy = reach_user_account_destroy;
    return REACH_OK;
}
