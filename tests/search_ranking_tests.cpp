#include "reach/support/search_types.h"

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = (uint16_t)src[index];
        ++index;
    }
    dst[index] = 0;
}

int main()
{
    int failed = 0;

    reach_search_candidate candidates[4] = {};
    copy_ascii(candidates[0].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "random-brave-notes.txt");
    copy_ascii(candidates[0].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
               "C:\\Docs\\random-brave-notes.txt");
    candidates[0].kind = reach_search_classify_result(candidates[0].path, 0);

    copy_ascii(candidates[1].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "brave-helper.exe");
    copy_ascii(candidates[1].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
               "C:\\Program Files\\Brave\\brave-helper.exe");
    candidates[1].kind = reach_search_classify_result(candidates[1].path, 0);

    copy_ascii(candidates[2].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "brave.exe");
    copy_ascii(candidates[2].path, REACH_SEARCH_RESULT_PATH_CAPACITY,
               "C:\\Program Files\\Brave\\brave.exe");
    candidates[2].kind = reach_search_classify_result(candidates[2].path, 0);

    copy_ascii(candidates[3].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "Other.txt");
    copy_ascii(candidates[3].path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\brave\\Other.txt");
    candidates[3].kind = reach_search_classify_result(candidates[3].path, 0);

    uint16_t query[16] = {};
    copy_ascii(query, 16, "brave");
    size_t count = reach_search_rank_candidates(query, candidates, 4, REACH_SEARCH_MAX_RESULTS);
    failed += expect(count == 4);
    failed += expect(candidates[0].name[0] == 'b' && candidates[0].name[5] == '.');
    failed += expect(candidates[1].name[6] == 'h');

    uint16_t path[260] = {};
    copy_ascii(path, 260, "C:\\Users\\me\\photo.jpg");
    failed += expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_PHOTO);
    copy_ascii(path, 260, "C:\\Users\\me\\movie.mp4");
    failed += expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_VIDEO);
    copy_ascii(path, 260, "C:\\Users\\me\\song.flac");
    failed += expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_MUSIC);
    copy_ascii(path, 260, "C:\\Users\\me\\doc.pdf");
    failed += expect(reach_search_classify_result(path, 0) == REACH_SEARCH_RESULT_DOCUMENT);
    copy_ascii(path, 260, "C:\\Users\\me\\folder");
    failed += expect(reach_search_classify_result(path, 1) == REACH_SEARCH_RESULT_FOLDER);

    reach_search_candidate many[8] = {};
    for (size_t index = 0; index < 8; ++index)
    {
        copy_ascii(many[index].name, REACH_SEARCH_RESULT_NAME_CAPACITY, "brave.exe");
        copy_ascii(many[index].path, REACH_SEARCH_RESULT_PATH_CAPACITY, "C:\\Apps\\brave.exe");
        many[index].kind = REACH_SEARCH_RESULT_APP;
    }
    failed += expect(reach_search_rank_candidates(query, many, 8, REACH_SEARCH_MAX_RESULTS) ==
                     REACH_SEARCH_MAX_RESULTS);

    return failed == 0 ? 0 : 1;
}
