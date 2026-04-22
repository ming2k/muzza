#ifndef MUZZA_PATH_UTILS_H
#define MUZZA_PATH_UTILS_H

#include <stdbool.h>
#include <stddef.h>

bool muzza_path_has_extension(const char* path, const char* ext);
void muzza_path_copy(char* dst, size_t dst_size, const char* src);
void muzza_path_join(char* dst, size_t dst_size, const char* dir, const char* name);
void muzza_path_parent_in_place(char* path, size_t path_size);
const char* muzza_path_basename(const char* path);

#endif
