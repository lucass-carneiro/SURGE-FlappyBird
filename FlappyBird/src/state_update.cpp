#include "flappy_bird.hpp"

using pipe_queue_t = surge::deque<glm::vec2>;

using acceleration_function = float (*)(float y, float y0);

static inline auto harmonic_oscillator(float y, float y0) -> float { return -50.0f * (y - y0); }
static inline auto gravity(float, float) -> float { return 1000.0f; }

static inline void update_background(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                     const glm::vec2 &window_dims) noexcept {
  using namespace surge::gl_atom;

  static const auto bckg_texture{tdb.find("resources/static/background-day.png").value_or(0)};
  const auto bckg_model{sprite::place(glm::vec2{0.0f}, window_dims, 0.1f)};

  sdb.add(bckg_texture, bckg_model, 1.0);
}

static inline void update_rolling_base(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                       const glm::vec2 &window_dims, const glm::vec2 &base_bbox,
                                       float drift_speed) noexcept {
  using namespace surge::gl_atom;

  static const auto base_texture{tdb.find("resources/static/base.png").value_or(0)};

  static glm::vec2 base_corner_l{0.0, window_dims[1] - base_bbox[1]};
  static glm::vec2 base_corner_r{base_bbox[0], window_dims[1] - base_bbox[1]};

  base_corner_l[0] -= drift_speed;
  base_corner_r[0] -= drift_speed;

  if (base_corner_l[0] < 0.0f && (base_bbox[0] + base_corner_l[0]) < 1.0e-6) {
    base_corner_l[0] = base_bbox[0];
    base_corner_r[0] = 0.0f;
  }

  if (base_corner_r[0] < 0.0f && (base_bbox[0] + base_corner_r[0]) < 1.0e-6) {
    base_corner_r[0] = base_bbox[0];
    base_corner_l[0] = 0.0f;
  }

  const auto base_model_l{sprite::place(base_corner_l, base_bbox, 0.2f)};
  const auto base_model_r{sprite::place(base_corner_r, base_bbox, 0.2f)};

  sdb.add(base_texture, base_model_l, 1.0);
  sdb.add(base_texture, base_model_r, 1.0);
}

static inline auto update_bird_flap_animation_frame(float dt) noexcept -> glm::vec4 {
  static const std::array<glm::vec4, 4> frame_views{
      glm::vec4{1.0f, 1.0f, 34.0f, 24.0f}, glm::vec4{36.0f, 1.0f, 34.0f, 24.0f},
      glm::vec4{71.0f, 1.0f, 34.0f, 24.0f}, glm::vec4{106.0f, 1.0f, 34.0f, 24.0f}};

  static const double frame_rate{10.0};
  static const double wait_time{1.0 / frame_rate};

  static surge::u8 frame_idx{0};
  static float elapsed{0};

  if (elapsed > wait_time) {
    frame_idx++;
    frame_idx %= 4;
    elapsed = 0;
  } else {
    elapsed += dt;
  }

  return frame_views[frame_idx]; // NOLINT
}

static inline void update_bird_physics(float y0, float &y_n, float &vy_n, float dt, float dt2,
                                       bool up_kick, bool collided,
                                       acceleration_function a) noexcept {
  // Velocity Verlet method
  if (up_kick) {
    vy_n = -300.0f;
  }

  if (collided) {
    vy_n = 0.0f;
  }

  const auto a_n{collided ? 0.0f : a(y_n, y0)};
  y_n = y_n + vy_n * dt + 0.5f * a_n * dt2;
  const auto a_np1{collided ? 0.0f : a(y_n, y0)};
  vy_n = vy_n + 0.5f * (a_n + a_np1) * dt;
}

static inline auto update_bird(const fpb::tdb_t &tdb, fpb::sdb_t &sdb, const glm::vec2 &bird_origin,
                               const glm::vec2 &bird_bbox,
                               const glm::vec2 &original_bird_sheet_size, float dt, float dt2,
                               bool up_kick, acceleration_function a) noexcept -> glm::mat4 {

  static const auto bird_sheet{tdb.find("resources/sheets/bird_red.png").value_or(0)};

  const auto flap_frame_view{update_bird_flap_animation_frame(dt)};

  static float y_n{bird_origin[1] - 10.0f};
  static float vy_n{0};
  update_bird_physics(bird_origin[1], y_n, vy_n, dt, dt2, up_kick, false, a);

  const glm::vec2 bird_pos{bird_origin[0], y_n};
  const auto bird_model{surge::gl_atom::sprite::place(bird_pos, bird_bbox, 0.3f)};

  sdb.add_view(bird_sheet, bird_model, flap_frame_view, original_bird_sheet_size, 1.0f);

  return bird_model;
}

static inline void update_pipes(const fpb::tdb_t &tdb, fpb::sdb_t &sdb, float drift_speed,
                                const glm::vec2 &pipe_gaps, const glm::vec2 &pipe_bbox,
                                pipe_queue_t &pipe_queue) noexcept {
  using namespace surge::gl_atom;

  static const auto pipe_handle{tdb.find("resources/static/pipe-green.png").value_or(0)};

  for (auto &pipe_down_pos : pipe_queue) {
    // Place pipe sprites
    const glm::vec2 pipe_up_pos{pipe_down_pos[0], pipe_down_pos[1] - pipe_gaps[1]};

    const auto pipe_down{sprite::place(pipe_down_pos, pipe_bbox, 0.15f)};
    const auto pipe_up{
        glm::translate(glm::rotate(sprite::place(pipe_up_pos, pipe_bbox, 0.15f),
                                   glm::radians(180.0f), glm::vec3{0.0f, 0.0f, 1.0f}),
                       glm::vec3{-1.0f, 0.0f, 0.0f})};

    sdb.add(pipe_handle, pipe_down, 1.0f);
    sdb.add(pipe_handle, pipe_up, 1.0f);

    // Update pipe position
    pipe_down_pos[0] -= drift_speed;
  }

  // Check if leftmost pipe left the screen
  if ((pipe_queue.front()[0] + pipe_bbox[0]) < 0) {
    pipe_queue.pop_front();
  }
}

static inline auto rect_collision(const glm::vec2 &rect1_start, const glm::vec2 &rect1_dims,
                                  const glm::vec2 &rect2_start,
                                  const glm::vec2 &rect2_dims) noexcept -> bool {
  const auto r1x{rect1_start[0]};
  const auto r1y{rect1_start[1]};
  const auto r1w{rect1_dims[0]};
  const auto r1h{rect1_dims[1]};

  const auto r2x{rect2_start[0]};
  const auto r2y{rect2_start[1]};
  const auto r2w{rect2_dims[0]};
  const auto r2h{rect2_dims[1]};

  return r1x < r2x + r2w && r1x + r1w > r2x && r1y < r2y + r2h && r1y + r1h > r2y;
}

static inline auto update_collision(const glm::mat4 &bird_model, const glm::vec2 &bird_bbox,
                                    const glm::vec2 &base_pos, const ::glm::vec2 &pipe_gaps,
                                    const ::glm::vec2 &pipe_bbox,
                                    const pipe_queue_t &pipe_queue) noexcept -> bool {
  bool collision{false};

  const glm::vec2 bird_pos{bird_model[3][0], bird_model[3][1]};

  // Ground collision: True if bird bottom equals base top
  // To make sure that the very bottom of the bird touches the ground, we need to introduce a 1px
  // offset. This is probably due to the fact that the sprites have a 1 pixel buffer around then.
  // This should not be necessary, but nevertheless, it is happening.
  // TODO: Fix this
  const auto bird_bottom{bird_pos[1] + bird_bbox[1]};
  const auto base_top{base_pos[1]};
  collision |= ((base_top - bird_bottom + 2.0f) < 1.0e-6f);

  // Pipe collision: Construct the pipe rects and check bird-pipe for each pipe
  for (const auto &pipe_down_pos : pipe_queue) {
    const glm::vec2 pipe_up_pos{pipe_down_pos[0], 0.0f};
    const glm::vec2 pip_up_bbox{pipe_bbox[0], pipe_down_pos[1] - pipe_gaps[1]};

    collision |= rect_collision(bird_pos, bird_bbox, pipe_down_pos, pipe_bbox);
    collision |= rect_collision(bird_pos, bird_bbox, pipe_up_pos, pip_up_bbox);
  }

  return collision;
}

static inline void update_state_prepare(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                        const glm::vec2 &window_dims, const glm::vec2 &base_bbox,
                                        const glm::vec2 &bird_origin, const glm::vec2 &bird_bbox,
                                        const glm::vec2 &original_bird_sheet_size,
                                        float drift_speed, float dt, float dt2) noexcept {
  // Database reset
  sdb.reset();

  // Background
  update_background(tdb, sdb, window_dims);

  // Rolling base
  update_rolling_base(tdb, sdb, window_dims, base_bbox, drift_speed);

  // Bird
  update_bird(tdb, sdb, bird_origin, bird_bbox, original_bird_sheet_size, dt, dt2, false,
              harmonic_oscillator);
}

static inline auto update_state_play(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                     const glm::vec2 &window_dims, const glm::vec2 &base_bbox,
                                     const glm::vec2 &bird_origin, const glm::vec2 &bird_bbox,
                                     const glm::vec2 &original_bird_sheet_size,
                                     const glm::vec2 &pipe_gaps, const glm::vec2 &pipe_bbox,
                                     pipe_queue_t &pipe_queue, float drift_speed, float dt,
                                     float dt2) noexcept -> bool {
  // Database reset
  sdb.reset();

  // Background
  update_background(tdb, sdb, window_dims);

  // Rolling base
  const glm::vec2 &base_pos{0.0, window_dims[1] - base_bbox[1]};
  update_rolling_base(tdb, sdb, window_dims, base_bbox, drift_speed);

  // Pipes
  update_pipes(tdb, sdb, drift_speed, pipe_gaps, pipe_bbox, pipe_queue);

  // Bird
  static auto old_click_state{GLFW_RELEASE}; // We enter this state on a mouse press
  const auto current_click_state{surge::window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT)};
  const auto up_kick{current_click_state == GLFW_PRESS && old_click_state == GLFW_RELEASE};

  const auto bird_model{update_bird(tdb, sdb, bird_origin, bird_bbox, original_bird_sheet_size, dt,
                                    dt2, up_kick, gravity)};

  // Refresh click cache
  old_click_state = surge::window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT);

  return update_collision(bird_model, bird_bbox, base_pos, pipe_gaps, pipe_bbox, pipe_queue);
}

static inline void update_score() {}

void fpb::state_machine::state_update(const fpb::tdb_t &tdb, fpb::sdb_t &sdb, const state &state_a,
                                      state &state_b, double dt) noexcept {
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

  const glm::vec2 original_bird_sheet_size{141.0f, 26.0f};

  const auto window_dims{window::get_dims()};
  const auto scale_factor{window_dims / original_window_size};

  // Base sizes
  const float drift_speed{static_cast<float>(80.0 * dt)};
  const auto base_bbox{original_base_bbox * scale_factor};

  // Bird sizes
  const auto bird_bbox{original_bird_bbox * scale_factor};
  const glm::vec2 bird_origin{window_dims[0] / 3.0f - bird_bbox[0] / 2.0f,
                              window_dims[1] / 2.0f - bird_bbox[1] / 2.0f};

  // Pipe sizes
  const glm::vec2 pipe_gaps{window_dims[0] / 2.0f, 150.0f};
  const glm::vec2 pipe_bbox{original_pipe_bbox[0] * scale_factor[0], window_dims[1]};

  // Allowed pipe y range
  const float allowed_pipe_area_fraction{(window_dims[1] - base_bbox[1]) / 4.0f};
  std::uniform_real_distribution<float> pipe_y_range{
      allowed_pipe_area_fraction, window_dims[1] - base_bbox[1] - allowed_pipe_area_fraction};

  // Initialize pipe position queue
  static std::minstd_rand engine{std::random_device{}()};

  static pipe_queue_t pipe_queue{
      {glm::vec2{window_dims[0], pipe_y_range(engine)},
       glm::vec2{window_dims[0] + window_dims[0] / 2.0f, pipe_y_range(engine)},
       glm::vec2{window_dims[0] + 2.0f * window_dims[0] / 2.0f, pipe_y_range(engine)},
       glm::vec2{window_dims[0] + 3.0f * window_dims[0] / 2.0f, pipe_y_range(engine)}}};

  // Keep pipe number constant
  if (pipe_queue.size() != 4) {
    const auto &last_pipe{pipe_queue.back()};
    const glm::vec2 new_pipe{last_pipe[0] + window_dims[0] / 2.0f, pipe_y_range(engine)};
    pipe_queue.emplace_back(new_pipe);
  }

  // State switch
  switch (state_a) {

  case state::prepare:
    update_state_prepare(tdb, sdb, window_dims, base_bbox, bird_origin, bird_bbox,
                         original_bird_sheet_size, drift_speed, fdt, fdt2);

    if (window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      state_b = state::play;
    }
    break;

  case state::play:
    if (update_state_play(tdb, sdb, window_dims, base_bbox, bird_origin, bird_bbox,
                          original_bird_sheet_size, pipe_gaps, pipe_bbox, pipe_queue, drift_speed,
                          fdt, fdt2)) {
      state_b = state::score;
    }
    break;

  case state::score:
    update_score();
    break;

  default:
    break;
  }
}