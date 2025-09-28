#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

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

#include "rive/renderer/rive_renderer.hpp"
