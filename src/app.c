#include "app.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#include "flux/flux.h"
#include "import_browser.h"
#include "path_utils.h"
#include "playback_controller.h"
#include "project.h"
#include "ui.h"

typedef struct {
    SDL_Window* window;
    fx_context* ctx;
    fx_surface* surface;
    muzza_project* project;
    muzza_ui_state ui;
} muzza_app;

static void normalize_process_locale(void) {
    const char* preferred = getenv("LC_ALL");

    setlocale(LC_ALL, "");

    if (!preferred || preferred[0] == '\0') {
        preferred = getenv("LC_CTYPE");
    }
    if (!preferred || preferred[0] == '\0') {
        preferred = setlocale(LC_CTYPE, NULL);
    }
    if (!preferred || preferred[0] == '\0') {
        preferred = "C.UTF-8";
    }

    if (!getenv("LC_ALL") || getenv("LC_ALL")[0] == '\0') {
        SDL_setenv_unsafe("LC_CTYPE", preferred, 1);
    }
    SDL_setenv_unsafe("LANG", preferred, 1);
}

static void sanitize_ui_after_project_change(muzza_app* app) {
    if (!app || !app->project) {
        return;
    }

    if (app->ui.timeline.selected_clip_index >= (int)app->project->num_clips) {
        app->ui.timeline.selected_clip_index = -1;
    }
    if (app->ui.timeline.active_clip_index >= (int)app->project->num_clips) {
        app->ui.timeline.active_clip_index = -1;
    }
    if (app->ui.media_panel.selected_media_index >= (int)app->project->num_media) {
        app->ui.media_panel.selected_media_index = app->project->num_media > 0 ? 0 : -1;
    }
    if (app->ui.preview.preview_media_index >= (int)app->project->num_media) {
        app->ui.preview.preview_media_index = -1;
    }
    if (app->ui.media_panel.context_menu_media_index >= (int)app->project->num_media) {
        app->ui.media_panel.context_menu_media_index = -1;
        app->ui.media_panel.show_context_menu = false;
    }
    if (app->ui.media_panel.dragged_media_index >= (int)app->project->num_media) {
        app->ui.media_panel.dragged_media_index = -1;
        app->ui.media_panel.is_dragging_media = false;
    }
}

static bool import_media_into_project(muzza_app* app, const char* path) {
    int media_id = -1;
    muzza_media* media = NULL;
    muzza_decoder* dec = NULL;

    if (!app || !app->project || !path || path[0] == '\0') {
        return false;
    }

    media_id = project_add_media(app->project, path);
    if (media_id < 0) {
        return false;
    }

    media = project_get_media(app->project, media_id);
    dec = project_ensure_media_decoder(app->project, media_id, app->ctx);
    if (!dec) {
        project_remove_media(app->project, media_id);
        return false;
    }

    if (media) {
        media->duration = decoder_get_duration(dec);
        media->has_video = decoder_has_video(dec);
        media->has_audio = decoder_has_audio(dec);
        SDL_Log(
            "Imported media: %s duration=%.3f video=%d audio=%d",
            path,
            media->duration,
            media->has_video ? 1 : 0,
            media->has_audio ? 1 : 0
        );
    }

    decoder_set_paused(dec, true);
    decoder_seek_to_time(dec, 0.0);
    app->ui.media_panel.selected_media_index = media_id;
    app->ui.preview.preview_media_index = -1;
    app->ui.timeline.selected_clip_index = -1;
    playback_reset_session(&app->ui);
    return true;
}

static int choose_track_for_insert(const muzza_project* project, double start_time, double duration) {
    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        bool conflict = false;

        for (size_t clip_index = 0; clip_index < project->num_clips; ++clip_index) {
            const muzza_clip* clip = &project->clips[clip_index];
            double clip_end = clip->tl_start + clip->tl_duration;
            double new_end = start_time + duration;

            if (clip->track_index != track) {
                continue;
            }

            if (!(new_end <= clip->tl_start || start_time >= clip_end)) {
                conflict = true;
                break;
            }
        }

        if (!conflict) {
            return track;
        }
    }

    return MUZZA_MAX_TRACKS - 1;
}

static bool insert_media_into_timeline(muzza_app* app, int media_id, double start_time_override, int track_override) {
    muzza_media* media = project_get_media(app->project, media_id);
    double start_time = 0.0;
    double duration = 5.0;
    int track = 0;
    int clip_index = -1;

    if (!app || !app->project || !media) {
        return false;
    }

    if (media->duration > 0.0) {
        duration = media->duration;
    }

    start_time = start_time_override >= 0.0 ? start_time_override : app->ui.timeline.playhead_time;
    if (start_time < 0.0) {
        start_time = 0.0;
    }

    if (track_override >= 0 && track_override < MUZZA_MAX_TRACKS) {
        track = track_override;
    } else {
        track = choose_track_for_insert(app->project, start_time, duration);
    }

    project_clear_selection(app->project);

    if (media->has_video && media->has_audio) {
        // Add video clip
        clip_index = project_add_clip(app->project, media_id, track, MUZZA_CLIP_VIDEO, start_time, duration, 0.0);
        if (clip_index >= 0) {
            app->project->clips[clip_index].selected = true;
        }

        // Add audio clip on the next track if possible
        int audio_track = (track + 1) < MUZZA_MAX_TRACKS ? (track + 1) : track;
        int audio_clip_index = project_add_clip(app->project, media_id, audio_track, MUZZA_CLIP_AUDIO, start_time, duration, 0.0);
        if (audio_clip_index >= 0) {
            app->project->clips[audio_clip_index].selected = true;
            if (clip_index < 0) clip_index = audio_clip_index;
        }
    } else if (media->has_video) {
        clip_index = project_add_clip(app->project, media_id, track, MUZZA_CLIP_VIDEO, start_time, duration, 0.0);
    } else {
        clip_index = project_add_clip(app->project, media_id, track, MUZZA_CLIP_AUDIO, start_time, duration, 0.0);
    }

    if (clip_index < 0) {
        return false;
    }

    SDL_Log(
        "Inserted media: media=%d start=%.3f dur=%.3f track=%d project_dur=%.3f",
        media_id,
        start_time,
        duration,
        track,
        app->project->duration
    );

    if (clip_index >= 0) {
        app->project->clips[clip_index].selected = true;
        app->ui.timeline.selected_clip_index = clip_index;
    }
    app->ui.media_panel.selected_media_index = media_id;
    playback_reset_session(&app->ui);
    return true;
}

static muzza_project* create_project_from_args(fx_context* ctx, int argc, char** argv) {
    muzza_project* proj = NULL;

    if (argc > 1 && muzza_path_has_extension(argv[1], ".muzza")) {
        proj = project_load(argv[1]);
        if (!proj) {
            SDL_Log("Failed to load project: %s", argv[1]);
        }
    }

    if (!proj) {
        proj = project_create();
        if (!proj) {
            return NULL;
        }

        if (argc > 1 && !muzza_path_has_extension(argv[1], ".muzza")) {
            int media_id = project_add_media(proj, argv[1]);

            if (media_id >= 0) {
                muzza_decoder* dec = project_ensure_media_decoder(proj, media_id, ctx);
                if (!dec) {
                    project_remove_media(proj, media_id);
                }
            }
        }
    }

    return proj;
}

static void cleanup_app(muzza_app* app) {
    if (!app) {
        return;
    }

    if (app->project) {
        project_destroy(app->project);
    }
    if (app->surface) {
        fx_surface_destroy(app->surface);
    }
    if (app->ctx) {
        fx_context_destroy(app->ctx);
    }
    if (app->window) {
        SDL_DestroyWindow(app->window);
    }

    SDL_Quit();
}

int muzza_app_main(int argc, char** argv) {
    muzza_app app = {0};
    Uint64 last_ticks = 0;
    bool running = true;

    normalize_process_locale();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    app.window = SDL_CreateWindow(
        "muzza Pro",
        1280,
        720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!app.window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        cleanup_app(&app);
        return 1;
    }

    app.ctx = fx_context_create(&(fx_context_desc) { .app_name = "muzza" });
    if (!app.ctx) {
        SDL_Log("fx_context_create failed");
        cleanup_app(&app);
        return 1;
    }

    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(app.window, fx_context_get_instance(app.ctx), NULL, &vk_surface)) {
        SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        cleanup_app(&app);
        return 1;
    }

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSizeInPixels(app.window, &window_w, &window_h);
    app.surface = fx_surface_create_vulkan(app.ctx, vk_surface, window_w, window_h, FX_CS_SRGB);
    if (!app.surface) {
        SDL_Log("fx_surface_create_vulkan failed");
        cleanup_app(&app);
        return 1;
    }

    app.project = create_project_from_args(app.ctx, argc, argv);
    if (!app.project) {
        SDL_Log("Failed to initialize project");
        cleanup_app(&app);
        return 1;
    }

    ui_state_init(&app.ui, app.project);
    app.ui.ui_scale = SDL_GetWindowDisplayScale(app.window);
    import_browser_refresh(&app.ui.import_browser);
    last_ticks = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        Uint64 now_ticks = SDL_GetTicks();
        double delta_time = (now_ticks - last_ticks) / 1000.0;
        muzza_ui_actions actions = {0};

        last_ticks = now_ticks;
        ui_begin_frame(&app.ui);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                fx_surface_resize(app.surface, event.window.data1, event.window.data2);
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    if (app.ui.import_browser.visible) {
                        import_browser_close(&app.ui.import_browser);
                    } else {
                        running = false;
                    }
                } else if (event.key.key == SDLK_SPACE && !app.ui.import_browser.visible) {
                    app.ui.is_playing = !app.ui.is_playing;
                }
            }

            ui_handle_event(&app.ui, &event, app.window);
        }

        app.ui.ui_scale = SDL_GetWindowDisplayScale(app.window);
        if (app.ui.ui_scale < 1.0f) {
            app.ui.ui_scale = 1.0f;
        }

        playback_sync_preview(app.ctx, app.project, &app.ui, delta_time);

        fx_canvas* canvas = fx_surface_acquire(app.surface);
        if (canvas) {
            SDL_GetWindowSizeInPixels(app.window, &app.ui.window_width, &app.ui.window_height);
            ui_render(canvas, &app.ui, &actions);
            fx_surface_present(app.surface);
        }

        if (actions.toggle_playback) {
            app.ui.is_playing = !app.ui.is_playing;
            playback_reset_session(&app.ui);
        }
        if (actions.open_import_browser) {
            import_browser_open(&app.ui.import_browser);
        }
        if (actions.close_import_browser) {
            import_browser_close(&app.ui.import_browser);
        }
        if (actions.delete_media_id >= 0) {
            project_remove_media(app.project, actions.delete_media_id);
            app.ui.timeline.selected_clip_index = -1;
            app.ui.timeline.active_clip_index = -1;
            app.ui.preview.preview_media_index = -1;
            playback_reset_session(&app.ui);
            sanitize_ui_after_project_change(&app);
        }
        if (actions.insert_media_id >= 0) {
            insert_media_into_timeline(&app, actions.insert_media_id, actions.insert_timeline_time, actions.insert_track_index);
        }
        if (actions.import_path[0] != '\0') {
            if (!import_media_into_project(&app, actions.import_path)) {
                SDL_Log("Failed to import media: %s", actions.import_path);
            }
        }
    }

    cleanup_app(&app);
    return 0;
}
