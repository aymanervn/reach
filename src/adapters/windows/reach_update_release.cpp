#include "reach_update_release.h"

#include <windows.h>
#include <bcrypt.h>

#include <vector>

static const char reach_app_update_digest_prefix[] = "sha256:";
static const size_t reach_app_update_digest_hex_length = 64;

static size_t reach_json_skip_space(const std::string &json, size_t index)
{
    while (index < json.size() && (json[index] == ' ' || json[index] == '\t' ||
                                   json[index] == '\r' || json[index] == '\n'))
    {
        ++index;
    }
    return index;
}

static int32_t reach_json_read_string(const std::string &json, size_t *index, std::string *out)
{
    if (*index >= json.size() || json[*index] != '"')
    {
        return 0;
    }
    ++*index;
    std::string value;
    while (*index < json.size())
    {
        char c = json[*index];
        if (c == '"')
        {
            ++*index;
            if (out != nullptr)
            {
                *out = value;
            }
            return 1;
        }
        if (c == '\\')
        {
            if (*index + 1 >= json.size())
            {
                return 0;
            }
            char escaped = json[*index + 1];
            if (escaped == 'u')
            {
                if (*index + 6 > json.size())
                {
                    return 0;
                }
                value.push_back('?');
                *index += 6;
                continue;
            }
            value.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
            *index += 2;
            continue;
        }
        value.push_back(c);
        ++*index;
    }
    return 0;
}

static int32_t reach_json_skip_value(const std::string &json, size_t *index)
{
    if (*index >= json.size())
    {
        return 0;
    }
    char c = json[*index];
    if (c == '"')
    {
        return reach_json_read_string(json, index, nullptr);
    }
    if (c == '{' || c == '[')
    {
        int32_t depth = 0;
        while (*index < json.size())
        {
            char current = json[*index];
            if (current == '"')
            {
                if (!reach_json_read_string(json, index, nullptr))
                {
                    return 0;
                }
                continue;
            }
            if (current == '{' || current == '[')
            {
                ++depth;
            }
            else if (current == '}' || current == ']')
            {
                --depth;
                if (depth == 0)
                {
                    ++*index;
                    return 1;
                }
            }
            ++*index;
        }
        return 0;
    }
    size_t start = *index;
    while (*index < json.size() && json[*index] != ',' && json[*index] != '}' &&
           json[*index] != ']' && json[*index] != ' ' && json[*index] != '\r' &&
           json[*index] != '\n' && json[*index] != '\t')
    {
        ++*index;
    }
    return *index > start;
}

static int32_t reach_json_find_member(const std::string &json, size_t object, const char *key,
                                      size_t *out_value)
{
    size_t index = reach_json_skip_space(json, object);
    if (index >= json.size() || json[index] != '{')
    {
        return 0;
    }
    ++index;
    for (;;)
    {
        index = reach_json_skip_space(json, index);
        if (index >= json.size() || json[index] == '}')
        {
            return 0;
        }
        std::string name;
        if (!reach_json_read_string(json, &index, &name))
        {
            return 0;
        }
        index = reach_json_skip_space(json, index);
        if (index >= json.size() || json[index] != ':')
        {
            return 0;
        }
        index = reach_json_skip_space(json, index + 1);
        if (name == key)
        {
            *out_value = index;
            return 1;
        }
        if (!reach_json_skip_value(json, &index))
        {
            return 0;
        }
        index = reach_json_skip_space(json, index);
        if (index < json.size() && json[index] == ',')
        {
            ++index;
        }
    }
}

static int32_t reach_json_member_string(const std::string &json, size_t object, const char *key,
                                        std::string *out)
{
    size_t value = 0;
    return reach_json_find_member(json, object, key, &value) &&
           reach_json_read_string(json, &value, out);
}

static int32_t reach_json_member_uint64(const std::string &json, size_t object, const char *key,
                                        uint64_t *out)
{
    size_t value = 0;
    if (!reach_json_find_member(json, object, key, &value))
    {
        return 0;
    }
    uint64_t result = 0;
    size_t start = value;
    while (value < json.size() && json[value] >= '0' && json[value] <= '9')
    {
        result = result * 10 + (uint64_t)(json[value] - '0');
        ++value;
    }
    *out = result;
    return value > start;
}

static int32_t reach_app_update_hex_value(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static int32_t reach_app_update_digest_valid(const std::string &digest)
{
    size_t prefix_length = sizeof(reach_app_update_digest_prefix) - 1;
    if (digest.size() != prefix_length + reach_app_update_digest_hex_length ||
        digest.compare(0, prefix_length, reach_app_update_digest_prefix) != 0)
    {
        return 0;
    }
    for (size_t index = prefix_length; index < digest.size(); ++index)
    {
        if (reach_app_update_hex_value(digest[index]) < 0)
        {
            return 0;
        }
    }
    return 1;
}

int32_t reach_windows_app_update_parse_release(const std::string &json,
                                               reach_windows_app_update_release *out_release)
{
    if (out_release == nullptr)
    {
        return 0;
    }
    *out_release = {};
    size_t root = reach_json_skip_space(json, 0);
    std::string tag;
    if (!reach_json_member_string(json, root, "tag_name", &tag) || tag.empty())
    {
        return 0;
    }
    std::string asset_name = "reach-" + tag + "-Release.zip";

    size_t assets = 0;
    if (!reach_json_find_member(json, root, "assets", &assets) || json[assets] != '[')
    {
        return 0;
    }
    size_t index = assets + 1;
    for (;;)
    {
        index = reach_json_skip_space(json, index);
        if (index >= json.size() || json[index] != '{')
        {
            return 0;
        }
        std::string name;
        if (reach_json_member_string(json, index, "name", &name) && name == asset_name)
        {
            reach_windows_app_update_release release = {};
            release.tag = tag;
            release.asset_name = asset_name;
            if (!reach_json_member_string(json, index, "browser_download_url",
                                          &release.asset_url) ||
                !reach_json_member_string(json, index, "digest", &release.digest) ||
                !reach_json_member_uint64(json, index, "size", &release.asset_size) ||
                release.asset_url.empty() || release.asset_size == 0 ||
                !reach_app_update_digest_valid(release.digest))
            {
                return 0;
            }
            *out_release = release;
            return 1;
        }
        if (!reach_json_skip_value(json, &index))
        {
            return 0;
        }
        index = reach_json_skip_space(json, index);
        if (index >= json.size() || json[index] != ',')
        {
            return 0;
        }
        ++index;
    }
}

static int32_t reach_app_update_hash_file(const wchar_t *path, uint8_t out_hash[32])
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    int32_t ok = 0;
    if (BCRYPT_SUCCESS(
            BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) &&
        BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0)))
    {
        std::vector<uint8_t> buffer(64 * 1024);
        ok = 1;
        for (;;)
        {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), (DWORD)buffer.size(), &read, nullptr))
            {
                ok = 0;
                break;
            }
            if (read == 0)
            {
                break;
            }
            if (!BCRYPT_SUCCESS(BCryptHashData(hash, buffer.data(), read, 0)))
            {
                ok = 0;
                break;
            }
        }
        if (ok && !BCRYPT_SUCCESS(BCryptFinishHash(hash, out_hash, 32, 0)))
        {
            ok = 0;
        }
    }
    if (hash != nullptr)
    {
        BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    CloseHandle(file);
    return ok;
}

int32_t reach_windows_app_update_file_matches_digest(const wchar_t *path, const char *digest)
{
    if (path == nullptr || digest == nullptr || !reach_app_update_digest_valid(digest))
    {
        return 0;
    }
    uint8_t hash[32] = {};
    if (!reach_app_update_hash_file(path, hash))
    {
        return 0;
    }
    const char *hex = digest + sizeof(reach_app_update_digest_prefix) - 1;
    for (size_t index = 0; index < sizeof(hash); ++index)
    {
        int32_t high = reach_app_update_hex_value(hex[index * 2]);
        int32_t low = reach_app_update_hex_value(hex[index * 2 + 1]);
        if (((high << 4) | low) != hash[index])
        {
            return 0;
        }
    }
    return 1;
}
