#include "state_machine.hpp"

auto fpb::state_to_str(fpb::state s) noexcept -> const char * {
  using fpb::state;

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