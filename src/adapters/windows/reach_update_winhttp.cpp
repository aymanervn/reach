#include "windows_adapters_internal.h"

#include "reach/ports/app_update.h"

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <cstring>
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

static std::string reach_app_update_clean_notes(const std::string &html)
{
    std::string out;
    size_t index = 0;
    int32_t in_tag = 0;
    while (index < html.size() && out.size() < REACH_APP_UPDATE_NOTES_CAPACITY - 1)
    {
        char c = html[index];
        if (c == '<')
        {
            in_tag = 1;
            ++index;
            continue;
        }
        if (c == '>')
        {
            in_tag = 0;
            ++index;
            continue;
        }
        if (in_tag)
        {
            ++index;
            continue;
        }
        if (c == '&')
        {
            if (html.compare(index, 4, "&lt;") == 0)
            {
                out.push_back('<');
                index += 4;
            }
            else if (html.compare(index, 4, "&gt;") == 0)
            {
                out.push_back('>');
                index += 4;
            }
            else if (html.compare(index, 5, "&amp;") == 0)
            {
                out.push_back('&');
                index += 5;
            }
            else if (html.compare(index, 6, "&quot;") == 0)
            {
                out.push_back('"');
                index += 6;
            }
            else if (html.compare(index, 5, "&#39;") == 0)
            {
                out.push_back('\'');
                index += 5;
            }
            else
            {
                out.push_back('&');
                ++index;
            }
            continue;
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

    const wchar_t *owner_w = reinterpret_cast<const wchar_t *>(owner);
    const wchar_t *repo_w = reinterpret_cast<const wchar_t *>(repo);

    std::wstring path = std::wstring(L"/") + owner_w + L"/" + repo_w + L"/releases.atom";
    std::string body;
    reach_result result =
        reach_app_update_https_get(L"github.com", path.c_str(),
                                   L"Accept: application/atom+xml\r\n", &body);
    if (result != REACH_OK || body.empty())
    {
        return REACH_ERROR;
    }

    size_t entry = body.find("<entry>");
    if (entry == std::string::npos)
    {
        return REACH_ERROR;
    }
    const char *tag_marker = "/releases/tag/";
    size_t tag_start = body.find(tag_marker, entry);
    if (tag_start == std::string::npos)
    {
        return REACH_ERROR;
    }
    tag_start += strlen(tag_marker);
    size_t tag_end = body.find('"', tag_start);
    if (tag_end == std::string::npos || tag_end <= tag_start)
    {
        return REACH_ERROR;
    }
    std::string tag = body.substr(tag_start, tag_end - tag_start);

    std::string notes;
    size_t content = body.find("<content", entry);
    if (content != std::string::npos)
    {
        size_t content_start = body.find('>', content);
        size_t content_end = body.find("</content>", content);
        if (content_start != std::string::npos && content_end != std::string::npos &&
            content_end > content_start)
        {
            notes = reach_app_update_clean_notes(
                body.substr(content_start + 1, content_end - content_start - 1));
        }
    }

    std::wstring tag_w = reach_app_update_utf8_to_utf16(tag.c_str(), tag.size());
    std::wstring asset_w = L"reach-" + tag_w + L"-Release.zip";
    std::wstring url_w = std::wstring(L"https://github.com/") + owner_w + L"/" + repo_w +
                         L"/releases/download/" + tag_w + L"/" + asset_w;

    reach_app_update_store_utf16(out_info->version, REACH_APP_UPDATE_VERSION_CAPACITY, tag_w);
    reach_app_update_store_utf16(out_info->asset_name, REACH_APP_UPDATE_NAME_CAPACITY, asset_w);
    reach_app_update_store_utf16(out_info->download_url, REACH_APP_UPDATE_URL_CAPACITY, url_w);
    reach_app_update_store_utf16(out_info->notes, REACH_APP_UPDATE_NOTES_CAPACITY,
                                 reach_app_update_utf8_to_utf16(notes.c_str(), notes.size()));
    out_info->has_release = !tag.empty();
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
