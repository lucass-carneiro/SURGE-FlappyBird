#include "flappy_bird.hpp"

void fpb::state_machine::state_transition(state &state_a, state &state_b) noexcept {
  const auto a_empty_b_empty{state_a == state::no_state && state_b == state::no_state};
  const auto a_full_b_empty{state_a != state::no_state && state_b == state::no_state};

  if (!(a_empty_b_empty || a_full_b_empty)) {
    surge::renderer::gl::wait_idle();
    state_a = state_b;
    state_b = state::no_state;
  }
}

auto fpb::state_machine::state_to_str(const state &s) noexcept -> const char * {
  using namespace fpb::state_machine;

  switch (s) {
  case state::no_state:
    return "no state";

  case state::prepare:
    return "prepare";

  case state::play:
    return "play";

  case state::score:
    return "score";

  case state::count:
    return "count";

  default:
    return "unknown state";
  }
}