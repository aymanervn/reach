#include "reach/support/application_identity.h"

#include "reach/support/util.h"

int32_t reach_application_identity_equal(const uint16_t *path_a, const uint16_t *aumid_a,
                                          const uint16_t *path_b, const uint16_t *aumid_b)
{
    int32_t path_equal = path_a != nullptr && path_b != nullptr && path_a[0] != 0 &&
                         path_b[0] != 0 && reach_path_equals(path_a, path_b);
    int32_t aumid_equal =
        aumid_a != nullptr && aumid_b != nullptr && aumid_a[0] != 0 && aumid_b[0] != 0 &&
        reach_utf16_equal_ascii_case_insensitive(aumid_a, aumid_b);
    return path_equal || aumid_equal;
}
