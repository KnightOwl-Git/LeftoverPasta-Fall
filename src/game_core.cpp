#include "game_core.hpp"
#include <cstdio>
#include <cstring>

static GameState g_gameState;

extern "C" void game_init() {
    // Initialize default points
    g_gameState.points = {
        {260 + 2 * 100, 60 + 2 * 500},
        {260 + 2 * 257, 60 + 2 * 233},
        {260 + 2 * -100, 60 + 2 * 300},
        {260 + 2 * 100, 60 + 2 * 200},
        {260 + 2 * 250, 60 + 2 * 0},
        {260 + 2 * 400, 60 + 2 * 200},
        {260 + 2 * 213, 60 + 2 * 200},
        {260 + 2 * 213, 60 + 2 * 300},
        {260 + 2 * 391, 60 + 2 * 480},
        {1400, 1400}
    };
}

extern "C" void game_update(double deltaTime) {
    // Game logic updates here
    // This is where you'd put your game state updates
    // that don't depend on rendering
}

extern "C" void game_handle_mouse_button(int button, int action, double x, double y) {
    if (button == 0 && action == 1) { // Left mouse down
        // Find closest point
        float minDist = 1000.0f;
        int closestIdx = -1;
        
        for (size_t i = 0; i < g_gameState.points.size(); i++) {
            float dx = g_gameState.points[i].first - x;
            float dy = g_gameState.points[i].second - y;
            float dist = dx * dx + dy * dy;
            if (dist < minDist) {
                minDist = dist;
                closestIdx = i;
            }
        }
        
        if (closestIdx >= 0 && minDist < 400) { // 20 pixel threshold
            g_gameState.dragIdx = closestIdx;
            g_gameState.dragLastX = x;
            g_gameState.dragLastY = y;
        }
    } else if (button == 0 && action == 0) { // Left mouse up
        g_gameState.dragIdx = -1;
    }
}

extern "C" void game_handle_mouse_move(double x, double y) {
    if (g_gameState.dragIdx >= 0) {
        g_gameState.points[g_gameState.dragIdx].first = x;
        g_gameState.points[g_gameState.dragIdx].second = y;
    }
}

extern "C" void game_handle_key(int key, int scancode, int action) {
    if (action == 1) { // Key press
        switch (key) {
            case 32: // Space
                g_gameState.paused = !g_gameState.paused;
                break;
            case 76: // L
                g_gameState.unlockedLogic = !g_gameState.unlockedLogic;
                break;
            case 87: // W
                g_gameState.wireframe = !g_gameState.wireframe;
                break;
            case 70: // F
                g_gameState.disableFill = !g_gameState.disableFill;
                break;
            case 83: // S
                g_gameState.disableStroke = !g_gameState.disableStroke;
                break;
            case 67: // C
                g_gameState.doClose = !g_gameState.doClose;
                break;
        }
    }
}

extern "C" void game_set_riv_file(const char* filename) {
    // Store filename for renderer to load
    // This is just a placeholder - the actual loading happens in the renderer
}

extern "C" GameState* game_get_state() {
    return &g_gameState;
}

extern "C" void game_shutdown() {
    // Cleanup game state
}
// test game core change
