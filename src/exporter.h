#ifndef MUZZA_EXPORTER_H
#define MUZZA_EXPORTER_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "project.h"

typedef struct muzza_exporter muzza_exporter;

typedef enum {
    MUZZA_EXPORT_IDLE = 0,
    MUZZA_EXPORT_RUNNING,
    MUZZA_EXPORT_DONE,
    MUZZA_EXPORT_CANCELLED,
    MUZZA_EXPORT_ERROR
} muzza_export_status;

muzza_exporter* exporter_create(const muzza_project* project, const char* output_path);
void exporter_destroy(muzza_exporter* exporter);

SDL_Thread* exporter_start_thread(muzza_exporter* exporter);

muzza_export_status exporter_get_status(muzza_exporter* exporter);
double exporter_get_progress(muzza_exporter* exporter);
const char* exporter_get_error(muzza_exporter* exporter);
void exporter_request_cancel(muzza_exporter* exporter);

#endif
