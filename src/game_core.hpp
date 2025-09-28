#pragma once

#include <vector>
#include <string>

// Lightweight game logic interface - no heavy dependencies
struct GameState {
    float strokeWidth = 70.0f;
    float scale = 1.0f;
    bool paused = false;
    bool unlockedLogic = true;
    bool doClose = false;
    bool wireframe = false;
    bool disableFill = false;
    bool disableStroke = false;
    bool clockwiseFill = false;
    
    // Interactive points for path editing
    std::vector<std::pair<float, float>> points;
    int dragIdx = -1;
    float dragLastX = 0.0f;
    float dragLastY = 0.0f;
    
    // Animation state
    int animation = -1;
    int stateMachine = -1;
    int horzRepeat = 0;
    int upRepeat = 0;
    int downRepeat = 0;
};

// C interface for game logic
extern "C" {
    void game_init();
    void game_update(double deltaTime);
    void game_handle_mouse_button(int button, int action, double x, double y);
    void game_handle_mouse_move(double x, double y);
    void game_handle_key(int key, int scancode, int action);
    void game_set_riv_file(const char* filename);
    GameState* game_get_state();
    void game_shutdown();
}
