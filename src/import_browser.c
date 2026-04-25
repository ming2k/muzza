#include "import_browser.h"

#include "path_utils.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int compare_browser_entry(const void* lhs_ptr, const void* rhs_ptr) {
    const muzza_browser_entry* lhs = lhs_ptr;
    const muzza_browser_entry* rhs = rhs_ptr;

    if (lhs->is_dir != rhs->is_dir) {
        return lhs->is_dir ? -1 : 1;
    }

    return strcmp(lhs->name, rhs->name);
}

void import_browser_init(muzza_import_browser_state* state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->selected_index = -1;
}

void import_browser_open(muzza_import_browser_state* state) {
    if (!state) {
        return;
    }

    state->visible = true;
    state->selected_index = -1;
    state->scroll = 0;
    import_browser_refresh(state);
}

void import_browser_close(muzza_import_browser_state* state) {
    if (!state) {
        return;
    }

    state->visible = false;
    state->selected_index = -1;
    state->scroll = 0;
    state->is_dragging_scroll = false;
}

void import_browser_refresh(muzza_import_browser_state* state) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    muzza_browser_entry sorted[MUZZA_IMPORT_BROWSER_MAX_FILES];
    int sorted_count = 0;

    if (!state) {
        return;
    }

    if (state->current_dir[0] == '\0') {
        if (!getcwd(state->current_dir, sizeof(state->current_dir))) {
            muzza_path_copy(state->current_dir, sizeof(state->current_dir), "/");
        }
    }

    dir = opendir(state->current_dir);
    if (!dir) {
        muzza_path_copy(state->current_dir, sizeof(state->current_dir), "/");
        dir = opendir(state->current_dir);
        if (!dir) {
            state->num_files = 0;
            state->selected_index = -1;
            state->scroll = 0;
            return;
        }
    }

    state->num_files = 0;
    if (strcmp(state->current_dir, "/") != 0 && state->num_files < MUZZA_IMPORT_BROWSER_MAX_FILES) {
        muzza_browser_entry* file = &state->files[state->num_files++];
        muzza_path_copy(file->name, sizeof(file->name), "..");
        file->is_dir = true;
        file->is_parent_dir = true;
    }

    while ((entry = readdir(dir)) != NULL && sorted_count < MUZZA_IMPORT_BROWSER_MAX_FILES) {
        muzza_browser_entry* file = &sorted[sorted_count];
        struct stat st;
        char full_path[1024];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        muzza_path_join(full_path, sizeof(full_path), state->current_dir, entry->d_name);
        if (stat(full_path, &st) != 0) {
            continue;
        }

        muzza_path_copy(file->name, sizeof(file->name), entry->d_name);
        file->is_dir = S_ISDIR(st.st_mode);
        file->is_parent_dir = false;
        sorted_count++;
    }

    closedir(dir);

    qsort(sorted, (size_t)sorted_count, sizeof(sorted[0]), compare_browser_entry);
    for (int i = 0; i < sorted_count && state->num_files < MUZZA_IMPORT_BROWSER_MAX_FILES; ++i) {
        state->files[state->num_files++] = sorted[i];
    }

    if (state->selected_index >= state->num_files) {
        state->selected_index = -1;
    }
    if (state->scroll < 0) {
        state->scroll = 0;
    }
}

bool import_browser_activate_entry(muzza_import_browser_state* state, int file_index, char* out_path, int out_path_size) {
    const muzza_browser_entry* file = NULL;

    if (!state || file_index < 0 || file_index >= state->num_files) {
        return false;
    }

    file = &state->files[file_index];
    if (file->is_parent_dir) {
        muzza_path_parent_in_place(state->current_dir, sizeof(state->current_dir));
        state->selected_index = -1;
        state->scroll = 0;
        import_browser_refresh(state);
        return false;
    }

    if (file->is_dir) {
        char next_dir[sizeof(state->current_dir)];
        muzza_path_join(next_dir, sizeof(next_dir), state->current_dir, file->name);
        muzza_path_copy(state->current_dir, sizeof(state->current_dir), next_dir);
        state->selected_index = -1;
        state->scroll = 0;
        import_browser_refresh(state);
        return false;
    }

    state->selected_index = file_index;
    if (out_path && out_path_size > 0) {
        muzza_path_join(out_path, (size_t)out_path_size, state->current_dir, file->name);
    }
    return true;
}
