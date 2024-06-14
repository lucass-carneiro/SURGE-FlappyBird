#ifndef SURGE_MODULE_FLAPPY_BIRD_HPP
#define SURGE_MODULE_FLAPPY_BIRD_HPP

// clang-format off
#include "player/options.hpp"
#include "player/window.hpp"
// clang-format on

#if defined(SURGE_COMPILER_Clang)                                                                  \
    || defined(SURGE_COMPILER_GCC) && COMPILING_SURGE_MODULE_FLAPPY_BIRD
#  define SURGE_MODULE_EXPORT __attribute__((__visibility__("default")))
#elif defined(SURGE_COMPILER_MSVC) && COMPILING_SURGE_MODULE_FLAPPY_BIRD
#  define SURGE_MODULE_EXPORT __declspec(dllexport)
#elif defined(SURGE_COMPILER_MSVC)
#  define SURGE_MODULE_EXPORT __declspec(dllimport)
#else
#  define SURGE_MODULE_EXPORT
#endif

namespace fpb {

// Callbacks
auto bind_callbacks(GLFWwindow *window) noexcept -> int;
auto unbind_callbacks(GLFWwindow *window) noexcept -> int;

auto update_bird_animation(double dt) noexcept -> glm::vec4;
auto update_bird_model(GLFWwindow *window, double dt) -> glm::mat4;

} // namespace fpb

extern "C" {
SURGE_MODULE_EXPORT auto on_load(GLFWwindow *window) noexcept -> int;

SURGE_MODULE_EXPORT auto on_unload(GLFWwindow *window) noexcept -> int;

SURGE_MODULE_EXPORT auto draw(GLFWwindow *window) noexcept -> int;

SURGE_MODULE_EXPORT auto update(GLFWwindow *window, double dt) noexcept -> int;

SURGE_MODULE_EXPORT void keyboard_event(GLFWwindow *window, int key, int scancode, int action,
                                        int mods) noexcept;

SURGE_MODULE_EXPORT void mouse_button_event(GLFWwindow *window, int button, int action,
                                            int mods) noexcept;

SURGE_MODULE_EXPORT void mouse_scroll_event(GLFWwindow *window, double xoffset,
                                            double yoffset) noexcept;
}

#endif // SURGE_MODULE_FLAPPY_BIRD_HPP