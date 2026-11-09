#ifndef REACH_PACKAGE_STORE_PATH_H
#define REACH_PACKAGE_STORE_PATH_H

#include <stddef.h>
#include <stdint.h>

int32_t reach_package_store_path_split(const wchar_t *path, wchar_t *out_family_name,
                                       size_t family_name_count, const wchar_t **out_relative);

#endif
