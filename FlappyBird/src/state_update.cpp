#include "flappy_bird.hpp"

#include <cmath>

using pipe_queue_t = surge::deque<glm::vec2>;

using acceleration_function = float (*)(float y, float y0);

static inline auto sign(float x) noexcept -> int {
  if (x > 0.0f) {
    return 1;
  } else if (x < 0.0f) {
    return -1;
  } else {
    return 0;
  }
}

static inline auto num_digits(surge::u64 number) noexcept -> surge::u64 {
  // 18,446,744,073,709,551,615
  if (number <= 9) {
    return 1;
  } else if (number <= 99) {
    return 2;
  } else if (number <= 999) {
    return 3;
  } else if (number <= 9999) {
    return 4;
  } else if (number <= 99999) {
    return 5;
  } else if (number <= 999999) {
    return 6;
  } else if (number <= 9999999) {
    return 7;
  } else if (number <= 99999999) {
    return 8;
  } else if (number <= 999999999) {
    return 9;
  } else if (number <= 9999999999) {
    return 10;
  } else {
    return 11;
  }
}

static inline auto harmonic_oscillator(float y, float y0) -> float { return -50.0f * (y - y0); }
static inline auto gravity(float, float) -> float { return 1000.0f; }

static inline void update_background(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                     const glm::vec2 &window_dims) noexcept {
  using namespace surge::gl_atom;

  static const auto bckg_texture{tdb.find("resources/static/background-day.png").value_or(0)};
  const auto bckg_model{sprite::place(glm::vec2{0.0f}, window_dims, 0.1f)};

  sprite_database::add(sdb, bckg_texture, bckg_model, 1.0);
}

static inline void update_rolling_base(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                       const glm::vec2 &window_dims, const glm::vec2 &base_bbox,
                                       float delta_t) noexcept {
  using namespace surge::gl_atom;

  static const auto base_texture{tdb.find("resources/static/base.png").value_or(0)};

  static glm::vec2 base_corner_l{0.0, window_dims[1] - base_bbox[1]};
  static glm::vec2 base_corner_r{base_bbox[0], window_dims[1] - base_bbox[1]};

  const float drift_speed{80.0f};
  const float dt{1.0f / 60.0f};

  static float elapsed{0.0f};
  elapsed += delta_t;

  if (elapsed > dt) {
    base_corner_l[0] -= drift_speed * dt;
    base_corner_r[0] -= drift_speed * dt;
    elapsed -= dt;
  }

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

  sprite_database::add(sdb, base_texture, base_model_l, 1.0);
  sprite_database::add(sdb, base_texture, base_model_r, 1.0);
}

static inline auto update_bird_flap_animation_frame(float delta_t) noexcept -> glm::vec4 {
  static const std::array<glm::vec4, 4> frame_views{
      glm::vec4{1.0f, 1.0f, 34.0f, 24.0f}, glm::vec4{36.0f, 1.0f, 34.0f, 24.0f},
      glm::vec4{71.0f, 1.0f, 34.0f, 24.0f}, glm::vec4{106.0f, 1.0f, 34.0f, 24.0f}};

  static const float frame_rate{10.0f};
  static const float wait_time{1.0f / frame_rate};

  static surge::u8 frame_idx{0};
  static float elapsed{0.0f};

  if (elapsed > wait_time) {
    frame_idx++;
    frame_idx %= 4;
    elapsed = 0;
  } else {
    elapsed += delta_t;
  }

  return frame_views[frame_idx]; // NOLINT
}

static inline void update_bird_physics(float y0, float &y_n, float &vy_n, float delta_t,
                                       bool up_kick, acceleration_function a) noexcept {
  const float dt{1.0f / 60.0f};

  static float elapsed{0.0f};
  elapsed += delta_t;

  if (up_kick) {
    vy_n = -300.0f;
  }

  //  Velocity Verlet method
  if (elapsed > dt) {
    const auto a_n{a(y_n, y0)};
    y_n = y_n + vy_n * dt + 0.5f * a_n * dt * dt;
    const auto a_np1{a(y_n, y0)};
    vy_n = vy_n + 0.5f * (a_n + a_np1) * dt;
    elapsed -= dt;
  }
}

static inline auto update_bird(const fpb::tdb_t &tdb, fpb::sdb_t &sdb, const glm::vec2 &bird_origin,
                               const glm::vec2 &bird_bbox,
                               const glm::vec2 &original_bird_sheet_size, float delta_t,
                               bool up_kick, acceleration_function a) noexcept -> glm::mat4 {

  static const auto bird_sheet{tdb.find("resources/sheets/bird_red.png").value_or(0)};

  const auto flap_frame_view{update_bird_flap_animation_frame(delta_t)};

  static float y_n{bird_origin[1] - 10.0f};
  static float vy_n{0};
  update_bird_physics(bird_origin[1], y_n, vy_n, delta_t, up_kick, a);

  const glm::vec2 bird_pos{bird_origin[0], y_n};
  const auto bird_model{surge::gl_atom::sprite::place(bird_pos, bird_bbox, 0.3f)};

  surge::gl_atom::sprite_database::add_view(sdb, bird_sheet, bird_model, flap_frame_view,
                                            original_bird_sheet_size, 1.0f);

  return bird_model;
}

static inline void update_pipes(const fpb::tdb_t &tdb, fpb::sdb_t &sdb, float delta_t,
                                const glm::vec2 &pipe_gaps, const glm::vec2 &pipe_bbox,
                                pipe_queue_t &pipe_queue) noexcept {
  using namespace surge::gl_atom;

  static const auto pipe_handle{tdb.find("resources/static/pipe-green.png").value_or(0)};

  const float drift_speed{80.0f};
  const float dt{1.0f / 60.0f};

  static float elapsed{0.0f};
  elapsed += delta_t;

  for (auto &pipe_down_pos : pipe_queue) {
    // Place pipe sprites
    const glm::vec2 pipe_up_pos{pipe_down_pos[0], pipe_down_pos[1] - pipe_gaps[1]};

    const auto pipe_down{sprite::place(pipe_down_pos, pipe_bbox, 0.15f)};
    const auto pipe_up{
        glm::translate(glm::rotate(sprite::place(pipe_up_pos, pipe_bbox, 0.15f),
                                   glm::radians(180.0f), glm::vec3{0.0f, 0.0f, 1.0f}),
                       glm::vec3{-1.0f, 0.0f, 0.0f})};

    sprite_database::add(sdb, pipe_handle, pipe_down, 1.0f);
    sprite_database::add(sdb, pipe_handle, pipe_up, 1.0f);

    // Update pipe position
    if (elapsed > dt) {
      pipe_down_pos[0] -= drift_speed * dt;
    }
  }

  if (elapsed > dt) {
    elapsed -= dt;
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
  using std::fabs;

  bool collision{false};

  const glm::vec2 bird_pos{bird_model[3][0], bird_model[3][1]};

  // Ground collision: True if bird bottom equals base top
  // To make sure that the very bottom of the bird touches the ground, we need to introduce a 1px
  // offset. This is probably due to the fact that the sprites have a 1 pixel buffer around then.
  // This should not be necessary, but nevertheless, it is happening.
  // TODO: Fix this
  const auto bird_bottom{bird_pos[1] + bird_bbox[1]};
  const auto base_top{base_pos[1]};
  collision |= (bird_bottom > base_top) || ((base_top - bird_bottom) < 1.0e-1f);

  // Pipe collision: Construct the pipe rects and check bird-pipe for each pipe
  for (const auto &pipe_down_pos : pipe_queue) {
    const glm::vec2 pipe_up_pos{pipe_down_pos[0], 0.0f};
    const glm::vec2 pip_up_bbox{pipe_bbox[0], pipe_down_pos[1] - pipe_gaps[1]};

    collision |= rect_collision(bird_pos, bird_bbox, pipe_down_pos, pipe_bbox);
    collision |= rect_collision(bird_pos, bird_bbox, pipe_up_pos, pip_up_bbox);
  }

  return collision;
}

static inline void compute_score(const pipe_queue_t &pipe_queue, const glm::vec2 &pipe_bbox,
                                 const glm::vec2 &bird_origin, surge::u64 &score) noexcept {
  // Get the right x value of the leftmost pipe
  const auto pipe_right_x{pipe_queue.front()[0] + pipe_bbox[0]};

  // Get the bird x
  const auto bird_x{bird_origin[0]};

  // Bird - pipe distance
  const auto distance{pipe_right_x - bird_x};

  // Distance signs
  const auto curr_dist_sign{sign(distance)};
  static auto prev_dist_sign{curr_dist_sign};

  // If there is a + to - sign shift, we score
  if (curr_dist_sign == -1 && prev_dist_sign == 1) {
    score += 1;
  }

  prev_dist_sign = curr_dist_sign;
}

static inline void update_instructions_msg(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                           const glm::vec2 &window_dims,
                                           const glm::vec2 &bird_origin, const glm::vec2 &bird_bbox,
                                           const glm::vec2 &instructions_1_bbox,
                                           const glm::vec2 &instructions_2_bbox) noexcept {
  using namespace surge::gl_atom;

  static const auto instructions_1_texture{
      tdb.find("resources/text/instructions_1.png").value_or(0)};

  static const auto instructions_2_texture{
      tdb.find("resources/text/instructions_2.png").value_or(0)};

  const glm::vec2 instructions_1_pos{(window_dims[0] - instructions_1_bbox[0]) / 2.0f, 0.0f};
  const auto instructions_1_model{sprite::place(instructions_1_pos, instructions_1_bbox, 0.5f)};

  const glm::vec2 bird_center{bird_origin - bird_bbox / 2.0f};
  const glm::vec2 instructions_2_pos{bird_center[0] - instructions_2_bbox[0] / 2.0f,
                                     bird_origin[1] + 60.0f};
  const auto instructions_2_model{sprite::place(instructions_2_pos, instructions_1_bbox, 0.5f)};

  sprite_database::add(sdb, instructions_1_texture, instructions_1_model, 1.0);
  sprite_database::add(sdb, instructions_2_texture, instructions_2_model, 1.0);
}

static inline void update_score_msg(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                    const glm::vec2 &window_dims, const glm::vec2 &numbers_bbox,
                                    const surge::u64 &score) noexcept {
  using namespace surge::gl_atom;

  static const auto texture_0{tdb.find("resources/numbers/0.png").value_or(0)};
  static const auto texture_1{tdb.find("resources/numbers/1.png").value_or(0)};
  static const auto texture_2{tdb.find("resources/numbers/2.png").value_or(0)};
  static const auto texture_3{tdb.find("resources/numbers/3.png").value_or(0)};
  static const auto texture_4{tdb.find("resources/numbers/4.png").value_or(0)};
  static const auto texture_5{tdb.find("resources/numbers/5.png").value_or(0)};
  static const auto texture_6{tdb.find("resources/numbers/6.png").value_or(0)};
  static const auto texture_7{tdb.find("resources/numbers/7.png").value_or(0)};
  static const auto texture_8{tdb.find("resources/numbers/8.png").value_or(0)};
  static const auto texture_9{tdb.find("resources/numbers/9.png").value_or(0)};

  // Score total width
  const auto socre_digits{num_digits(score)};
  const auto score_width{numbers_bbox[1] * static_cast<float>(socre_digits)};

  // Score start and end
  const auto score_start_x{(window_dims[0] - score_width) / 2.0f};
  const auto score_end_x{score_start_x + score_width};

  // Score y
  const auto score_y{window_dims[1] / 10.0f};

  // Loop over score digits, lowest to highest
  auto score_cursor{score_end_x - numbers_bbox[0]};
  auto local_score{score};

  if (local_score == 0) {
    sprite_database::add(sdb, texture_0,
                         sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
    return;
  }

  while (local_score > 0) {
    const auto digit{local_score % 10};

    switch (digit) {

    case 0:
      sprite_database::add(
          sdb, texture_0, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 1:
      sprite_database::add(
          sdb, texture_1, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 2:
      sprite_database::add(
          sdb, texture_2, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 3:
      sprite_database::add(
          sdb, texture_3, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 4:
      sprite_database::add(
          sdb, texture_4, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 5:
      sprite_database::add(
          sdb, texture_5, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 6:
      sprite_database::add(
          sdb, texture_6, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 7:
      sprite_database::add(
          sdb, texture_7, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 8:
      sprite_database::add(
          sdb, texture_8, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    case 9:
      sprite_database::add(
          sdb, texture_9, sprite::place(glm::vec2{score_cursor, score_y}, numbers_bbox, 0.5f), 1.0);
      break;

    default:
      break;
    }

    local_score /= 10;
    score_cursor -= numbers_bbox[0];
  }
}

static inline void update_game_over_msg(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                        const glm::vec2 &window_dims,
                                        const glm::vec2 &game_over_bbox) noexcept {
  using namespace surge::gl_atom;

  static const auto game_over_texture{tdb.find("resources/text/gameover.png").value_or(0)};

  const auto game_over_pos{(window_dims - game_over_bbox) / 2.0f};
  const auto game_over_model{sprite::place(game_over_pos, game_over_bbox, 0.5f)};

  sprite_database::add(sdb, game_over_texture, game_over_model, 1.0);
}

static inline void update_state_prepare(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                        const glm::vec2 &window_dims, const glm::vec2 &base_bbox,
                                        const glm::vec2 &bird_origin, const glm::vec2 &bird_bbox,
                                        const glm::vec2 &original_bird_sheet_size,
                                        const glm::vec2 &instructions_1_bbox,
                                        const glm::vec2 &instructions_2_bbox,
                                        float delta_t) noexcept {
  // Database reset
  surge::gl_atom::sprite_database::begin_add(sdb);

  // Background
  update_background(tdb, sdb, window_dims);

  // Rolling base
  update_rolling_base(tdb, sdb, window_dims, base_bbox, delta_t);

  // Bird
  update_bird(tdb, sdb, bird_origin, bird_bbox, original_bird_sheet_size, delta_t, false,
              harmonic_oscillator);

  // Instructions
  update_instructions_msg(tdb, sdb, window_dims, bird_origin, bird_bbox, instructions_1_bbox,
                          instructions_2_bbox);
}

static inline auto update_state_play(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                     const glm::vec2 &window_dims, const glm::vec2 &base_bbox,
                                     const glm::vec2 &bird_origin, const glm::vec2 &bird_bbox,
                                     const glm::vec2 &original_bird_sheet_size,
                                     const glm::vec2 &pipe_gaps, const glm::vec2 &pipe_bbox,
                                     pipe_queue_t &pipe_queue, const glm::vec2 &numbers_bbox,
                                     surge::u64 &score, float delta_t) noexcept -> bool {
  // Database reset
  surge::gl_atom::sprite_database::begin_add(sdb);

  // Background
  update_background(tdb, sdb, window_dims);

  // Rolling base
  const glm::vec2 &base_pos{0.0, window_dims[1] - base_bbox[1]};
  update_rolling_base(tdb, sdb, window_dims, base_bbox, delta_t);

  // Pipes
  update_pipes(tdb, sdb, delta_t, pipe_gaps, pipe_bbox, pipe_queue);

  // Bird
  static auto old_click_state{GLFW_RELEASE}; // We enter this state on a mouse press
  const auto current_click_state{surge::window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT)};
  const auto up_kick{current_click_state == GLFW_PRESS && old_click_state == GLFW_RELEASE};

  const auto bird_model{update_bird(tdb, sdb, bird_origin, bird_bbox, original_bird_sheet_size,
                                    delta_t, up_kick, gravity)};

  // Update collisions
  const auto collided{
      update_collision(bird_model, bird_bbox, base_pos, pipe_gaps, pipe_bbox, pipe_queue)};

  // Update score
  if (!collided) {
    compute_score(pipe_queue, pipe_bbox, bird_origin, score);
  }

  update_score_msg(tdb, sdb, window_dims, numbers_bbox, score);

  // Refresh click cache
  old_click_state = surge::window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT);

  return collided;
}

static inline void update_score(const fpb::tdb_t &tdb, fpb::sdb_t &sdb,
                                const glm::vec2 &window_dims, const glm::vec2 &base_bbox,
                                const glm::vec2 &bird_origin, const glm::vec2 &bird_bbox,
                                const glm::vec2 &original_bird_sheet_size,
                                const glm::vec2 &pipe_gaps, const glm::vec2 &pipe_bbox,
                                pipe_queue_t &pipe_queue, const glm::vec2 &numbers_bbox,
                                const glm::vec2 &game_over_bbox, surge::u64 &score) {
  const float delta_t{0.0f};

  surge::gl_atom::sprite_database::begin_add(sdb);

  update_background(tdb, sdb, window_dims);
  update_rolling_base(tdb, sdb, window_dims, base_bbox, delta_t);
  update_pipes(tdb, sdb, delta_t, pipe_gaps, pipe_bbox, pipe_queue);
  update_bird(tdb, sdb, bird_origin, bird_bbox, original_bird_sheet_size, delta_t, false, gravity);
  update_score_msg(tdb, sdb, window_dims, numbers_bbox, score);
  update_game_over_msg(tdb, sdb, window_dims, game_over_bbox);
}

void fpb::state_machine::state_update(const fpb::tdb_t &tdb, fpb::sdb_t &sdb, const state &state_a,
                                      state &state_b, double delta_t) noexcept {
  using namespace surge;
  using namespace fpb::state_machine;

  // Float conversions
  const auto fdelta_t{static_cast<float>(delta_t)};

  // Original sizes
  const glm::vec2 original_window_size{288.0f, 512.0f};
  const glm::vec2 original_bird_bbox{34.0f, 24.0f};
  const glm::vec2 original_base_bbox{288.0f, 112.0f};
  const glm::vec2 original_pipe_bbox{52.0f, 32.0f};

  const glm::vec2 original_bird_sheet_size{141.0f, 26.0f};

  const glm::vec2 original_instructions_1_size{184.0f, 152.0f};
  const glm::vec2 original_instructions_2_size{114.0f, 60.0f};
  const glm::vec2 original_game_over_size{192.0f, 42.0f};

  const glm::vec2 original_numnbers_size{24.0f, 36.0f};

  const auto window_dims{window::get_dims()};
  const auto scale_factor{window_dims / original_window_size};

  // Base sizes
  const auto base_bbox{original_base_bbox * scale_factor};

  // Bird sizes
  const auto bird_bbox{original_bird_bbox * scale_factor};
  const glm::vec2 bird_origin{window_dims[0] / 3.0f - bird_bbox[0] / 2.0f,
                              window_dims[1] / 2.0f - bird_bbox[1] / 2.0f};

  // Pipe sizes
  const glm::vec2 pipe_gaps{window_dims[0] / 2.0f, 150.0f};
  const glm::vec2 pipe_bbox{original_pipe_bbox[0] * scale_factor[0], window_dims[1]};

  // Instructions size
  const auto instructions_1_bbox{original_instructions_1_size * scale_factor};
  const glm::vec2 instructions_2_bbox{original_instructions_2_size * scale_factor};

  // Game over screen size
  const auto game_over_bbox{original_game_over_size * scale_factor};

  // Score numbers size
  const auto numbers_bbox{original_numnbers_size * scale_factor};

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

  // Game score
  static surge::u64 score{0};

  // State switch
  switch (state_a) {

  case state::prepare:
    update_state_prepare(tdb, sdb, window_dims, base_bbox, bird_origin, bird_bbox,
                         original_bird_sheet_size, instructions_1_bbox, instructions_2_bbox,
                         fdelta_t);

    if (window::get_mouse_button(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      state_b = state::play;
    }
    break;

  case state::play:
    if (update_state_play(tdb, sdb, window_dims, base_bbox, bird_origin, bird_bbox,
                          original_bird_sheet_size, pipe_gaps, pipe_bbox, pipe_queue, numbers_bbox,
                          score, fdelta_t)) {
      gl_atom::sprite_database::wait_idle(sdb);
      state_b = state::score;
    }
    break;

  case state::score:
    update_score(tdb, sdb, window_dims, base_bbox, bird_origin, bird_bbox, original_bird_sheet_size,
                 pipe_gaps, pipe_bbox, pipe_queue, numbers_bbox, game_over_bbox, score);
    break;

  default:
    break;
  }
}
