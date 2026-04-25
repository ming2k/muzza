#include "project.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    muzza_project* proj = project_create();
    assert(proj != NULL);

    // 1. Test Media Addition and Type Identification
    int m1 = project_add_media(proj, "test_va.mp4");
    assert(m1 >= 0);
    muzza_media* media = project_get_media(proj, m1);
    assert(strcmp(media->filepath, "test_va.mp4") == 0);
    
    // Manual flag set (since we don't have a real file for decoder here)
    media->has_video = true;
    media->has_audio = true;
    media->duration = 10.0;

    // 2. Test Clip Creation with Types
    int c1 = project_add_clip(proj, m1, 0, MUZZA_CLIP_VIDEO, 0.0, 5.0, 0.0);
    int c2 = project_add_clip(proj, m1, 1, MUZZA_CLIP_AUDIO, 0.0, 5.0, 0.0);
    assert(c1 == 0);
    assert(c2 == 1);
    assert(proj->clips[c1].type == MUZZA_CLIP_VIDEO);
    assert(proj->clips[c2].type == MUZZA_CLIP_AUDIO);

    // 3. Test Clip Lookup by Type at Time
    int found = project_find_clip_at_time(proj, 2.5);
    assert(found == c1); // Should find video clip

    // 4. Test Duration Recalculation
    assert(proj->duration >= 5.0);
    project_add_clip(proj, m1, 0, MUZZA_CLIP_VIDEO, 10.0, 5.0, 0.0);
    assert(proj->duration >= 15.0);

    // 5. Test Waveform envelope memory allocation
    media->waveform.num_peaks = 10;
    media->waveform.mins = calloc(10, sizeof(float));
    media->waveform.maxs = calloc(10, sizeof(float));
    assert(media->waveform.mins != NULL);
    assert(media->waveform.maxs != NULL);

    // 6. Test Timeline Coordinate Math
    float tracks_x = 170.0f;
    double scroll_x = 0.0;
    float zoom = 50.0f;
    float s = 1.0f;

    float x = time_to_x(5.0, tracks_x, scroll_x, zoom, s);
    assert(fabsf(x - (tracks_x + 5.0f * zoom * s)) < 0.001f);

    double t = x_to_time(x, tracks_x, scroll_x, zoom, s);
    assert(fabs(t - 5.0) < 0.001);

    // Round-trip test
    for (double test_t = 0.0; test_t <= 20.0; test_t += 0.5) {
        float tx = time_to_x(test_t, tracks_x, scroll_x, zoom, s);
        double rt = x_to_time(tx, tracks_x, scroll_x, zoom, s);
        assert(fabs(rt - test_t) < 0.001);
    }

    // 7. Test Cursor-Anchored Zoom
    double test_scroll = 0.0;
    float test_zoom = 50.0f;
    float cursor_x = tracks_x + 200.0f; // cursor at some pixel position
    double time_before = x_to_time(cursor_x, tracks_x, test_scroll, test_zoom, s);
    apply_cursor_anchored_zoom(&test_scroll, &test_zoom, tracks_x, cursor_x, 1.5f, s);
    double time_after = x_to_time(cursor_x, tracks_x, test_scroll, test_zoom, s);
    assert(fabs(time_after - time_before) < 0.001);
    assert(test_zoom == 75.0f);

    // 8. Test Track Model Defaults
    assert(proj->tracks[0].height == MUZZA_DEFAULT_TRACK_HEIGHT);
    assert(proj->tracks[0].gain == 1.0f);
    assert(proj->tracks[0].muted == false);
    assert(proj->tracks[0].locked == false);

    // 9. Test Ripple Delete
    int c3 = project_add_clip(proj, m1, 2, MUZZA_CLIP_VIDEO, 20.0, 5.0, 0.0);
    assert(c3 >= 0);
    project_ripple_delete_clip(proj, c1);
    // c1 was at track 0, start 0.0, dur 5.0
    // remaining clips: c2 (track1, 0-5), mid (track0, 10-15), c3 (track2, 20-25)
    // after ripple: only track 0 shifts; mid shifts to 5-10
    assert(proj->num_clips == 3);
    assert(proj->clips[1].track_index == 0);
    assert(fabs(proj->clips[1].tl_start - 5.0) < 0.001);
    assert(proj->clips[2].track_index == 2);
    assert(fabs(proj->clips[2].tl_start - 20.0) < 0.001);

    // 10. Test Move Clip Overlap Rejection
    assert(project_move_clip(proj, 2, 6.0, 0) == MUZZA_MOVE_REJECTED); // overlaps shifted mid
    assert(project_move_clip(proj, 2, 25.0, 2) == MUZZA_MOVE_OK);

    // 11. Test Destruction safety
    project_destroy(proj);
    printf("Muzza Core Regression Tests Passed.\n");
    return 0;
}
