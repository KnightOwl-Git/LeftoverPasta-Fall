#include "game_core.hpp"
#include "renderer_interface.hpp"

int main(int argc, const char **argv) {
    game_init();
    renderer_set_game_state(game_get_state());
    renderer_set_riv_file("riv/lp_level_editor.riv");
    const int rc = renderer_app_run();
    renderer_shutdown();
    game_shutdown();
    return rc;
}
// test app main change
