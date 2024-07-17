#include "flappy_bird.hpp"

#include <random>

namespace globals {

static fpb::tdb_t tdb{};      // NOLINT
static fpb::pvubo_t pv_ubo{}; // NOLINT
static fpb::sdb_t sdb{};      // NOLINT

static fpb::state_machine::state state_a{}; // NOLINT
static fpb::state_machine::state state_b{}; // NOLINT

} // namespace globals

using pipe_queue_storage_t = foonathan::memory::static_allocator_storage<1024>;
using pipe_queue_alloc_t = foonathan::memory::static_allocator;
using pipe_queue_t = foonathan::memory::deque<glm::vec2, pipe_queue_alloc_t>;

static inline void update_background(const glm::vec2 &window_dims) {
  using namespace surge::gl_atom;

  static const auto bckg_texture{
      globals::tdb.find("resources/static/background-day.png").value_or(0)};
  const auto bckg_model{sprite::place(glm::vec2{0.0f}, window_dims, 0.1f)};

  globals::sdb.add(bckg_texture, bckg_model, 1.0);
}

static inline void update_rolling_base(float ground_drift, const glm::vec2 &window_dims,
                                       glm::vec2 &base_pos, const glm::vec2 &base_bbox) {
  using namespace surge::gl_atom;

  static const auto base_texture{globals::tdb.find("resources/static/base.png").value_or(0)};
  static auto base_shift{0.0f};

  const auto base_model{
      sprite::place(base_pos, glm::vec2{window_dims[0] * 3.0f, base_bbox[1]}, 0.2f)};

  if (base_shift < base_bbox[0]) {
    base_pos[0] -= ground_drift;
    base_shift += ground_drift;
  } else {
    base_pos[0] = 0.0f;
    base_shift = 0.0;
  }

  globals::sdb.add(base_texture, base_model, 1.0);
}

using acceleration_function = float (*)(float y, float y0);

static inline auto harmonic_oscillator(float y, float y0) -> float { return -50.0f * (y - y0); }
static inline auto gravity(float, float) -> float { return 1000.0f; }

static inline auto update_bird_physics(const glm::vec2 &window_dims, float dt, float dt2,
                                       bool up_kick, const glm::vec2 &bird_bbox,
                                       acceleration_function a) {
  using namespace surge::gl_atom;

  // Bird position (Velocity Verlet update)
  const auto x0{window_dims[0] / 2.0f - bird_bbox[0] / 2.0f - 50.0f};
  const auto y0{window_dims[1] / 2.0f - bird_bbox[1] / 2.0f};

  static float y_n{y0 - 5.0f};
  static float vy_n{0.0f};

  if (up_kick) {
    vy_n = -300.0f;
  }

  const auto a_n{a(y_n, y0)};

  y_n = y_n + vy_n * dt + 0.5f * a_n * dt2;
  const auto a_np1{a(y_n, y0)};
  vy_n = vy_n + 0.5f * (a_n + a_np1) * dt;

  return sprite::place(glm::vec2{x0, y_n}, bird_bbox, 0.3f);
}

static inline auto update_bird_collision(const glm::mat4 &model, const glm::vec2 &base_pos,
                                         const glm::vec2 &base_bbox,
                                         const glm::vec2 &bird_bbox) noexcept -> bool {
  const auto bird_x{(model)[3][0]};
  const auto bird_y{(model)[3][1]};
  const auto bird_w{bird_bbox[0]};
  const auto bird_h{bird_bbox[1]};

  // Base colision
  const auto base_x{base_pos[0]};
  const auto base_y{base_pos[1]};
  const auto base_w{base_bbox[0]};
  const auto base_h{base_bbox[1]};

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

  const auto &bird_frame_view{frame_views[frame_idx]}; // NOLINT

  globals::sdb.add_view(bird_sheet, bird_model, bird_frame_view, glm::vec2{141.0f, 26.0f}, 1.0f);
}

static inline void update_pipes(float ground_drift, const glm::vec2 &pipe_gaps,
                                const glm::vec2 &pipe_bbox, pipe_queue_t &pipe_queue) noexcept {
  using namespace surge::gl_atom;

  static const auto pipe_handle{globals::tdb.find("resources/static/pipe-green.png").value_or(0)};

  for (auto &pipe_down_pos : pipe_queue) {
    // Place pipe sprites
    const glm::vec2 pipe_up_pos{pipe_down_pos[0], pipe_down_pos[1] - pipe_gaps[1]};

    const auto pipe_down{sprite::place(pipe_down_pos, pipe_bbox, 0.15f)};
    const auto pipe_up{
        glm::translate(glm::rotate(sprite::place(pipe_up_pos, pipe_bbox, 0.15f),
                                   glm::radians(180.0f), glm::vec3{0.0f, 0.0f, 1.0f}),
                       glm::vec3{-1.0f, 0.0f, 0.0f})};

    globals::sdb.add(pipe_handle, pipe_down, 1.0f);
    globals::sdb.add(pipe_handle, pipe_up, 1.0f);

    // Update pipe position
    pipe_down_pos[0] -= ground_drift;
  }

  // Check if leftmost pipe left the screen
  if ((pipe_queue.front()[0] + pipe_bbox[0]) < 0) {
    pipe_queue.pop_front();
  }
}

static inline void update_state_prepare(float dt, float dt2, float ground_drift,
                                        const glm::vec2 &window_dims, glm::vec2 &base_pos,
                                        const glm::vec2 &base_bbox, const glm::vec2 &bird_bbox,
                                        const glm::vec2 &pipe_gaps, const glm::vec2 &pipe_bbox,
                                        pipe_queue_t &pipe_queue) noexcept {
  using namespace surge;
  using namespace fpb::state_machine;

  // Database reset
  globals::sdb.reset();

  // Background
  update_background(window_dims);

  // Rolling base
  update_rolling_base(ground_drift, window_dims, base_pos, base_bbox);

  // Bird physics
  const auto bird_model{
      update_bird_physics(window_dims, dt, dt2, false, bird_bbox, harmonic_oscillator)};

  // Bird flap animation
  update_bird_flap_animation(dt, bird_model);

  // TODO: Temporary, pipes
  update_pipes(ground_drift, pipe_gaps, pipe_bbox, pipe_queue);

  // State transition
  if (window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
    globals::state_b = state::play;
  }
}

static inline void update_state_play(float dt, float dt2, float ground_drift,
                                     const glm::vec2 &window_dims, glm::vec2 &base_pos,
                                     const glm::vec2 &base_bbox, const glm::vec2 &bird_bbox,
                                     const glm::vec2 &pipe_gaps, const glm::vec2 &pipe_bbox,
                                     pipe_queue_t &pipe_queue) noexcept {
  using namespace surge;
  using namespace fpb::state_machine;

  // Database reset
  globals::sdb.reset();

  // Background
  update_background(window_dims);

  // Rolling base
  update_rolling_base(ground_drift, window_dims, base_pos, base_bbox);

  // Bird physics
  static auto old_click_state{GLFW_RELEASE}; // We enter this state on a mouse press
  const auto current_click_state{window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT)};
  const auto up_kick{current_click_state == GLFW_PRESS && old_click_state == GLFW_RELEASE};

  const auto bird_model{update_bird_physics(window_dims, dt, dt2, up_kick, bird_bbox, gravity)};
  const auto collision{update_bird_collision(bird_model, base_pos, base_bbox, bird_bbox)};

  // Bird flap animation
  update_bird_flap_animation(dt, bird_model);

  // Pipes
  update_pipes(ground_drift, pipe_gaps, pipe_bbox, pipe_queue);

  // Refresh click cache
  old_click_state = window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT);

  // State transition
  if (collision) {
    globals::state_b = state::score;
  }
}

static inline void update_state_score(double) noexcept {}

void fpb::state_machine::state_transition() noexcept {
  using namespace globals;
  using namespace fpb::state_machine;

  const auto a_empty_b_empty{state_a == state::no_state && state_b == state::no_state};
  const auto a_full_b_empty{state_a != state::no_state && state_b == state::no_state};

  if (!(a_empty_b_empty || a_full_b_empty)) {
    state_a = state_b;
    state_b = state::no_state;
  }
}

void fpb::state_machine::state_update(double dt) noexcept {
  using namespace surge;
  using namespace fpb::state_machine;

  // Float conversions
  const auto fdt{static_cast<float>(dt)};
  const auto fdt2{static_cast<float>(dt * dt)};

  // Original sizes
  const glm::vec2 original_window_size{288.0f, 512.0f};
  const glm::vec2 original_bird_bbox{34.0f, 24.0f};
  const glm::vec2 original_base_bbox{288.0f, 112.0f};
  const glm::vec2 original_pipe_bbox{52.0f, 32.0f};

  const auto window_dims{window::get_dims()};
  const auto scale_factor{window_dims / original_window_size};

  // Base sizes
  const auto base_bbox{original_base_bbox * scale_factor};
  static glm::vec2 base_pos{0.0f, window_dims[1] - base_bbox[1]};
  const auto ground_drift{100.0f * fdt};

  // Bird sizes
  const auto bird_bbox{original_bird_bbox * scale_factor};

  // Pipe sizes
  const glm::vec2 pipe_gaps{window_dims[0] / 2.0f, 150.0f};
  const glm::vec2 pipe_bbox{original_pipe_bbox[0] * scale_factor[0], window_dims[1]};

  // Allowed pipe y range
  const float allowed_pipe_area_fraction{(window_dims[1] - base_bbox[1]) / 4.0f};
  std::uniform_real_distribution<float> pipe_y_range{
      allowed_pipe_area_fraction, window_dims[1] - base_bbox[1] - allowed_pipe_area_fraction};

  // Initialize pipe position queue
  static std::minstd_rand engine{std::random_device{}()};

  static pipe_queue_storage_t storage{};
  static pipe_queue_alloc_t sa{storage};
  static pipe_queue_t pipe_queue{
      {glm::vec2{window_dims[0], pipe_y_range(engine)},
       glm::vec2{window_dims[0] + window_dims[0] / 2.0f, pipe_y_range(engine)},
       glm::vec2{window_dims[0] + 2.0f * window_dims[0] / 2.0f, pipe_y_range(engine)},
       glm::vec2{window_dims[0] + 3.0f * window_dims[0] / 2.0f, pipe_y_range(engine)}},
      sa};

  // Keep pipe number constant
  if (pipe_queue.size() != 4) {
    const auto &last_pipe{pipe_queue.back()};
    pipe_queue.push_back(glm::vec2{last_pipe[0] + window_dims[0] / 2.0f, pipe_y_range(engine)});
  }

  // State switch
  switch (globals::state_a) {

  case state::prepare:
    update_state_prepare(fdt, fdt2, ground_drift, window_dims, base_pos, base_bbox, bird_bbox,
                         pipe_gaps, pipe_bbox, pipe_queue);
    break;

  case state::play:
    update_state_play(fdt, fdt2, ground_drift, window_dims, base_pos, base_bbox, bird_bbox,
                      pipe_bbox, pipe_gaps, pipe_queue);
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
                   "resources/sheets/bird_red.png", "resources/static/pipe-green.png");

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
  using namespace fpb::state_machine;

  // Update states
  state_transition();
  state_update(dt);

  return 0;
}

extern "C" SURGE_MODULE_EXPORT void keyboard_event(int, int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_button_event(int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_scroll_event(double, double) noexcept {}