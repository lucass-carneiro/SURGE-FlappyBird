#ifndef SURGE_MODULE_FLAPPY_BIRD_HPP
#define SURGE_MODULE_FLAPPY_BIRD_HPP

#include "sc_opengl/atoms/pv_ubo.hpp"
#include "sc_opengl/atoms/sprite_database.hpp"
#include "sc_opengl/atoms/texture.hpp"
#include "sc_window.hpp"

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

using namespace surge::window;

namespace fpb {

using pvubo_t = surge::gl_atom::pv_ubo::buffer;

using tdb_t = surge::gl_atom::texture::database;
using sdb_t = surge::gl_atom::sprite_database::database;

namespace state_machine {

using state_t = surge::u32;
enum state : surge::u32 { no_state, prepare, play, score, count };

void state_transition(state &state_a, state &state_b) noexcept;
void state_update(window_t w, const fpb::tdb_t &tdb, fpb::sdb_t &sdb, const state &state_a,
                  state &state_b, double dt) noexcept;

auto state_to_str(const state &s) noexcept -> const char *;

} // namespace state_machine
} // namespace fpb

extern "C" {
SURGE_MODULE_EXPORT auto gl_on_load(window_t w) noexcept -> int;

SURGE_MODULE_EXPORT auto gl_on_unload(window_t w) noexcept -> int;

SURGE_MODULE_EXPORT auto gl_draw(window_t w) noexcept -> int;

SURGE_MODULE_EXPORT auto gl_update(window_t w, double dt) noexcept -> int;

SURGE_MODULE_EXPORT void gl_keyboard_event(window_t w, int key, int scancode, int action,
                                           int mods) noexcept;

SURGE_MODULE_EXPORT void gl_mouse_button_event(window_t w, int button, int action,
                                               int mods) noexcept;

SURGE_MODULE_EXPORT void gl_mouse_scroll_event(window_t w, double xoffset, double yoffset) noexcept;
}

#endif // SURGE_MODULE_FLAPPY_BIRD_HPP