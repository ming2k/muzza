#ifndef MUZZA_IMPORT_BROWSER_H
#define MUZZA_IMPORT_BROWSER_H

#include <stdbool.h>

#define MUZZA_IMPORT_BROWSER_MAX_FILES 256

typedef struct {
    char name[256];
    bool is_dir;
    bool is_parent_dir;
} muzza_browser_entry;

typedef struct {
    bool visible;
    char current_dir[512];
    muzza_browser_entry files[MUZZA_IMPORT_BROWSER_MAX_FILES];
    int num_files;
    int selected_index;
    int scroll;
    float pos_x;        /* 0 = auto-center on next show */
    float pos_y;        /* 0 = auto-center on next show */
    bool is_dragging;
    float drag_offset_x;
    float drag_offset_y;
    bool is_dragging_scroll;
} muzza_import_browser_state;

void import_browser_init(muzza_import_browser_state* state);
void import_browser_open(muzza_import_browser_state* state);
void import_browser_close(muzza_import_browser_state* state);
void import_browser_refresh(muzza_import_browser_state* state);
bool import_browser_activate_entry(muzza_import_browser_state* state, int file_index, char* out_path, int out_path_size);

#endif
