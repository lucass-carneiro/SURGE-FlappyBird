#ifndef SURGE_FLAPPY_BIRD_STATE_MACHINE_HPP
#define SURGE_FLAPPY_BIRD_STATE_MACHINE_HPP

#include "player/integer_types.hpp"

namespace fpb {

using state_t = surge::u32;
enum state : surge::u32 { no_state, prepare, play, score, count };

auto state_to_str(state s) noexcept -> const char *;

} // namespace fpb

#endif // SURGE_FLAPPY_BIRD_STATE_MACHINE_HPP