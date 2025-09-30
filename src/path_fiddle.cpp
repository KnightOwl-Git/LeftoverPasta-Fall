

#include "pasta_context.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/math/simd.hpp"
#include "rive/profiler/profiler_macros.h"
#include "rive/static_scene.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <sstream>
#endif

using namespace rive;

constexpr static char kMoltenVKICD[] =
    "dependencies/MoltenVK/Package/Release/MoltenVK/dynamic/dylib/macOS/"
    "MoltenVK_icd.json";

constexpr static char kSwiftShaderICD[] = "dependencies/SwiftShader/build/"
#ifdef __APPLE__
                                          "Darwin"
#elif defined(_WIN32)
                                          "Windows"
#else
                                          "Linux"
#endif
                                          "/vk_swiftshader_icd.json";
static FiddleContextOptions options;
static GLFWwindow *window = nullptr;
static int msaa = 0;
static bool forceAtomicMode = false;
static bool wireframe = false;
static bool disableFill = false;
static bool disableStroke = false;
static bool clockwiseFill = false;
static bool hotloadShaders = false;

float userUIScale;
float FinalDPIScale;

static std::unique_ptr<FiddleContext> fiddleContext;

static float2 pts[] = {
    {260 + 2 * 100, 60 + 2 * 500},  {260 + 2 * 257, 60 + 2 * 233},
    {260 + 2 * -100, 60 + 2 * 300}, {260 + 2 * 100, 60 + 2 * 200},
    {260 + 2 * 250, 60 + 2 * 0},    {260 + 2 * 400, 60 + 2 * 200},
    {260 + 2 * 213, 60 + 2 * 200},  {260 + 2 * 213, 60 + 2 * 300},
    {260 + 2 * 391, 60 + 2 * 480},  {1400, 1400}}; // Feather control.
constexpr static int NUM_INTERACTIVE_PTS = sizeof(pts) / sizeof(*pts);

static float strokeWidth = 70;

static float2 translate;
static float scale = 1;

static StrokeJoin join = StrokeJoin::round;
static StrokeCap cap = StrokeCap::round;

static bool doClose = false;
static bool paused = false;
static bool unlockedLogic = true;

static int dragIdx = -1;
static float2 dragLastPos;

static int animation = -1;
static int stateMachine = 0;
static int horzRepeat = 0;
static int upRepeat = 0;
static int downRepeat = 0;

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
rcp<File> rivFile;
std::vector<std::unique_ptr<Artboard>> artboards;
std::vector<std::unique_ptr<Scene>> scenes;
std::vector<rive::rcp<rive::ViewModelInstance>> viewModelInstances;
// static std::unique_ptr<rive::StateMachineInstance> stateMachineInstance;

static void clear_scenes() {
  // Unbind all artboards to properly clean up ViewModelInstance references
  
  artboards.clear();
  scenes.clear();
  viewModelInstances.clear();
}

static void make_scenes(size_t count) {
  clear_scenes();
  // stateMachineInstance.reset();

  for (size_t i = 0; i < count; ++i) {
    printf("DEBUG: Creating scene %zu\n", i);

    // auto artboard = rivFile->artboardAt(2);
    auto artboard = rivFile->artboardDefault();

    userUIScale = 1; // default UI scale

    // Set artboard dimensions to match the current window size if provided

    printf("Artboard has %zu state machines\n", artboard->stateMachineCount());
    printf("Artboard has %zu animations\n", artboard->animationCount());
    printf("Current stateMachine index: %d\n", stateMachine);
    printf("Current animation index: %d\n", animation);
    std::unique_ptr<Scene> scene;
    if (stateMachine >= 0) {

      printf("Trying to load state machine at index %d\n", stateMachine);
      scene = artboard->stateMachineAt(stateMachine);
      // if (i == 0) {
      //   stateMachineInstance = artboard->stateMachineAt(stateMachine);
      //   printf("State machine instance created: %s\n",
      //          stateMachineInstance ? stateMachineInstance->name().c_str()
      //                               : "none");
      // }

    } else if (animation >= 0) {
      scene = artboard->animationAt(animation);
    } else {
      scene = artboard->animationAt(0);
    }
    if (scene == nullptr) {
      // This is a riv without any animations or state machines. Just draw
      // the artboard.
      scene = std::make_unique<StaticScene>(artboard.get());
    }

    int viewModelId = artboard.get()->viewModelId();
    viewModelInstances.push_back(
        viewModelId == -1 ? rivFile->createViewModelInstance(artboard.get())
                          : rivFile->createViewModelInstance(viewModelId, 0));
    artboard->bindViewModelInstance(viewModelInstances.back());

    if (viewModelInstances.back() != nullptr) {
      scene->bindViewModelInstance(viewModelInstances.back());
    }

    scene->advanceAndApply(scene->durationSeconds() * i / count);
    artboards.push_back(std::move(artboard));
    scenes.push_back(std::move(scene));
    printf("Finished making scenes");
  }
}

#ifdef __EMSCRIPTEN__
EM_JS(int, window_inner_width, (), { return window["innerWidth"]; });
EM_JS(int, window_inner_height, (), { return window["innerHeight"]; });
EM_JS(char *, get_location_hash_str, (), {
  var jsString = window.location.hash.substring(1);
  var lengthBytes = lengthBytesUTF8(jsString) + 1;
  var stringOnWasmHeap = _malloc(lengthBytes);
  stringToUTF8(jsString, stringOnWasmHeap, lengthBytes);
  return stringOnWasmHeap;
});
#endif
void window_refresh_callback(GLFWwindow *window);

static rive::Vec2D transformMouseToRiveCoords(double x, double y, int width,
                                              int height) {
  // Apply DPI scaling
  // float dpiScale = fiddleContext->dpiScale(window);
  // x *= dpiScale;
  // y *= dpiScale;

  // Create the same transformation matrix used for rendering
  Mat2D forward = computeAlignment(rive::Fit::layout, rive::Alignment::center,
                                   rive::AABB(0, 0, width, height),
                                   artboards.front()->bounds(), FinalDPIScale);
  forward = Mat2D(scale, 0, 0, scale, translate.x, translate.y) * forward;

  // Get the inverse transformation matrix
  Mat2D inverse = forward.invertOrIdentity();

  // Transform the mouse coordinates
  rive::Vec2D mousePos(static_cast<float>(x), static_cast<float>(y));
  return inverse * mousePos;
}

static void mouse_button_callback(GLFWwindow *window, int button, int action,
                                  int mods) {
  double x, y;
  glfwGetCursorPos(window, &x, &y);

  x *= FinalDPIScale;
  y *= FinalDPIScale;

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  // pass mouse coordinates to rive for state machine listener, etc.
  rive::Vec2D position = transformMouseToRiveCoords(x, y, width, height);

  if (scenes[0] && button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      scenes[0]->pointerDown(position);
    } else if (action == GLFW_RELEASE) {
      scenes[0]->pointerUp(position);
    }
  }

  dragLastPos = float2{(float)x, (float)y};
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    dragIdx = -1;
    if (!rivFile) {
      for (int i = 0; i < NUM_INTERACTIVE_PTS; ++i) {
        if (simd::all(simd::abs(dragLastPos - (pts[i] + translate)) < 100)) {
          dragIdx = i;
          break;
        }
      }
    }
  }
}

static void mousemove_callback(GLFWwindow *window, double x, double y) {
  x *= FinalDPIScale;
  y *= FinalDPIScale;
  // if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
  //   float2 pos = float2{(float)x, (float)y};
  //   if (dragIdx >= 0) {
  //     pts[dragIdx] += (pos - dragLastPos);
  //   } else {
  //     translate += (pos - dragLastPos);
  //   }
  //   dragLastPos = pos;
  // }
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  // pass mouse coordinates to rive for state machine listener, etc.
  rive::Vec2D position = transformMouseToRiveCoords(x, y, width, height);

  if (scenes.size() > 0) {
    scenes[0]->pointerMove(position);
  } else {
    printf("no scene");
  }
}

int lastWidth = 0, lastHeight = 0;
double fpsLastTime = 0;
double fpsLastTimeLogic = 0;
int fpsFrames = 0;
static bool needsTitleUpdate = false;

// Throttle DPI changes to prevent memory accumulation
double lastDPIChangeTime = 0;
const double DPI_CHANGE_THROTTLE_INTERVAL = 0.1; // 100ms minimum between DPI changes

enum class API {
  metal,
  vulkan,
};

API api =
#if defined(__APPLE__)
    API::metal
#else
    API::vulkan
#endif
    ;

bool angle = false;
bool skia = false;

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  bool shift = mods & GLFW_MOD_SHIFT;
  if (action == GLFW_PRESS) {
    switch (key) {
    case GLFW_KEY_ESCAPE:
      glfwSetWindowShouldClose(window, 1);
      break;
    case GLFW_KEY_GRAVE_ACCENT: // ` key, backtick
      hotloadShaders = true;
      break;
    case GLFW_KEY_A:
      forceAtomicMode = !forceAtomicMode;
      fpsLastTime = 0;
      fpsFrames = 0;
      needsTitleUpdate = true;
      break;
    case GLFW_KEY_D:
      printf("static float scale = %f;\n", scale);
      printf("static float2 translate = {%f, %f};\n", translate.x, translate.y);
      printf("static float2 pts[] = {");
      for (int i = 0; i < NUM_INTERACTIVE_PTS; i++) {
        printf("{%g, %g}", pts[i].x, pts[i].y);
        if (i < NUM_INTERACTIVE_PTS - 1) {
          printf(", ");
        } else {
          printf("};\n");
        }
      }
      fflush(stdout);
      break;
    case GLFW_KEY_Z:
      fiddleContext->toggleZoomWindow();
      break;
    case GLFW_KEY_MINUS:
      strokeWidth /= 1.5f;
      break;
    case GLFW_KEY_EQUAL:
      strokeWidth *= 1.5f;
      break;
    case GLFW_KEY_W:
      wireframe = !wireframe;
      break;
    case GLFW_KEY_C:
      cap = static_cast<StrokeCap>((static_cast<int>(cap) + 1) % 3);
      break;
    case GLFW_KEY_O:
      doClose = !doClose;
      break;
    case GLFW_KEY_S:
      if (shift) {
        // Toggle Skia.
        clear_scenes();
        rivFile = nullptr;
        lastWidth = 0;
        lastHeight = 0;
        fpsLastTime = 0;
        fpsFrames = 0;
        needsTitleUpdate = true;
      } else {
        disableStroke = !disableStroke;
      }
      break;
    case GLFW_KEY_F:
      disableFill = !disableFill;
      break;
    case GLFW_KEY_X:
      clockwiseFill = !clockwiseFill;
      break;
    case GLFW_KEY_P:
      paused = !paused;
      break;
    case GLFW_KEY_H:
      if (!shift)
        ++horzRepeat;
      else if (horzRepeat > 0)
        --horzRepeat;
      break;
    case GLFW_KEY_K:
      if (!shift)
        ++upRepeat;
      else if (upRepeat > 0)
        --upRepeat;
      break;
    case GLFW_KEY_J:
      if (!rivFile)
        join = static_cast<StrokeJoin>((static_cast<int>(join) + 1) % 3);
      else if (!shift)
        ++downRepeat;
      else if (downRepeat > 0)
        --downRepeat;
      break;
    case GLFW_KEY_UP: {
      float oldScale = scale;
      scale *= 1.25;
      double x = 0, y = 0;
      glfwGetCursorPos(window, &x, &y);
      float2 cursorPos =
          float2{(float)x, (float)y} * fiddleContext->dpiScale(window);
      translate = cursorPos + (translate - cursorPos) * scale / oldScale;
      break;
    }
    case GLFW_KEY_DOWN: {
      float oldScale = scale;
      scale /= 1.25;
      double x = 0, y = 0;
      glfwGetCursorPos(window, &x, &y);
      float2 cursorPos =
          float2{(float)x, (float)y} * fiddleContext->dpiScale(window);
      translate = cursorPos + (translate - cursorPos) * scale / oldScale;
      break;
    }
    case GLFW_KEY_U:
      unlockedLogic = !unlockedLogic;
    }
  }
}

static void glfw_error_callback(int code, const char *message) {
  printf("GLFW error: %i - %s\n", code, message);
}

static void set_environment_variable(const char *name, const char *value) {
  if (const char *existingValue = getenv(name)) {
    printf("warning: %s=%s already set. Overriding with %s=%s\n", name,
           existingValue, name, value);
  }
#ifdef _WIN32
  SetEnvironmentVariableA(name, value);
#else
  setenv(name, value, /*overwrite=*/true);
#endif
}

std::unique_ptr<Renderer> renderer;
std::string rivName;

void riveMainLoop(int width, int height);

int main(int argc, const char **argv) {
  // Cause stdout and stderr to print immediately without buffering.

  rivName =
      "/Users/benknight/Documents/LeftoverPasta_Fall2025/LeftoverPasta-Fall/"
      "src/riv/lp_level_editor.riv"; // load level editor .riv
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

#ifdef DEBUG
  options.enableVulkanValidationLayers = true;
#endif

  printf("set riv name\n");

  glfwSetErrorCallback(glfw_error_callback);

  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize glfw.\n");
    return 1;
  }

  if (msaa > 0) {
    if (msaa > 1) {
      glfwWindowHint(GLFW_SAMPLES, msaa);
    }
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 16);
  }
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  switch (api) {

  case API::metal:
  case API::vulkan:
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    break;
  }
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
  // glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  window = glfwCreateWindow(1600, 1600, "Rive Renderer", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    fprintf(stderr, "Failed to create window.\n");
    return -1;
  }

  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetCursorPosCallback(window, mousemove_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwShowWindow(window);

  switch (api) {
  case API::metal:
    printf("Creating Metal context...\n");
    fiddleContext = FiddleContext::MakeMetalPLS(options);
    printf("Metal context created successfully\n");
    break;
  case API::vulkan:
    fiddleContext = FiddleContext::MakeVulkanPLS(options);
    break;
  }
  if (!fiddleContext) {
    fprintf(stderr, "Failed to create a fiddle context.\n");
    abort();
  }
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  while (!glfwWindowShouldClose(window)) {
    if (!rivName.empty()) {
      glfwPollEvents();
    } else {
      glfwWaitEvents();
    }
    int width, height;

    glfwGetFramebufferSize(window, &width, &height);
    printf("calling rive main loop\n");
    riveMainLoop(width, height);
  }

  // We need to clear the riv scene (which can be holding on to render
  // resources) before releasing the fiddle context
  clear_scenes();
  rivFile = nullptr;

  renderer.reset();

  fiddleContext = nullptr;
  glfwTerminate();

  return 0;
}

static void update_window_title(double fps, int instances, int width,
                                int height) {
  std::ostringstream title;
  if (fps != 0) {
    title << '[' << fps << " FPS]";
  }
  if (instances > 1) {
    title << " (x" << instances << " instances)";
  }
  if (skia) {
    title << " | SKIA Renderer";
  } else {
    title << " | RIVE Renderer";
  }
  if (msaa) {
    title << " (msaa" << msaa << ')';
  } else if (forceAtomicMode) {
    title << " (atomic)";
  }
  title << " | " << width << " x " << height;
  glfwSetWindowTitle(window, title.str().c_str());
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  // riveMainLoop(width, height);
}


void riveMainLoop(int width, int height) {
  fiddleContext->tick();

  // hmm
  RIVE_PROF_FRAME()
  RIVE_PROF_SCOPE()

  if (lastWidth != width || lastHeight != height) {
    printf("size changed to %ix%i\n", width, height);
    lastWidth = width;
    lastHeight = height;
    fiddleContext->onSizeChanged(window, width, height, msaa);
    renderer = fiddleContext->makeRenderer(width, height);
    needsTitleUpdate = true;
  }
  FinalDPIScale = fiddleContext->dpiScale(window) *
                  userUIScale; // multiply DPI scale from monitor by the UI
                               // scale that the user sets :00
  if (needsTitleUpdate) {
    update_window_title(0, 1, width, height);
    needsTitleUpdate = false;
  }

  if (!rivName.empty() && !rivFile) {
    std::ifstream rivStream(rivName, std::ios::binary);
    std::vector<uint8_t> rivBytes(std::istreambuf_iterator<char>(rivStream),
                                  {});
    printf("loading riv file\n");
    rivFile = File::import(rivBytes, fiddleContext->factory());
    if (rivFile) {
      printf("we loaded it\n");
    } else {
      printf("failed to load\n");
    }
  }

  // Call right before begin()
  if (hotloadShaders) {
    hotloadShaders = false;

#ifndef RIVE_BUILD_FOR_IOS
    // We want to build to /tmp/rive (or the correct equivalent)
    std::filesystem::path tempRiveDir =
        std::filesystem::temp_directory_path() / "rive";

    // Get the u8 version of the path (this is especially necessary on
    // windows where the native path character type is wchar_t, then
    // reinterpret_cast the char8_t pointer to char so we can append it to
    // our string.
    // Store the u8string result to extend its lifetime (need to
    // reinterpret_cast through a const char * pointer because u8string
    // returns a std::u8string in C++20 and newer, but we need it as a
    // "char" string)
    std::string tempRiveDirStr =
        reinterpret_cast<const char *>(tempRiveDir.u8string().c_str());

    std::string rebuildCommand = "sh rebuild_shaders.sh " + tempRiveDirStr;
    std::system(rebuildCommand.c_str());
#endif
    fiddleContext->hotloadShaders();
  }
  fiddleContext->begin({
      .renderTargetWidth = static_cast<uint32_t>(width),
      .renderTargetHeight = static_cast<uint32_t>(height),
      .clearColor = 0xff303030,
      .msaaSampleCount = msaa,
      .disableRasterOrdering = forceAtomicMode,
      .wireframe = wireframe,
      .fillsDisabled = disableFill,
      .strokesDisabled = disableStroke,
      .clockwiseFillOverride = clockwiseFill,
  });

  int instances = 1;
  if (rivFile) {
    instances = (1 + horzRepeat * 2) * (1 + upRepeat + downRepeat);
    printf("DEBUG: instances = %d, artboards.size() = %zu, scenes.size() = %zu\n", 
           instances, artboards.size(), scenes.size());
    
    // Always ensure we have the right number of scenes
    if (artboards.size() != instances || scenes.size() != instances) {
      printf("DEBUG: About to call make_scenes(%d)\n", instances);
      make_scenes(instances);
      printf("DEBUG: make_scenes completed\n");
    }
    
    if (!paused && !scenes.empty()) {
      double time = glfwGetTime();
      float dT = static_cast<float>(time - fpsLastTimeLogic);
      fpsLastTimeLogic = time;

      // Clamp delta time to prevent large jumps (e.g., when window regains focus)
      dT = std::min(dT, 1.0f / 30.0f); // Cap at 30 FPS minimum

      for (const auto &scene : scenes) {
        scene->advanceAndApply(dT);
      }
    }
    
    // Add safety checks before accessing vectors
    if (!artboards.empty() && !scenes.empty()) {
      Mat2D m = computeAlignment(rive::Fit::layout, rive::Alignment::center,
                                 rive::AABB(0, 0, width, height),
                                 artboards.front()->bounds(), FinalDPIScale);
      if (width > 0 && height > 0) {
        artboards.front()->width(static_cast<float>(width) / FinalDPIScale);
        artboards.front()->height(static_cast<float>(height) / FinalDPIScale);
      }
      printf("got to line 633\n");

      renderer->save();
      m = Mat2D(scale, 0, 0, scale, translate.x, translate.y) * m;
      renderer->transform(m);
      float spacing = 200 / m.findMaxScale();
      auto scene = scenes.begin();
      for (int j = 0; j < upRepeat + 1 + downRepeat; ++j) {
        renderer->save();
        renderer->transform(Mat2D::fromTranslate(-spacing * horzRepeat,
                                                 (j - upRepeat) * spacing));
        for (int i = 0; i < horzRepeat * 2 + 1; ++i) {
          (*scene++)->draw(renderer.get());
          renderer->transform(Mat2D::fromTranslate(spacing, 0));
        }
        renderer->restore();
      }
      renderer->restore();
    }
  } else {
    float2 p[9];
    for (int i = 0; i < 9; ++i) {
      p[i] = pts[i] + translate;
    }
    RawPath rawPath;
    rawPath.moveTo(p[0].x, p[0].y);
    rawPath.cubicTo(p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
    float2 c0 = simd::mix(p[3], p[4], float2(2 / 3.f));
    float2 c1 = simd::mix(p[5], p[4], float2(2 / 3.f));
    rawPath.cubicTo(c0.x, c0.y, c1.x, c1.y, p[5].x, p[5].y);
    rawPath.cubicTo(p[6].x, p[6].y, p[7].x, p[7].y, p[8].x, p[8].y);
    if (doClose) {
      rawPath.close();
    }

    Factory *factory = fiddleContext->factory();
    auto path = factory->makeRenderPath(rawPath, FillRule::clockwise);

    auto fillPaint = factory->makeRenderPaint();
    float feather = powf(1.5f, (1400 - pts[std::size(pts) - 1].y) / 75);
    if (feather > 1) {
      fillPaint->feather(feather);
    }
    fillPaint->color(0xd0ffffff);

    renderer->drawPath(path.get(), fillPaint.get());

    if (!disableStroke) {
      auto strokePaint = factory->makeRenderPaint();
      strokePaint->style(RenderPaintStyle::stroke);
      strokePaint->color(0x8000ffff);
      strokePaint->thickness(strokeWidth);
      if (feather > 1) {
        strokePaint->feather(feather);
      }

      strokePaint->join(join);
      strokePaint->cap(cap);
      renderer->drawPath(path.get(), strokePaint.get());

      // Draw the interactive points.
      auto pointPaint = factory->makeRenderPaint();
      pointPaint->style(RenderPaintStyle::stroke);
      pointPaint->color(0xff0000ff);
      pointPaint->thickness(14);
      pointPaint->cap(StrokeCap::round);
      pointPaint->join(StrokeJoin::round);

      auto pointPath = factory->makeEmptyRenderPath();
      for (int i : {1, 2, 4, 6, 7}) {
        float2 pt = pts[i] + translate;
        pointPath->moveTo(pt.x, pt.y);
        pointPath->close();
      }
      renderer->drawPath(pointPath.get(), pointPaint.get());

      // Draw the feather control point.
      pointPaint->color(0xffff0000);
      pointPath = factory->makeEmptyRenderPath();
      float2 pt = pts[std::size(pts) - 1] + translate;
      pointPath->moveTo(pt.x, pt.y);
      renderer->drawPath(pointPath.get(), pointPaint.get());
    }
  }

  fiddleContext->end(window);

  if (rivFile) {
    // Count FPS.
    ++fpsFrames;
    double time = glfwGetTime();
    double fpsElapsed = time - fpsLastTime;
    if (fpsElapsed > 1) {
      int instances = (1 + horzRepeat * 2) * (1 + upRepeat + downRepeat);
      double fps = fpsLastTime == 0 ? 0 : fpsFrames / fpsElapsed;
      update_window_title(fps, instances, width, height);
      fpsFrames = 0;
      fpsLastTime = time;
    }
  }
}


