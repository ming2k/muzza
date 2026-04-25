#include "project.h"
#include "path_utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char joined[256];
    char parent[256];
    const char* save_path = "/tmp/muzza_project_roundtrip.muzza";
    const char* invalid_path = "/tmp/muzza_invalid.muzza";
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

    // Stable media IDs
    assert(proj->media_pool[0].uid > 0);
    assert(proj->media_pool[1].uid > 0);
    assert(proj->media_pool[0].uid != proj->media_pool[1].uid);
    uint64_t uid0 = proj->media_pool[0].uid;
    uint64_t uid1 = proj->media_pool[1].uid;

    clip_index = project_add_clip(proj, media_id, 2, MUZZA_CLIP_VIDEO, 12.5, 4.0, 1.25);
    assert(clip_index == 0);
    assert(project_add_clip(proj, 1, 1, MUZZA_CLIP_VIDEO, 3.0, 2.0, 0.0) == 1);
    proj->clips[clip_index].opacity = 0.75f;
    proj->clips[clip_index].scale = 1.2f;
    proj->clips[clip_index].selected = true;
    proj->tracks[2].height = 88.0f;

    // Stable clip IDs
    assert(proj->clips[0].uid > 0);
    assert(proj->clips[1].uid > 0);
    assert(proj->clips[0].uid != proj->clips[1].uid);
    uint64_t cuid0 = proj->clips[0].uid;
    (void)proj->clips[1].uid;

    assert(project_find_clip_at_time(proj, 13.0) == clip_index);

    // Test overlap detection
    assert(!project_clips_would_overlap(proj, -1, 2, 0.0, 1.0)); // no overlap
    assert(project_clips_would_overlap(proj, -1, 2, 13.0, 2.0)); // overlaps clip 0
    assert(!project_clips_would_overlap(proj, 0, 2, 13.0, 2.0)); // excluded self

    // Test move helper
    assert(project_move_clip(proj, 0, 20.0, 2) == MUZZA_MOVE_OK);
    assert(project_move_clip(proj, 0, 3.5, 1) == MUZZA_MOVE_REJECTED); // overlaps clip 1
    assert(project_move_clip(proj, 0, -5.0, 2) == MUZZA_MOVE_OK); // clamps to 0
    assert(proj->clips[0].tl_start == 0.0);

    // Restore position for rest of tests
    proj->clips[0].tl_start = 12.5;
    proj->clips[0].track_index = 2;
    project_recalculate_duration(proj);

    // Test trim helpers
    double new_media_in = 0.0;
    double new_dur = 0.0;
    assert(project_trim_clip_left(proj, 0, 13.0, &new_media_in, &new_dur));
    assert(proj->clips[0].tl_start == 13.0);
    assert(proj->clips[0].tl_duration == 3.5); // 12.5+4.0 - 13.0
    assert(new_media_in == 1.75); // 1.25 + (13.0 - 12.5)

    assert(project_trim_clip_right(proj, 0, 2.0));
    assert(proj->clips[0].tl_duration == 2.0);

    // Test media lookup by uid
    assert(project_get_media_by_uid(proj, uid0) == &proj->media_pool[0]);
    assert(project_get_media_by_uid(proj, uid1) == &proj->media_pool[1]);
    assert(project_get_media_by_uid(proj, 999999) == NULL);

    project_remove_media(proj, 1);
    assert(proj->num_media == 1);
    assert(proj->num_clips == 1);
    // uid of remaining media should be unchanged
    assert(proj->media_pool[0].uid == uid0);
    assert(project_find_clip_at_time(proj, 13.0) == clip_index);

    assert(project_save(proj, save_path));
    project_destroy(proj);

    loaded = project_load(save_path);
    assert(loaded != NULL);
    assert(loaded->num_media == 1);
    assert(loaded->num_clips == 1);
    assert(strcmp(loaded->media_pool[0].filepath, "example.mp4") == 0);
    assert(loaded->media_pool[0].id == 0);
    assert(loaded->media_pool[0].uid == uid0);
    assert(loaded->clips[0].uid == cuid0);
    assert(loaded->clips[0].track_index == 2);
    assert(loaded->clips[0].selected);
    assert(loaded->tracks[2].height == 88.0f);
    assert(project_find_clip_at_time(loaded, 13.0) == 0);
    project_destroy(loaded);

    // Test invalid project file load (negative duration)
    FILE* f = fopen(invalid_path, "w");
    assert(f);
    fprintf(f, "[Project]\nversion=1\nduration=10.0\n");
    fprintf(f, "[Media 0]\npath=test.mp4\n");
    fprintf(f, "[Clip 0]\nmedia=0\ntrack=0\ntype=0\nstart=0.0\ndur=-1.0\nin=0.0\n");
    fclose(f);
    loaded = project_load(invalid_path);
    assert(loaded == NULL);
    remove(invalid_path);

    // Test invalid project file load (bad media reference)
    f = fopen(invalid_path, "w");
    assert(f);
    fprintf(f, "[Project]\nversion=1\nduration=10.0\n");
    fprintf(f, "[Media 0]\npath=test.mp4\n");
    fprintf(f, "[Clip 0]\nmedia=5\ntrack=0\ntype=0\nstart=0.0\ndur=1.0\nin=0.0\n");
    fclose(f);
    loaded = project_load(invalid_path);
    assert(loaded == NULL);
    remove(invalid_path);

    // Test invalid project file load (bad track index)
    f = fopen(invalid_path, "w");
    assert(f);
    fprintf(f, "[Project]\nversion=1\nduration=10.0\n");
    fprintf(f, "[Media 0]\npath=test.mp4\n");
    fprintf(f, "[Clip 0]\nmedia=0\ntrack=99\ntype=0\nstart=0.0\ndur=1.0\nin=0.0\n");
    fclose(f);
    loaded = project_load(invalid_path);
    assert(loaded == NULL);
    remove(invalid_path);

    remove(save_path);
    printf("Project model tests passed.\n");
    return 0;
}
