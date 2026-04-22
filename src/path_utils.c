#include "path_utils.h"

#include <stdio.h>
#include <string.h>

bool muzza_path_has_extension(const char* path, const char* ext) {
    size_t path_len = 0;
    size_t ext_len = 0;

    if (!path || !ext) {
        return false;
    }

    path_len = strlen(path);
    ext_len = strlen(ext);
    if (path_len < ext_len) {
        return false;
    }

    return strcmp(path + path_len - ext_len, ext) == 0;
}

void muzza_path_copy(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

void muzza_path_join(char* dst, size_t dst_size, const char* dir, const char* name) {
    if (!dst || dst_size == 0) {
        return;
    }

    if (!dir || dir[0] == '\0') {
        muzza_path_copy(dst, dst_size, name);
        return;
    }

    if (!name || name[0] == '\0') {
        muzza_path_copy(dst, dst_size, dir);
        return;
    }

    if (strcmp(dir, "/") == 0) {
        snprintf(dst, dst_size, "/%s", name);
    } else {
        snprintf(dst, dst_size, "%s/%s", dir, name);
    }
}

void muzza_path_parent_in_place(char* path, size_t path_size) {
    char* slash = NULL;

    if (!path || path_size == 0) {
        return;
    }

    if (path[0] == '\0' || strcmp(path, "/") == 0) {
        muzza_path_copy(path, path_size, "/");
        return;
    }

    slash = strrchr(path, '/');
    if (!slash) {
        muzza_path_copy(path, path_size, "/");
        return;
    }

    if (slash == path) {
        path[1] = '\0';
        return;
    }

    *slash = '\0';
}

const char* muzza_path_basename(const char* path) {
    const char* slash = NULL;

    if (!path || path[0] == '\0') {
        return "";
    }

    slash = strrchr(path, '/');
    if (!slash) {
        return path;
    }

    if (slash[1] == '\0') {
        return slash;
    }

    return slash + 1;
}
