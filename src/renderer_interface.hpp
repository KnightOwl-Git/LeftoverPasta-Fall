#pragma once

// C interface for rendering owned entirely by the renderer dylib
extern "C" {
    // Provide game state pointer before running
    void renderer_set_game_state(void* gameState);
    void renderer_set_riv_file(const char* filename);

    // Runs the full app loop (creates window, sets callbacks, renders)
    int renderer_app_run();

    // Clean shutdown
    void renderer_shutdown();
}
