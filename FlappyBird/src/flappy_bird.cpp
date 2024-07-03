#include "flappy_bird.hpp"

namespace globals {

static fpb::tdb_t tdb{};      // NOLINT
static fpb::pvubo_t pv_ubo{}; // NOLINT
static fpb::sdb_t sdb{};      // NOLINT

static fpb::state_machine::state state_a{}; // NOLINT
static fpb::state_machine::state state_b{}; // NOLINT

static const glm::vec2 bird_bbox{34.0f, 24.0f};
static const glm::vec2 base_pos{0.0, 400.0f};
static const glm::vec2 base_bbox{288.0f, 112.0f};

} // namespace globals

static inline void update_background(const glm::vec2 &dims) {
  using namespace surge::gl_atom;

  static const auto bckg_texture{
      globals::tdb.find("resources/static/background-day.png").value_or(0)};
  const auto bckg_model{sprite::place(glm::vec2{0.0f}, dims, 0.1f)};

  globals::sdb.add(bckg_texture, bckg_model, 1.0);
}

static inline void update_rolling_base(float dt, float ww) {
  using namespace surge::gl_atom;

  static const auto base_texture{globals::tdb.find("resources/static/base.png").value_or(0)};

  const float base_drift_speed{0.1f};
  static float base_drift{0.0f};

  static auto base_model{
      sprite::place(globals::base_pos, glm::vec2{ww * 3.0f, globals::base_bbox[1]}, 0.2f)};

  if (base_drift < 0.5f) {
    base_model = glm::translate(base_model, glm::vec3{-base_drift_speed * dt, 0.0f, 0.0f});
    base_drift += base_drift_speed * dt;
  } else {
    base_model = glm::translate(base_model, glm::vec3{0.5f, 0.0f, 0.0f});
    base_drift = 0.0f;
  }

  globals::sdb.add(base_texture, base_model, 1.0);
}

using acceleration_function = float (*)(float y, float y0);

static inline auto harmonic_oscillator(float y, float y0) -> float { return -50.0f * (y - y0); }
static inline auto gravity(float, float) -> float { return 1000.0f; }

static inline auto update_bird_physics(const glm::vec2 &dims, float dt, float dt2, bool up_kick,
                                       acceleration_function a) {
  using namespace surge::gl_atom;

  // Bird position (Velocity Verlet update)
  const auto x0{dims[0] / 2.0f - globals::bird_bbox[0] / 2.0f - 10.0f};
  const auto y0{dims[1] / 2.0f - globals::bird_bbox[1] / 2.0f};

  static float y_n{y0 - 5.0f};
  static float vy_n{0.0f};

  if (up_kick) {
    vy_n = -250.0f;
  }

  const auto a_n{a(y_n, y0)};

  y_n = y_n + vy_n * dt + 0.5f * a_n * dt2;
  const auto a_np1{a(y_n, y0)};
  vy_n = vy_n + 0.5f * (a_n + a_np1) * dt;

  return sprite::place(glm::vec2{x0, y_n}, globals::bird_bbox, 0.3f);
}

static inline auto update_bird_collision(const glm::mat4 &model) noexcept -> bool {
  const auto bird_x{(model)[3][0]};
  const auto bird_y{(model)[3][1]};
  const auto bird_w{globals::bird_bbox[0]};
  const auto bird_h{globals::bird_bbox[1]};

  // Base colision
  const auto base_x{globals::base_pos[0]};
  const auto base_y{globals::base_pos[1]};
  const auto base_w{globals::base_bbox[0]};
  const auto base_h{globals::base_bbox[1]};

  const auto base_collision{bird_x < base_x + base_w && bird_x + bird_w > base_x
                            && bird_y < base_y + base_h && bird_y + bird_h > base_y};

  return base_collision;
}

static inline void update_bird_flap_animation(float dt, const glm::mat4 &bird_model) {
  static const auto bird_sheet{globals::tdb.find("resources/sheets/bird_red.png").value_or(0)};

  // Bird flap
  const std::array<glm::vec4, 4> frame_views{
      glm::vec4{1.0f, 1.0f, 34.0f, 24.0f}, glm::vec4{36.0f, 1.0f, 34.0f, 24.0f},
      glm::vec4{71.0f, 1.0f, 34.0f, 24.0f}, glm::vec4{106.0f, 1.0f, 34.0f, 24.0f}};

  const double frame_rate{10.0};
  const double wait_time{1.0 / frame_rate};

  static surge::u8 frame_idx{0};
  static double elapsed{0};

  if (elapsed > wait_time) {
    frame_idx++;
    frame_idx %= 4;
    elapsed = 0;
  } else {
    elapsed += dt;
  }

  const auto bird_frame_view{frame_views[frame_idx]}; // NOLINT

  globals::sdb.add_view(bird_sheet, bird_model, bird_frame_view, glm::vec2{141.0f, 26.0f}, 1.0f);
}

void fpb::update_state_prepare(double dt) noexcept {
  using namespace surge;
  using namespace fpb::state_machine;

  // Window dims and ticks
  const auto dims{window::get_dims()};
  const auto fdt{static_cast<float>(dt)};
  const auto fdt2{static_cast<float>(dt * dt)};

  // Database reset
  globals::sdb.reset();

  // Background
  update_background(dims);

  // Rolling base
  update_rolling_base(fdt, dims[0]);

  // Bird physics
  const auto bird_model{update_bird_physics(dims, fdt, fdt2, false, harmonic_oscillator)};

  // Bird flap animation
  update_bird_flap_animation(fdt, bird_model);

  // State transition
  if (window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
    globals::state_b = state::play;
  }
}

void fpb::update_state_play(double dt) noexcept {
  using namespace surge;
  using namespace fpb::state_machine;

  // Window dims and ticks
  const auto dims{window::get_dims()};
  const auto fdt{static_cast<float>(dt)};
  const auto fdt2{static_cast<float>(dt * dt)};

  // Database reset
  globals::sdb.reset();

  // Background
  update_background(dims);

  // Rolling base
  update_rolling_base(fdt, dims[0]);

  // Bird physics
  static auto old_click_state{GLFW_RELEASE}; // We enter this state on a mouse press
  const auto current_click_state{window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT)};
  const auto up_kick{current_click_state == GLFW_PRESS && old_click_state == GLFW_RELEASE};

  const auto bird_model{update_bird_physics(dims, fdt, fdt2, up_kick, gravity)};
  const auto collision{update_bird_collision(bird_model)};

  // Bird flap animation
  update_bird_flap_animation(fdt, bird_model);

  // Refresh click cache
  old_click_state = window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT);

  // State transition
  if (collision) {
    globals::state_b = state::score;
  }
}

void fpb::update_state_score(double) noexcept {}

void fpb::state_transition() noexcept {
  using namespace globals;
  using namespace fpb::state_machine;

  const auto a_empty_b_empty{state_a == state::no_state && state_b == state::no_state};
  const auto a_full_b_empty{state_a != state::no_state && state_b == state::no_state};

  if (!(a_empty_b_empty || a_full_b_empty)) {
    state_a = state_b;
    state_b = state::no_state;
  }
}

void fpb::state_update(double dt) noexcept {
  using namespace fpb::state_machine;

  switch (globals::state_a) {

  case state::prepare:
    update_state_prepare(dt);
    break;

  case state::play:
    update_state_play(dt);
    break;

  case state::score:
    update_state_score(dt);
    break;

  default:
    break;
  }
}

auto fpb::state_machine::state_to_str(state s) noexcept -> const char * {
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
  // All textures
  texture::create_info ci{};
  ci.filtering = texture::texture_filtering::nearest;
  globals::tdb.add(ci, "resources/static/base.png", "resources/static/background-day.png",
                   "resources/sheets/bird_red.png");

  // First state
  globals::state_b = state::prepare;
  state_transition();

  return 0;
}

extern "C" SURGE_MODULE_EXPORT auto on_unload() noexcept -> int {
  using namespace fpb;

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
  using namespace fpb;

  // Update states
  state_transition();
  state_update(dt);

  return 0;
}

extern "C" SURGE_MODULE_EXPORT void keyboard_event(int, int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_button_event(int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_scroll_event(double, double) noexcept {}