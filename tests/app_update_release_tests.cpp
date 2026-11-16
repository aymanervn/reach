#include "../src/adapters/windows/reach_update_release.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

#include <string>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static const char release_json[] = R"({
  "url": "https://api.github.com/repos/aymanervn/reach/releases/1",
  "tag_name": "v1.9.4",
  "name": "v1.9.4",
  "draft": false,
  "author": { "login": "aymanervn", "name": "nested \"quoted\" {not a brace}" },
  "assets": [
    {
      "name": "reach-v1.9.4-Debug.zip",
      "uploader": { "login": "github-actions[bot]" },
      "size": 10,
      "digest": "sha256:1111111111111111111111111111111111111111111111111111111111111111",
      "browser_download_url": "https://github.com/aymanervn/reach/releases/download/v1.9.4/reach-v1.9.4-Debug.zip"
    },
    {
      "url": "https://api.github.com/repos/aymanervn/reach/releases/assets/2",
      "name": "reach-v1.9.4-Release.zip",
      "label": null,
      "uploader": { "login": "github-actions[bot]", "name": "reach-v1.9.4-Release.zip" },
      "content_type": "application/zip",
      "size": 2500536,
      "digest": "sha256:5d5a522ba72085834072e8765571d0529c56d4427de2685f60c648c530afc46d",
      "browser_download_url": "https://github.com/aymanervn/reach/releases/download/v1.9.4/reach-v1.9.4-Release.zip"
    }
  ],
  "body": "## What's Changed"
})";

static void test_parses_release_asset(void)
{
    reach_windows_app_update_release release = {};
    expect_true(reach_windows_app_update_parse_release(release_json, &release),
                "release with a Release ZIP and digest parses");
    expect_true(release.tag == "v1.9.4", "tag is read");
    expect_true(release.asset_name == "reach-v1.9.4-Release.zip", "Release ZIP is selected");
    expect_true(release.asset_url == "https://github.com/aymanervn/reach/releases/download/v1.9.4/"
                                     "reach-v1.9.4-Release.zip",
                "download URL belongs to the Release ZIP");
    expect_true(release.digest ==
                    "sha256:5d5a522ba72085834072e8765571d0529c56d4427de2685f60c648c530afc46d",
                "digest belongs to the Release ZIP");
    expect_true(release.asset_size == 2500536, "size belongs to the Release ZIP");
}

static void test_rejects_release_without_usable_asset(void)
{
    reach_windows_app_update_release release = {};
    expect_true(
        !reach_windows_app_update_parse_release(R"({"tag_name":"v1.9.5","assets":[]})", &release),
        "release whose ZIP is not uploaded yet is rejected");

    std::string no_digest = release_json;
    no_digest.replace(no_digest.find("\"digest\": \"sha256:5d5a"), 8, "\"nodigest\"");
    expect_true(!reach_windows_app_update_parse_release(no_digest, &release),
                "Release ZIP without a digest is rejected");

    std::string bad_digest = release_json;
    bad_digest.replace(bad_digest.find("sha256:5d5a"), 7, "md5:");
    expect_true(!reach_windows_app_update_parse_release(bad_digest, &release),
                "Release ZIP with a non-SHA-256 digest is rejected");

    expect_true(!reach_windows_app_update_parse_release("Not Found", &release),
                "non-JSON response is rejected");
}

static std::wstring write_temp_file(const char *contents)
{
    wchar_t directory[MAX_PATH] = {};
    wchar_t path[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, directory);
    GetTempFileNameW(directory, L"rup", 0, path);
    HANDLE file =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    WriteFile(file, contents, (DWORD)strlen(contents), &written, nullptr);
    CloseHandle(file);
    return path;
}

static void test_verifies_downloaded_file_digest(void)
{
    std::wstring not_found = write_temp_file("Not Found");
    expect_true(reach_windows_app_update_file_matches_digest(
                    not_found.c_str(),
                    "sha256:0019dfc4b32d63c1392aa264aed2253c1e0c2fb09216f8e2cc269bbfb8bb49b5"),
                "file matching its digest is accepted");
    expect_true(reach_windows_app_update_file_matches_digest(
                    not_found.c_str(),
                    "sha256:0019DFC4B32D63C1392AA264AED2253C1E0C2FB09216F8E2CC269BBFB8BB49B5"),
                "uppercase digest is accepted");
    expect_true(!reach_windows_app_update_file_matches_digest(
                    not_found.c_str(),
                    "sha256:5d5a522ba72085834072e8765571d0529c56d4427de2685f60c648c530afc46d"),
                "GitHub Not Found page is rejected against the release digest");
    expect_true(!reach_windows_app_update_file_matches_digest(not_found.c_str(), "sha256:0019"),
                "truncated digest is rejected");
    DeleteFileW(not_found.c_str());

    expect_true(!reach_windows_app_update_file_matches_digest(
                    L"C:\\reach-missing-update.zip",
                    "sha256:0019dfc4b32d63c1392aa264aed2253c1e0c2fb09216f8e2cc269bbfb8bb49b5"),
                "missing file is rejected");
}

int main(void)
{
    test_parses_release_asset();
    test_rejects_release_without_usable_asset();
    test_verifies_downloaded_file_digest();
    if (failures != 0)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("app update release tests passed\n");
    return 0;
}
