#include "windows_adapters_internal.h"

#include "reach/support/search_types.h"

#include "retriever.h"

#include <windows.h>

#include <new>
#include <stdio.h>
#include <string.h>
#include <vector>

static const uint32_t REACH_RETRIEVER_REQUEST_CAP = 512;
static const size_t REACH_RETRIEVER_CANDIDATE_CAP = 1024;
static const uint64_t REACH_RETRIEVER_QUERY_BUDGET_MS = 3000;
static const DWORD REACH_RETRIEVER_BUSY_WAIT_MS = 1000;
static const size_t REACH_RETRIEVER_RESPONSE_CAP = 4 * 1024 * 1024;

struct reach_search_provider
{
    HANDLE pipe;
    HANDLE io_event;
    reach_search_candidate results[REACH_SEARCH_MAX_RESULTS];
    size_t result_count;
};

static void reach_search_copy_utf16(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    if (src == nullptr)
    {
        dst[0] = 0;
        return;
    }
    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = src[index];
        ++index;
    }
    dst[index] = 0;
}

static const uint16_t *reach_search_file_name(const uint16_t *path)
{
    const uint16_t *name = path;
    if (path == nullptr)
    {
        return nullptr;
    }
    for (const uint16_t *scan = path; *scan != 0; ++scan)
    {
        if (*scan == '\\' || *scan == '/')
        {
            name = scan + 1;
        }
    }
    return name;
}

static uint16_t reach_search_lower(uint16_t ch)
{
    return ch >= 'A' && ch <= 'Z' ? (uint16_t)(ch - 'A' + 'a') : ch;
}

static int32_t reach_search_path_equals_ci(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        if (reach_search_lower(a[index]) != reach_search_lower(b[index]))
        {
            return 0;
        }
        ++index;
    }
    return a[index] == 0 && b[index] == 0;
}

static int32_t reach_search_path_has_extension_ci(const uint16_t *path, const char *extension)
{
    const uint16_t *name = reach_search_file_name(path);
    if (name == nullptr || extension == nullptr)
    {
        return 0;
    }

    const uint16_t *candidate_extension = nullptr;
    for (const uint16_t *scan = name; *scan != 0; ++scan)
    {
        if (*scan == '.')
        {
            candidate_extension = scan;
        }
    }
    if (candidate_extension == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (candidate_extension[index] != 0 && extension[index] != 0)
    {
        if (reach_search_lower(candidate_extension[index]) !=
            reach_search_lower((uint16_t)extension[index]))
        {
            return 0;
        }
        ++index;
    }
    return candidate_extension[index] == 0 && extension[index] == 0;
}

static int32_t reach_retriever_is_shape_class(uint16_t ch)
{
    return ch == 'd' || ch == 'a' || ch == 'p';
}

typedef enum reach_retriever_mode_mask
{
    REACH_RETRIEVER_MODE_NAME = 1,
    REACH_RETRIEVER_MODE_PATH = 2,
    REACH_RETRIEVER_MODE_REGEX = 4
} reach_retriever_mode_mask;

static uint32_t reach_retriever_select_modes(const uint16_t *query)
{
    int32_t has_backslash = 0;
    int32_t has_path_char = 0;
    int32_t all_backslashes_shape = 1;

    for (size_t index = 0; query[index] != 0; ++index)
    {
        uint16_t ch = query[index];
        if (ch == '/' || ch == ':')
        {
            has_path_char = 1;
        }
        else if (ch == '\\')
        {
            has_backslash = 1;
            uint16_t next = query[index + 1];
            if (next == '\\')
            {
                ++index;
            }
            else if (!reach_retriever_is_shape_class(next))
            {
                all_backslashes_shape = 0;
            }
        }
    }

    if (has_path_char)
    {
        return REACH_RETRIEVER_MODE_PATH;
    }
    if (has_backslash)
    {
        return all_backslashes_shape ? REACH_RETRIEVER_MODE_PATH | REACH_RETRIEVER_MODE_REGEX
                                     : REACH_RETRIEVER_MODE_PATH;
    }
    return REACH_RETRIEVER_MODE_NAME;
}

static void reach_retriever_disconnect(reach_search_provider *provider)
{
    if (provider->pipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(provider->pipe);
        provider->pipe = INVALID_HANDLE_VALUE;
    }
}

static int32_t reach_retriever_connect(reach_search_provider *provider)
{
    if (provider->pipe != INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        HANDLE pipe = CreateFileA(RTV_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (pipe != INVALID_HANDLE_VALUE)
        {
            provider->pipe = pipe;
            return 1;
        }
        if (GetLastError() != ERROR_PIPE_BUSY ||
            !WaitNamedPipeA(RTV_PIPE_NAME, REACH_RETRIEVER_BUSY_WAIT_MS))
        {
            return 0;
        }
    }
    return 0;
}

static int32_t reach_retriever_io_finish(reach_search_provider *provider, OVERLAPPED *overlapped,
                                         uint64_t deadline_ms, DWORD *out_transferred)
{
    uint64_t now = GetTickCount64();
    DWORD remaining = now < deadline_ms ? (DWORD)(deadline_ms - now) : 0;
    if (WaitForSingleObject(provider->io_event, remaining) != WAIT_OBJECT_0)
    {
        CancelIoEx(provider->pipe, overlapped);
        DWORD discarded = 0;
        GetOverlappedResult(provider->pipe, overlapped, &discarded, TRUE);
        return 0;
    }
    return GetOverlappedResult(provider->pipe, overlapped, out_transferred, FALSE) ? 1 : 0;
}

static int32_t reach_retriever_write_line(reach_search_provider *provider, const char *line,
                                          size_t length, uint64_t deadline_ms)
{
    OVERLAPPED overlapped = {};
    overlapped.hEvent = provider->io_event;
    DWORD transferred = 0;
    if (!WriteFile(provider->pipe, line, (DWORD)length, nullptr, &overlapped) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        return 0;
    }
    return reach_retriever_io_finish(provider, &overlapped, deadline_ms, &transferred) &&
           transferred == length;
}

static int32_t reach_retriever_read_chunk(reach_search_provider *provider, char *chunk,
                                          DWORD capacity, uint64_t deadline_ms,
                                          DWORD *out_transferred)
{
    OVERLAPPED overlapped = {};
    overlapped.hEvent = provider->io_event;
    if (!ReadFile(provider->pipe, chunk, capacity, nullptr, &overlapped) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        return 0;
    }
    return reach_retriever_io_finish(provider, &overlapped, deadline_ms, out_transferred) &&
           *out_transferred > 0;
}

static void reach_search_add_candidate(std::vector<reach_search_candidate> *candidates,
                                       const uint16_t *path)
{
    if (path == nullptr || path[0] == 0 || candidates->size() >= REACH_RETRIEVER_CANDIDATE_CAP)
    {
        return;
    }

    DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path));
    int32_t is_directory =
        attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (!is_directory && reach_search_path_has_extension_ci(path, ".lnk"))
    {
        return;
    }

    for (const reach_search_candidate &candidate : *candidates)
    {
        if (reach_search_path_equals_ci(candidate.path, path))
        {
            return;
        }
    }

    reach_search_candidate candidate = {};
    reach_search_copy_utf16(candidate.path, REACH_SEARCH_RESULT_PATH_CAPACITY, path);
    reach_search_copy_utf16(candidate.name, REACH_SEARCH_RESULT_NAME_CAPACITY,
                            reach_search_file_name(path));
    candidate.is_directory = is_directory;
    candidate.kind = reach_search_classify_result(candidate.path, candidate.is_directory);
    candidates->push_back(candidate);
}

static void reach_retriever_accept_line(const char *line, size_t length,
                                        std::vector<reach_search_candidate> *candidates)
{
    if (length == 0 || line[0] == '*' || line[0] == '!')
    {
        return;
    }

    uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
    int converted = MultiByteToWideChar(CP_UTF8, 0, line, (int)length,
                                        reinterpret_cast<LPWSTR>(path),
                                        REACH_SEARCH_RESULT_PATH_CAPACITY - 1);
    if (converted <= 0)
    {
        return;
    }
    path[converted] = 0;
    reach_search_add_candidate(candidates, path);
}

static reach_result reach_retriever_collect(reach_search_provider *provider, const char *mode,
                                            const char *query_utf8, uint64_t deadline_ms,
                                            std::vector<reach_search_candidate> *candidates)
{
    if (!reach_retriever_connect(provider))
    {
        return REACH_ERROR;
    }

    char request[RTV_MAX_LINE] = {};
    int request_length = snprintf(request, sizeof(request), "S %s %u %s\n", mode,
                                  REACH_RETRIEVER_REQUEST_CAP, query_utf8);
    if (request_length <= 0 || (size_t)request_length >= sizeof(request))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (!reach_retriever_write_line(provider, request, (size_t)request_length, deadline_ms))
    {
        reach_retriever_disconnect(provider);
        return REACH_ERROR;
    }

    std::vector<char> response;
    size_t parse_offset = 0;
    for (;;)
    {
        char chunk[8192];
        DWORD transferred = 0;
        if (!reach_retriever_read_chunk(provider, chunk, sizeof(chunk), deadline_ms, &transferred))
        {
            reach_retriever_disconnect(provider);
            return REACH_ERROR;
        }
        if (response.size() + transferred > REACH_RETRIEVER_RESPONSE_CAP)
        {
            reach_retriever_disconnect(provider);
            return REACH_ERROR;
        }
        response.insert(response.end(), chunk, chunk + transferred);

        while (parse_offset < response.size())
        {
            const char *line_start = response.data() + parse_offset;
            const char *newline = static_cast<const char *>(
                memchr(line_start, '\n', response.size() - parse_offset));
            if (newline == nullptr)
            {
                break;
            }
            size_t line_length = (size_t)(newline - line_start);
            parse_offset += line_length + 1;
            if (line_length > 0 && line_start[line_length - 1] == '\r')
            {
                --line_length;
            }
            if (line_length == 1 && line_start[0] == '.')
            {
                return REACH_OK;
            }
            reach_retriever_accept_line(line_start, line_length, candidates);
        }
    }
}

static reach_result reach_search_retriever_query(reach_search_provider *provider,
                                                 const uint16_t *query)
{
    if (provider == nullptr || query == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    provider->result_count = 0;
    if (query[0] == 0)
    {
        return REACH_OK;
    }

    char query_utf8[RTV_MAX_LINE] = {};
    int converted = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWSTR>(query), -1,
                                        query_utf8, sizeof(query_utf8) - 32, nullptr, nullptr);
    if (converted <= 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    std::vector<reach_search_candidate> candidates;
    candidates.reserve(REACH_RETRIEVER_REQUEST_CAP);

    uint64_t deadline_ms = GetTickCount64() + REACH_RETRIEVER_QUERY_BUDGET_MS;
    uint32_t modes = reach_retriever_select_modes(query);
    int32_t any_pass_ok = 0;

    if (modes & REACH_RETRIEVER_MODE_NAME)
    {
        any_pass_ok |=
            reach_retriever_collect(provider, "name", query_utf8, deadline_ms, &candidates) ==
            REACH_OK;
    }
    if (modes & REACH_RETRIEVER_MODE_PATH)
    {
        any_pass_ok |=
            reach_retriever_collect(provider, "path", query_utf8, deadline_ms, &candidates) ==
            REACH_OK;
    }
    if (modes & REACH_RETRIEVER_MODE_REGEX)
    {
        any_pass_ok |=
            reach_retriever_collect(provider, "regex", query_utf8, deadline_ms, &candidates) ==
            REACH_OK;
    }

    if (!any_pass_ok)
    {
        return REACH_ERROR;
    }

    size_t count = reach_search_rank_candidates(query, candidates.data(), candidates.size(),
                                                REACH_SEARCH_MAX_RESULTS);
    provider->result_count = count;
    for (size_t index = 0; index < count; ++index)
    {
        provider->results[index] = candidates[index];
    }

    return REACH_OK;
}

static size_t reach_search_retriever_result_count(const reach_search_provider *provider)
{
    return provider != nullptr ? provider->result_count : 0;
}

static reach_result reach_search_retriever_result_at(const reach_search_provider *provider,
                                                     size_t index, reach_search_result *out_result)
{
    if (provider == nullptr || out_result == nullptr || index >= provider->result_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_search_candidate *candidate = &provider->results[index];
    *out_result = {};
    out_result->id = (uint32_t)(index + 1);
    reach_search_copy_utf16(out_result->title, REACH_SEARCH_RESULT_NAME_CAPACITY, candidate->name);
    reach_search_copy_utf16(out_result->subtitle, REACH_SEARCH_RESULT_PATH_CAPACITY,
                            candidate->path);
    reach_search_copy_utf16(out_result->path, REACH_SEARCH_RESULT_PATH_CAPACITY, candidate->path);
    out_result->kind = candidate->kind;
    out_result->is_directory = candidate->is_directory;
    out_result->score = candidate->score;
    return REACH_OK;
}

static void reach_search_retriever_destroy(reach_search_provider *provider)
{
    if (provider == nullptr)
    {
        return;
    }
    reach_retriever_disconnect(provider);
    if (provider->io_event != nullptr)
    {
        CloseHandle(provider->io_event);
    }
    delete provider;
}

reach_result reach_windows_create_search_provider(reach_search_provider_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_search_provider *provider = new (std::nothrow) reach_search_provider();
    if (provider == nullptr)
    {
        return REACH_ERROR;
    }
    provider->pipe = INVALID_HANDLE_VALUE;
    provider->io_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (provider->io_event == nullptr)
    {
        delete provider;
        return REACH_ERROR;
    }

    out_port->provider = provider;
    out_port->ops.query = reach_search_retriever_query;
    out_port->ops.result_count = reach_search_retriever_result_count;
    out_port->ops.result_at = reach_search_retriever_result_at;
    out_port->ops.destroy = reach_search_retriever_destroy;
    return REACH_OK;
}
