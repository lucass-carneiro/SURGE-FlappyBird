#include "flappy_bird.hpp"

namespace globals {

static fpb::tdb_t tdb{};      // NOLINT
static fpb::pvubo_t pv_ubo{}; // NOLINT
static fpb::sdb_t sdb{};      // NOLINT

static fpb::state_machine::state state_a{}; // NOLINT
static fpb::state_machine::state state_b{}; // NOLINT

} // namespace globals

extern "C" SURGE_MODULE_EXPORT auto on_load() noexcept -> int {
  using namespace surge;
  using namespace surge::gl_atom;
  using namespace fpb;
  using namespace fpb::state_machine;

  // Texture database
  globals::tdb = texture::database::create(128);

  // Sprite database
  auto sdb{sprite::database::create(128)};
  if (!sdb) {
    log_error("Unable to create sprite database");
    return static_cast<int>(sdb.error());
  }
  globals::sdb = *sdb;

  // Initialize global 2D projection matrix and view matrix
  const auto dims{window::get_dims()};
  const auto projection{glm::ortho(0.0f, dims[0], dims[1], 0.0f, 0.0f, 1.0f)};
  const auto view{glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                              glm::vec3(0.0f, 1.0f, 0.0f))};

  // PV UBO
  globals::pv_ubo = pv_ubo::buffer::create();
  globals::pv_ubo.update_all(&projection, &view);

  // Load game resources
  texture::create_info ci{};
  ci.filtering = texture::texture_filtering::nearest;

  // clang-format off
  globals::tdb.add(
    ci,
    "resources/static/base.png",
    "resources/static/background-day.png",
    "resources/sheets/bird_red.png",
    "resources/static/pipe-green.png",
    "resources/text/instructions_1.png",
    "resources/text/instructions_2.png",
    "resources/text/gameover.png",
    "resources/numbers/0.png",
    "resources/numbers/1.png",
    "resources/numbers/2.png",
    "resources/numbers/3.png",
    "resources/numbers/4.png",
    "resources/numbers/5.png",
    "resources/numbers/6.png",
    "resources/numbers/7.png",
    "resources/numbers/8.png",
    "resources/numbers/9.png"
  );
  // clang-format on

  // First state
  globals::state_b = state::prepare;
  state_transition(globals::state_a, globals::state_b);

  return 0;
}

extern "C" SURGE_MODULE_EXPORT auto on_unload() noexcept -> int {
  surge::renderer::gl::wait_idle();
  globals::pv_ubo.destroy();
  globals::sdb.destroy();
  globals::tdb.destroy();
  return 0;
}

extern "C" SURGE_MODULE_EXPORT auto draw() noexcept -> int {
  globals::pv_ubo.bind_to_location(2);
  globals::sdb.draw();
  return 0;
}

extern "C" SURGE_MODULE_EXPORT auto update(double dt) noexcept -> int {
  using namespace fpb::state_machine;
  state_transition(globals::state_a, globals::state_b);
  state_update(globals::tdb, globals::sdb, globals::state_a, globals::state_b, dt);
  return 0;
}

extern "C" SURGE_MODULE_EXPORT void keyboard_event(int, int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_button_event(int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_scroll_event(double, double) noexcept {}