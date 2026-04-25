#include "exporter.h"
#include "project.h"

#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    muzza_project* proj = project_create();
    assert(proj != NULL);

    // 1. Test create/destroy without starting
    muzza_exporter* exp = exporter_create(proj, "/tmp/muzza_test_export.mp4", NULL);
    assert(exp != NULL);
    exporter_destroy(exp);

    // 2. Test invalid path rejection
    assert(exporter_create(proj, "", NULL) == NULL);
    assert(exporter_create(proj, NULL, NULL) == NULL);
    assert(exporter_create(NULL, "/tmp/test.mp4", NULL) == NULL);

    // 3. Test cancellation before completion on empty project
    exp = exporter_create(proj, "/tmp/muzza_cancel_test.mp4", NULL);
    assert(exp != NULL);
    SDL_Thread* thread = exporter_start_thread(exp);
    assert(thread != NULL);
    exporter_request_cancel(exp);
    SDL_WaitThread(thread, NULL);
    muzza_export_status status = exporter_get_status(exp);
    assert(status == MUZZA_EXPORT_CANCELLED || status == MUZZA_EXPORT_DONE);
    exporter_destroy(exp);

    // 4. Test snapshot creation with clips
    int media_id = project_add_media(proj, "test_fixtures/generated/short_1s.mp4");
    if (media_id >= 0) {
        proj->media_pool[media_id].has_video = true;
        proj->media_pool[media_id].has_audio = true;
        proj->media_pool[media_id].duration = 1.0;
        int clip_id = project_add_clip(proj, media_id, 0, MUZZA_CLIP_VIDEO, 0.0, 1.0, 0.0);
        if (clip_id >= 0) {
            muzza_exporter* fixture_exp = exporter_create(proj, "/tmp/muzza_fixture_export.mp4", NULL);
            assert(fixture_exp != NULL);
            exporter_destroy(fixture_exp);
        }
    }

    project_destroy(proj);
    SDL_Quit();

    printf("Exporter tests passed.\n");
    return 0;
}
