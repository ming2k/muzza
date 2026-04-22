#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "flux/flux.h"
#include "decoder.h"
#include "ui.h"

#include <stdio.h>
#include <stdbool.h>

static bool g_stop = false;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video_file>\n", argv[0]);
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return 1;

    // High DPI is critical for professional software
    SDL_Window *window = SDL_CreateWindow("muzza Pro", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) return 1;

    fx_context *ctx = fx_context_create(&(fx_context_desc){ .app_name = "muzza", .enable_validation = false });
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    SDL_Vulkan_CreateSurface(window, fx_context_get_instance(ctx), NULL, &vk_surface);

    int w_px, h_px;
    SDL_GetWindowSizeInPixels(window, &w_px, &h_px);
    fx_surface *surface = fx_surface_create_vulkan(ctx, vk_surface, w_px, h_px, FX_CS_SRGB);
    muzza_decoder* dec = decoder_create(ctx, argv[1]);

    muzza_ui_state ui_state = {0};
    SDL_Event event;

    while (!g_stop) {
        // High DPI Handling: Get current display scale
        float dpi_scale = SDL_GetWindowDisplayScale(window);
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) g_stop = true;
            
            // Handle DPI-aware resizing
            if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                fx_surface_resize(surface, event.window.data1, event.window.data2);
            }
            
            // Map logical mouse coordinates to physical pixels
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                ui_state.mouse_x = event.motion.x * dpi_scale;
                ui_state.mouse_y = event.motion.y * dpi_scale;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    ui_state.mouse_down = true;
                    ui_state.mouse_x = event.button.x * dpi_scale;
                    ui_state.mouse_y = event.button.y * dpi_scale;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (event.button.button == SDL_BUTTON_LEFT) ui_state.mouse_down = false;
            }
        }

        decoder_read_frame(dec);
        fx_canvas *c = fx_surface_acquire(surface);
        if (c) {
            // ALWAYS use pixel size for rendering logic
            SDL_GetWindowSizeInPixels(window, &ui_state.window_width, &ui_state.window_height);
            
            float old_pos = ui_state.playhead_pos;
            if (!ui_state.mouse_down) ui_state.playhead_pos = decoder_get_progress(dec);
            
            ui_render(c, &ui_state);
            
            if (ui_state.playhead_pos != old_pos && ui_state.mouse_down) {
                decoder_seek(dec, ui_state.playhead_pos);
            }

            int vw, vh;
            ui_state.current_frame = decoder_get_image(dec, &vw, &vh);
            ui_state.frame_width = vw;
            ui_state.frame_height = vh;

            fx_surface_present(surface);
        }
    }

    decoder_destroy(dec);
    fx_surface_destroy(surface);
    fx_context_destroy(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
