#include "flappy_bird.hpp"

SURGE_MODULE_EXPORT auto on_load(GLFWwindow *) noexcept -> int { return 0; }

SURGE_MODULE_EXPORT auto on_unload(GLFWwindow *) noexcept -> int { return 0; }

SURGE_MODULE_EXPORT auto draw(GLFWwindow *) noexcept -> int { return 0; }

SURGE_MODULE_EXPORT auto update(GLFWwindow *, double) noexcept -> int { return 0; }

SURGE_MODULE_EXPORT void keyboard_event(GLFWwindow *, int, int, int, int) noexcept {}

SURGE_MODULE_EXPORT void mouse_button_event(GLFWwindow *, int, int, int) noexcept {}

SURGE_MODULE_EXPORT void mouse_scroll_event(GLFWwindow *, double, double) noexcept {}