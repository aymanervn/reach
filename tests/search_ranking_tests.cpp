#include "reach/support/util.h"
#include "reach/support/search_catalog.h"
#include "reach/support/search_types.h"

#include <stdio.h>

static int failed = 0;

static int ascii_equals(const char *text, const char *expected)
{
    size_t index = 0;
    while (text[index] != 0 && expected[index] != 0)
    {
        if (text[index] != expected[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0 && expected[index] == 0;
}

static int name_equals(const uint16_t *name, const char *expected)
{
    size_t index = 0;
    while (name[index] != 0 && expected[index] != 0)
    {
        if (name[index] != (uint16_t)expected[index])
        {
            return 0;
        }
        ++index;
    }
    return name[index] == 0 && expected[index] == 0;
}

static void print_name(const uint16_t *name)
{
    for (size_t index = 0; name[index] != 0; ++index)
    {
        fputc(name[index] < 128 ? (int)name[index] : '?', stderr);
    }
}

static void expect(int condition, const char *what)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failed;
    }
}

static void expect_order(const char *query_text, const char *const *paths, size_t path_count,
                         const char *const *expected, size_t expected_count)
{
    reach_search_candidate candidates[16] = {};
    for (size_t index = 0; index < path_count; ++index)
    {
        reach_copy_ascii_to_utf16(candidates[index].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                                  paths[index]);
        const char *name = paths[index];
        for (const char *scan = paths[index]; *scan != 0; ++scan)
        {
            if (*scan == '\\')
            {
                name = scan + 1;
            }
        }
        reach_copy_ascii_to_utf16(candidates[index].name, REACH_SEARCH_RESULT_NAME_CAPACITY, name);
        candidates[index].kind = reach_search_classify_result(candidates[index].path, 0);
    }

    uint16_t query[REACH_SEARCH_QUERY_CAPACITY] = {};
    reach_copy_ascii_to_utf16(query, REACH_SEARCH_QUERY_CAPACITY, query_text);
    size_t count =
        reach_search_rank_candidates(query, candidates, path_count, REACH_SEARCH_MAX_RESULTS);

    if (count != expected_count)
    {
        fprintf(stderr, "FAIL: query '%s' returned %zu results, expected %zu\n", query_text, count,
                expected_count);
        ++failed;
        return;
    }

    for (size_t index = 0; index < expected_count; ++index)
    {
        if (!name_equals(candidates[index].name, expected[index]))
        {
            fprintf(stderr, "FAIL: query '%s' position %zu: expected '%s', got '", query_text,
                    index, expected[index]);
            print_name(candidates[index].name);
            fprintf(stderr, "'\n");
            ++failed;
        }
    }
}

static void test_shorter_stem_wins_within_tier()
{
    static const char *const paths[] = {"C:\\Program Files\\Steam\\steamwebhelper.exe",
                                        "C:\\Program Files\\Steam\\steam.exe"};
    static const char *const expected[] = {"steam.exe", "steamwebhelper.exe"};
    expect_order("stea", paths, 2, expected, 2);
}

static void test_launchable_outranks_shorter_plain_file()
{
    static const char *const paths[] = {"C:\\logs\\stea.log",
                                        "C:\\Program Files\\Steam\\steam.exe"};
    static const char *const expected[] = {"steam.exe", "stea.log"};
    expect_order("stea", paths, 2, expected, 2);
}

static void test_short_msc_beats_long_exe()
{
    static const char *const paths[] = {"C:\\Windows\\System32\\perf_helper_service.exe",
                                        "C:\\Windows\\System32\\perfmon.msc"};
    static const char *const expected[] = {"perfmon.msc", "perf_helper_service.exe"};
    expect_order("perf", paths, 2, expected, 2);
}

static void test_exe_beats_msc_on_equal_stem()
{
    static const char *const paths[] = {"C:\\Windows\\System32\\perfmon.msc",
                                        "C:\\Windows\\System32\\perfmon.exe"};
    static const char *const expected[] = {"perfmon.exe", "perfmon.msc"};
    expect_order("perfmon", paths, 2, expected, 2);
}

static void test_exact_stem_beats_prefix()
{
    static const char *const paths[] = {"C:\\Windows\\System32\\controlpanel.exe",
                                        "C:\\Windows\\System32\\control.exe"};
    static const char *const expected[] = {"control.exe", "controlpanel.exe"};
    expect_order("control", paths, 2, expected, 2);
}

static void test_non_matching_candidates_are_dropped()
{
    static const char *const paths[] = {"C:\\Windows\\System32\\notepad.exe",
                                        "C:\\Windows\\System32\\calc.exe"};
    static const char *const expected[] = {"notepad.exe"};
    expect_order("notepad", paths, 2, expected, 1);
}

static void test_system_location_beats_transient()
{
    reach_search_candidate candidates[2] = {};
    reach_copy_ascii_to_utf16(candidates[0].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                              "C:\\Windows\\WinSxS\\amd64_x\\mstsc.exe");
    reach_copy_ascii_to_utf16(candidates[0].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "mstsc.exe");
    reach_copy_ascii_to_utf16(candidates[1].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                              "C:\\Windows\\System32\\mstsc.exe");
    reach_copy_ascii_to_utf16(candidates[1].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "mstsc.exe");

    uint16_t query[16] = {};
    reach_copy_ascii_to_utf16(query, 16, "mstsc");
    size_t count = reach_search_rank_candidates(query, candidates, 2, REACH_SEARCH_MAX_RESULTS);
    expect(count == 2, "both copies of mstsc are returned");
    expect(reach_search_classify_location(candidates[0].path) == REACH_SEARCH_LOCATION_SYSTEM,
           "system32 copy ranks above the winsxs copy");
}

static void test_word_prefix_matches_inside_name()
{
    static const char *const paths[] = {"C:\\Tools\\myschedulerx.exe",
                                        "C:\\Tools\\task-scheduler.exe"};
    static const char *const expected[] = {"task-scheduler.exe", "myschedulerx.exe"};
    expect_order("scheduler", paths, 2, expected, 2);
}

static void test_pinned_candidate_leads()
{
    reach_search_candidate candidates[2] = {};
    reach_copy_ascii_to_utf16(candidates[0].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "path.exe");
    reach_copy_ascii_to_utf16(candidates[0].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                              "C:\\Windows\\System32\\path.exe");
    reach_copy_ascii_to_utf16(candidates[1].name, REACH_SEARCH_RESULT_NAME_CAPACITY,
                              "Environment Variables");
    reach_copy_ascii_to_utf16(candidates[1].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                              "C:\\Windows\\System32\\rundll32.exe");
    candidates[1].source = REACH_SEARCH_SOURCE_CATALOG;
    candidates[1].match_tier = REACH_SEARCH_MATCH_EXACT;
    candidates[1].pinned = 1;

    uint16_t query[16] = {};
    reach_copy_ascii_to_utf16(query, 16, "path");
    size_t count = reach_search_rank_candidates(query, candidates, 2, REACH_SEARCH_MAX_RESULTS);
    expect(count == 2, "pinned alias keeps file results below it");
    expect(name_equals(candidates[0].name, "Environment Variables"),
           "pinned alias occupies the first row");
    expect(name_equals(candidates[1].name, "path.exe"), "file result follows the pinned alias");
}

static void test_catalog_alias_matching()
{
    uint16_t query[32] = {};
    const reach_search_alias_entry *environment = nullptr;
    const reach_search_alias_entry *scheduler = nullptr;

    for (size_t index = 0; index < reach_search_alias_count(); ++index)
    {
        const reach_search_alias_entry *entry = reach_search_alias_at(index);
        if (ascii_equals(entry->display, "Environment Variables"))
        {
            environment = entry;
        }
        if (ascii_equals(entry->display, "Task Scheduler"))
        {
            scheduler = entry;
        }
    }

    expect(environment != nullptr, "alias table contains Environment Variables");
    expect(scheduler != nullptr, "alias table contains Task Scheduler");
    if (environment == nullptr || scheduler == nullptr)
    {
        return;
    }

    reach_copy_ascii_to_utf16(query, 32, "path");
    expect(reach_search_alias_match(environment, query) == REACH_SEARCH_MATCH_EXACT,
           "'path' exactly matches the Environment Variables alias");

    reach_copy_ascii_to_utf16(query, 32, "env");
    expect(reach_search_alias_match(environment, query) == REACH_SEARCH_MATCH_EXACT,
           "'env' exactly matches the Environment Variables alias");

    reach_copy_ascii_to_utf16(query, 32, "task sched");
    expect(reach_search_alias_match(scheduler, query) == REACH_SEARCH_MATCH_PREFIX,
           "'task sched' prefix-matches Task Scheduler");

    reach_copy_ascii_to_utf16(query, 32, "sched");
    expect(reach_search_alias_match(scheduler, query) == REACH_SEARCH_MATCH_PREFIX,
           "'sched' matches the schedule alias");

    reach_copy_ascii_to_utf16(query, 32, "zzzz");
    expect(reach_search_alias_match(scheduler, query) == REACH_SEARCH_MATCH_NONE,
           "unrelated query does not match Task Scheduler");
}

static void test_launch_classification()
{
    uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\a\\tool.exe");
    expect(reach_search_classify_launch(path, 0) == REACH_SEARCH_LAUNCH_EXE, "exe launch class");
    expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_APP, "exe is an app");

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\a\\snap.msc");
    expect(reach_search_classify_launch(path, 0) == REACH_SEARCH_LAUNCH_MSC, "msc launch class");
    expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_APP, "msc is an app");

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\a\\setup.msi");
    expect(reach_search_classify_launch(path, 0) == REACH_SEARCH_LAUNCH_MSI, "msi launch class");

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\a\\applet.cpl");
    expect(reach_search_classify_launch(path, 0) == REACH_SEARCH_LAUNCH_CPL, "cpl launch class");

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\a\\run.bat");
    expect(reach_search_classify_launch(path, 0) == REACH_SEARCH_LAUNCH_SCRIPT, "bat launch class");

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\a\\notes.txt");
    expect(reach_search_classify_launch(path, 0) == REACH_SEARCH_LAUNCH_NONE,
           "txt is not launchable");

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\a\\tool.exe");
    expect(reach_search_classify_launch(path, 1) == REACH_SEARCH_LAUNCH_NONE,
           "directories are not launchable");
}

static void test_stem_length_uses_first_dot()
{
    uint16_t name[REACH_SEARCH_RESULT_NAME_CAPACITY] = {};

    reach_copy_ascii_to_utf16(name, REACH_SEARCH_RESULT_NAME_CAPACITY, "python3.11.exe");
    expect(reach_search_match_stem_length(name) == 7, "match stem stops at the first dot");

    reach_copy_ascii_to_utf16(name, REACH_SEARCH_RESULT_NAME_CAPACITY, "steam.exe");
    expect(reach_search_match_stem_length(name) == 5, "single-dot stem length");

    reach_copy_ascii_to_utf16(name, REACH_SEARCH_RESULT_NAME_CAPACITY, "makefile");
    expect(reach_search_match_stem_length(name) == 8, "extensionless stem length");
}

static void test_media_classification_unchanged()
{
    uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};

    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\Users\\me\\photo.jpg");
    expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_PHOTO, "photo kind");
    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\Users\\me\\movie.mp4");
    expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_VIDEO, "video kind");
    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\Users\\me\\song.flac");
    expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_MUSIC, "music kind");
    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\Users\\me\\doc.pdf");
    expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_DOCUMENT, "document kind");
    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\Users\\me\\folder");
    expect(reach_search_classify_result(path, 1) == REACH_SEARCH_RESULT_FOLDER, "folder kind");
    reach_copy_ascii_to_utf16(path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                              "C:\\Users\\me\\shortcut.lnk");
    expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_FILE, "lnk stays a file");
}

static void test_result_cap_is_respected()
{
    reach_search_candidate many[REACH_SEARCH_MAX_RESULTS + 1] = {};
    for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS + 1; ++index)
    {
        reach_copy_ascii_to_utf16(many[index].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "brave.exe");
        reach_copy_ascii_to_utf16(many[index].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
                                  "C:\\Apps\\brave.exe");
        many[index].kind = REACH_SEARCH_RESULT_APP;
    }

    uint16_t query[16] = {};
    reach_copy_ascii_to_utf16(query, 16, "brave");
    expect(reach_search_rank_candidates(query, many, REACH_SEARCH_MAX_RESULTS + 1,
                                        REACH_SEARCH_MAX_RESULTS) == REACH_SEARCH_MAX_RESULTS,
           "result count is capped");
}

int main()
{
    test_shorter_stem_wins_within_tier();
    test_launchable_outranks_shorter_plain_file();
    test_short_msc_beats_long_exe();
    test_exe_beats_msc_on_equal_stem();
    test_exact_stem_beats_prefix();
    test_non_matching_candidates_are_dropped();
    test_system_location_beats_transient();
    test_word_prefix_matches_inside_name();
    test_pinned_candidate_leads();
    test_catalog_alias_matching();
    test_launch_classification();
    test_stem_length_uses_first_dot();
    test_media_classification_unchanged();
    test_result_cap_is_respected();

    return failed == 0 ? 0 : 1;
}
