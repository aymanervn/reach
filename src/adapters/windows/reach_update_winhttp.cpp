#include "windows_adapters_internal.h"

#include "reach/ports/app_update.h"

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <new>
#include <string>

struct reach_app_update_state
{
    std::atomic<int32_t> cancelled;
};

static std::wstring reach_app_update_utf8_to_utf16(const char *text, size_t length)
{
    if (text == nullptr || length == 0)
    {
        return std::wstring();
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, text, (int)length, nullptr, 0);
    if (needed <= 0)
    {
        return std::wstring();
    }
    std::wstring result((size_t)needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, text, (int)length, &result[0], needed);
    return result;
}

static void reach_app_update_store_utf16(uint16_t *dst, size_t capacity, const std::wstring &value)
{
    reach_copy_utf16(dst, capacity, reinterpret_cast<const uint16_t *>(value.c_str()));
}

static reach_result reach_app_update_https_get(const wchar_t *host, const wchar_t *path,
                                               const wchar_t *accept, std::string *out_body)
{
    HINTERNET session = WinHttpOpen(L"reach-updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr)
    {
        return REACH_ERROR;
    }
    reach_result result = REACH_ERROR;
    HINTERNET connection = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection != nullptr)
    {
        HINTERNET request =
            WinHttpOpenRequest(connection, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (request != nullptr)
        {
            const wchar_t *headers = accept != nullptr ? accept : WINHTTP_NO_ADDITIONAL_HEADERS;
            DWORD headers_length = accept != nullptr ? (DWORD)-1L : 0;
            if (WinHttpSendRequest(request, headers, headers_length, WINHTTP_NO_REQUEST_DATA, 0, 0,
                                   0) &&
                WinHttpReceiveResponse(request, nullptr))
            {
                std::string body;
                for (;;)
                {
                    DWORD available = 0;
                    if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
                    {
                        break;
                    }
                    size_t offset = body.size();
                    body.resize(offset + available);
                    DWORD read = 0;
                    if (!WinHttpReadData(request, &body[offset], available, &read))
                    {
                        break;
                    }
                    body.resize(offset + read);
                    if (read == 0)
                    {
                        break;
                    }
                }
                *out_body = body;
                result = REACH_OK;
            }
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(connection);
    }
    WinHttpCloseHandle(session);
    return result;
}

static int reach_app_update_find_key(const std::string &json, const char *key, size_t from,
                                     size_t *out_value_start)
{
    std::string needle = std::string("\"") + key + "\"";
    size_t position = json.find(needle, from);
    if (position == std::string::npos)
    {
        return 0;
    }
    position += needle.size();
    while (position < json.size() && (json[position] == ' ' || json[position] == ':' ||
                                      json[position] == '\t' || json[position] == '\n'))
    {
        ++position;
    }
    *out_value_start = position;
    return 1;
}

static std::string reach_app_update_read_string(const std::string &json, size_t value_start)
{
    std::string out;
    if (value_start >= json.size() || json[value_start] != '"')
    {
        return out;
    }
    size_t index = value_start + 1;
    while (index < json.size())
    {
        char c = json[index];
        if (c == '\\' && index + 1 < json.size())
        {
            char next = json[index + 1];
            if (next == 'n')
            {
                out.push_back('\n');
            }
            else if (next == 'r')
            {
                out.push_back('\r');
            }
            else if (next == 't')
            {
                out.push_back(' ');
            }
            else if (next == 'u' && index + 5 < json.size())
            {
                index += 5;
                index += 1;
                continue;
            }
            else
            {
                out.push_back(next);
            }
            index += 2;
            continue;
        }
        if (c == '"')
        {
            break;
        }
        out.push_back(c);
        ++index;
    }
    return out;
}

static reach_result reach_app_update_check(void *userdata, const uint16_t *owner,
                                           const uint16_t *repo, reach_app_update_info *out_info)
{
    (void)userdata;
    REACH_ASSERT(owner != nullptr);
    REACH_ASSERT(repo != nullptr);
    REACH_ASSERT(out_info != nullptr);
    if (owner == nullptr || repo == nullptr || out_info == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_info = {};

    std::wstring path = L"/repos/";
    path += reinterpret_cast<const wchar_t *>(owner);
    path += L"/";
    path += reinterpret_cast<const wchar_t *>(repo);
    path += L"/releases/latest";

    std::string body;
    reach_result result = reach_app_update_https_get(
        L"api.github.com", path.c_str(),
        L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n", &body);
    if (result != REACH_OK || body.empty())
    {
        return REACH_ERROR;
    }

    size_t value_start = 0;
    if (!reach_app_update_find_key(body, "tag_name", 0, &value_start))
    {
        return REACH_ERROR;
    }
    std::string tag = reach_app_update_read_string(body, value_start);
    if (tag.empty())
    {
        return REACH_ERROR;
    }

    std::string chosen_url;
    size_t search = 0;
    while (reach_app_update_find_key(body, "browser_download_url", search, &value_start))
    {
        std::string url = reach_app_update_read_string(body, value_start);
        search = value_start + url.size() + 1;
        if (url.size() >= 4 && url.compare(url.size() - 4, 4, ".zip") == 0)
        {
            chosen_url = url;
            break;
        }
    }

    std::string notes;
    if (reach_app_update_find_key(body, "body", 0, &value_start))
    {
        notes = reach_app_update_read_string(body, value_start);
        if (notes.size() > REACH_APP_UPDATE_NOTES_CAPACITY - 1)
        {
            notes.resize(REACH_APP_UPDATE_NOTES_CAPACITY - 1);
        }
    }

    reach_app_update_store_utf16(out_info->version, REACH_APP_UPDATE_VERSION_CAPACITY,
                                 reach_app_update_utf8_to_utf16(tag.c_str(), tag.size()));
    reach_app_update_store_utf16(out_info->download_url, REACH_APP_UPDATE_URL_CAPACITY,
                                 reach_app_update_utf8_to_utf16(chosen_url.c_str(),
                                                                chosen_url.size()));
    reach_app_update_store_utf16(out_info->notes, REACH_APP_UPDATE_NOTES_CAPACITY,
                                 reach_app_update_utf8_to_utf16(notes.c_str(), notes.size()));

    size_t slash = chosen_url.find_last_of('/');
    std::string asset_name =
        slash == std::string::npos ? chosen_url : chosen_url.substr(slash + 1);
    reach_app_update_store_utf16(out_info->asset_name, REACH_APP_UPDATE_NAME_CAPACITY,
                                 reach_app_update_utf8_to_utf16(asset_name.c_str(),
                                                                asset_name.size()));
    out_info->has_release = !chosen_url.empty();
    return REACH_OK;
}

static reach_result reach_app_update_download(void *userdata, const uint16_t *url,
                                              const uint16_t *dest_path,
                                              reach_app_update_download_progress progress,
                                              void *progress_user)
{
    reach_app_update_state *state = static_cast<reach_app_update_state *>(userdata);
    REACH_ASSERT(url != nullptr);
    REACH_ASSERT(dest_path != nullptr);
    if (url == nullptr || dest_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (state != nullptr)
    {
        state->cancelled = 0;
    }

    const wchar_t *url_w = reinterpret_cast<const wchar_t *>(url);
    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    components.lpszHostName = host;
    components.dwHostNameLength = ARRAYSIZE(host);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = ARRAYSIZE(path);
    if (!WinHttpCrackUrl(url_w, 0, 0, &components))
    {
        return REACH_ERROR;
    }

    HINTERNET session = WinHttpOpen(L"reach-updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr)
    {
        return REACH_ERROR;
    }

    reach_result result = REACH_ERROR;
    HINTERNET connection = WinHttpConnect(session, host, components.nPort, 0);
    if (connection != nullptr)
    {
        DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = WinHttpOpenRequest(connection, L"GET", path, nullptr,
                                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               flags);
        if (request != nullptr)
        {
            DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
            if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, nullptr))
            {
                uint64_t total = 0;
                DWORD content_length = 0;
                DWORD length_size = sizeof(content_length);
                if (WinHttpQueryHeaders(request,
                                        WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &content_length, &length_size,
                                        WINHTTP_NO_HEADER_INDEX))
                {
                    total = content_length;
                }

                HANDLE file = CreateFileW(reinterpret_cast<const wchar_t *>(dest_path),
                                          GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file != INVALID_HANDLE_VALUE)
                {
                    uint64_t received = 0;
                    int32_t failed = 0;
                    for (;;)
                    {
                        if (state != nullptr && state->cancelled.load() != 0)
                        {
                            failed = 1;
                            break;
                        }
                        DWORD available = 0;
                        if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
                        {
                            break;
                        }
                        std::string chunk;
                        chunk.resize(available);
                        DWORD read = 0;
                        if (!WinHttpReadData(request, &chunk[0], available, &read) || read == 0)
                        {
                            break;
                        }
                        DWORD written = 0;
                        if (!WriteFile(file, chunk.data(), read, &written, nullptr) ||
                            written != read)
                        {
                            failed = 1;
                            break;
                        }
                        received += read;
                        if (progress != nullptr)
                        {
                            progress(progress_user, received, total);
                        }
                    }
                    CloseHandle(file);
                    if (!failed)
                    {
                        result = REACH_OK;
                    }
                    else
                    {
                        DeleteFileW(reinterpret_cast<const wchar_t *>(dest_path));
                    }
                }
            }
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(connection);
    }
    WinHttpCloseHandle(session);
    return result;
}

static void reach_app_update_cancel(void *userdata)
{
    reach_app_update_state *state = static_cast<reach_app_update_state *>(userdata);
    if (state != nullptr)
    {
        state->cancelled = 1;
    }
}

static void reach_app_update_destroy(void *userdata)
{
    reach_app_update_state *state = static_cast<reach_app_update_state *>(userdata);
    delete state;
}

reach_result reach_windows_create_app_update(reach_app_update_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};
    reach_app_update_state *state = new (std::nothrow) reach_app_update_state();
    if (state == nullptr)
    {
        return REACH_ERROR;
    }
    state->cancelled = 0;
    out_port->userdata = state;
    out_port->check = reach_app_update_check;
    out_port->download = reach_app_update_download;
    out_port->cancel = reach_app_update_cancel;
    out_port->destroy = reach_app_update_destroy;
    return REACH_OK;
}
