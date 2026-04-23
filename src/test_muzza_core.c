#include "project.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    // Note: find_clip_at_time_by_type is internal to playback_controller, 
    // but project_find_clip_at_time should prefer video.
    int found = project_find_clip_at_time(proj, 2.5);
    assert(found == c1); // Should find video clip

    // 4. Test Duration Recalculation
    assert(proj->duration >= 5.0);
    project_add_clip(proj, m1, 0, MUZZA_CLIP_VIDEO, 10.0, 5.0, 0.0);
    assert(proj->duration >= 15.0);

    // 5. Test Waveform peak memory allocation
    media->waveform.num_peaks = 10;
    media->waveform.peaks = calloc(10, sizeof(float));
    assert(media->waveform.peaks != NULL);

    // 6. Test Destruction safety
    project_destroy(proj);
    printf("Muzza Core Regression Tests Passed.\n");
    return 0;
}
