#include "project.h"
#include "path_utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char joined[256];
    char parent[256];
    const char* save_path = "/tmp/muzza_project_roundtrip.muzza";
    muzza_project* proj = project_create();
    muzza_project* loaded = NULL;
    int media_id = -1;
    int clip_index = -1;

    assert(muzza_path_has_extension("clip.mp4", ".mp4"));
    assert(!muzza_path_has_extension("clip.mov", ".mp4"));
    muzza_path_join(joined, sizeof(joined), "/tmp/demo", "clip.mp4");
    assert(strcmp(joined, "/tmp/demo/clip.mp4") == 0);
    assert(strcmp(muzza_path_basename("/tmp/demo/clip.mp4"), "clip.mp4") == 0);
    assert(strcmp(muzza_path_basename("clip.mp4"), "clip.mp4") == 0);
    muzza_path_copy(parent, sizeof(parent), "/tmp/demo/subdir");
    muzza_path_parent_in_place(parent, sizeof(parent));
    assert(strcmp(parent, "/tmp/demo") == 0);

    assert(proj != NULL);
    assert(proj->duration == MUZZA_DEFAULT_DURATION);

    media_id = project_add_media(proj, "example.mp4");
    assert(media_id == 0);
    assert(project_add_media(proj, "example.mp4") == 0);
    assert(proj->num_media == 1);
    assert(project_add_media(proj, "broll.mp4") == 1);
    assert(proj->num_media == 2);

    clip_index = project_add_clip(proj, media_id, 2, 12.5, 4.0, 1.25);
    assert(clip_index == 0);
    assert(project_add_clip(proj, 1, 1, 3.0, 2.0, 0.0) == 1);
    proj->clips[clip_index].opacity = 0.75f;
    proj->clips[clip_index].scale = 1.2f;
    proj->clips[clip_index].selected = true;
    proj->track_heights[2] = 88.0f;

    assert(project_find_clip_at_time(proj, 13.0) == clip_index);
    project_remove_media(proj, 1);
    assert(proj->num_media == 1);
    assert(proj->num_clips == 1);
    assert(project_find_clip_at_time(proj, 13.0) == clip_index);
    assert(project_save(proj, save_path));
    project_destroy(proj);

    loaded = project_load(save_path);
    assert(loaded != NULL);
    assert(loaded->num_media == 1);
    assert(loaded->num_clips == 1);
    assert(strcmp(loaded->media_pool[0].filepath, "example.mp4") == 0);
    assert(loaded->media_pool[0].id == 0);
    assert(loaded->clips[0].track_index == 2);
    assert(loaded->clips[0].selected);
    assert(loaded->track_heights[2] == 88.0f);
    assert(project_find_clip_at_time(loaded, 13.0) == 0);

    remove(save_path);
    project_destroy(loaded);
    return 0;
}
