#include "renderer_interface.hpp"
#include "game_core.hpp"

// Include all the heavy dependencies here
#include "pasta_context.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/math/simd.hpp"
#include "rive/profiler/profiler_macros.h"
#include "rive/static_scene.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

using namespace rive;

// Renderer state
static GLFWwindow* g_window = nullptr;
static std::unique_ptr<FiddleContext> fiddleContext;
static std::unique_ptr<Renderer> renderer;
static FiddleContextOptions options;
static std::string rivName;
static GameState* g_gameState = nullptr;

// Rive state
static rcp<File> rivFile;
static std::vector<std::unique_ptr<Artboard>> artboards;
static std::vector<std::unique_ptr<Scene>> scenes;
static std::vector<rive::rcp<rive::ViewModelInstance>> viewModelInstances;

static void clear_scenes() {
    artboards.clear();
    scenes.clear();
    viewModelInstances.clear();
}

static void make_scenes(size_t count) {
    clear_scenes();
    if (!rivFile) return;
    
    artboards.reserve(count);
    scenes.reserve(count);
    viewModelInstances.reserve(count);
    
    for (size_t i = 0; i < count; i++) {
        auto artboard = rivFile->artboardDefault();
        if (!artboard) continue;
        
        auto scene = artboard->defaultScene();
        if (!scene) continue;
        
        artboards.push_back(std::move(artboard));
        scenes.push_back(std::move(scene));
        
        auto viewModelInstance = rivFile->createViewModelInstance(artboards.back().get());
        viewModelInstances.push_back(std::move(viewModelInstance));
    }
}

static void glfw_error_callback(int code, const char *message) {
    printf("GLFW error: %i - %s\n", code, message);
}

extern "C" int renderer_app_run() {
    // Init GLFW and window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize glfw.\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    g_window = glfwCreateWindow(1600, 1600, "Leftover Pasta", nullptr, nullptr);
    if (!g_window) {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }

    // Create context/renderer
    switch (0) { // Default Metal
    case 0:
        fiddleContext = FiddleContext::MakeMetalPLS(options);
        break;
    case 1:
        fiddleContext = FiddleContext::MakeVulkanPLS(options);
        break;
    }
    if (!fiddleContext) {
        fprintf(stderr, "Failed to create a fiddle context.\n");
        return -1;
    }

    int width = 0, height = 0;
    glfwGetFramebufferSize(g_window, &width, &height);
    fiddleContext->onSizeChanged(g_window, width, height, 0);
    renderer = fiddleContext->makeRenderer(width, height);

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(g_window)) {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Update scenes and tick context
        if (!rivName.empty() && !rivFile) {
            std::ifstream rivStream(rivName, std::ios::binary);
            std::vector<uint8_t> rivBytes(std::istreambuf_iterator<char>(rivStream), {});
            rivFile = File::import(rivBytes, fiddleContext->factory());
            make_scenes(1);
        }
        for (auto& scene : scenes) {
            if (scene) scene->advanceAndApply(deltaTime);
        }
        fiddleContext->tick();

        // Render
        if (renderer) {
            for (auto& scene : scenes) {
                if (scene) scene->draw(renderer.get());
            }
        }

        // Resize
        int newWidth, newHeight;
        glfwGetFramebufferSize(g_window, &newWidth, &newHeight);
        if (newWidth != width || newHeight != height) {
            width = newWidth; height = newHeight;
            fiddleContext->onSizeChanged(g_window, width, height, 0);
            renderer = fiddleContext->makeRenderer(width, height);
        }

        glfwPollEvents();
    }
    return 0;
}

// Per-frame APIs not needed in app anymore

extern "C" void renderer_shutdown() {
    clear_scenes();
    rivFile.reset();
    renderer.reset();
    fiddleContext.reset();
}

extern "C" void renderer_set_riv_file(const char* filename) {
    rivName = filename ? filename : "";
    rivFile.reset(); // Force reload
}

extern "C" void renderer_set_game_state(void* gameState) {
    g_gameState = static_cast<GameState*>(gameState);
}
// test renderer change
