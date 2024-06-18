#include "flappy_bird.hpp"

#include "type_aliases.hpp"

//clang-format off
#include "player/error_types.hpp"
#include "player/logging.hpp"
// clang-format on

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace globals {

static fpb::tdb_t tdb{};      // NOLINT
static fpb::pvubo_t pv_ubo{}; // NOLINT
static fpb::sdb_t sdb{};      // NOLINT

static fpb::state state_a{}; // NOLINT
static fpb::state state_b{}; // NOLINT

} // namespace globals

auto fpb::bind_callbacks(GLFWwindow *window) noexcept -> int {
  log_info("Binding interaction callbacks");

  glfwSetKeyCallback(window, keyboard_event);
  if (glfwGetError(nullptr) != GLFW_NO_ERROR) {
    log_warn("Unable to bind keyboard event callback");
    return static_cast<int>(surge::error::keyboard_event_unbinding);
  }

  glfwSetMouseButtonCallback(window, mouse_button_event);
  if (glfwGetError(nullptr) != GLFW_NO_ERROR) {
    log_warn("Unable to bind mouse button event callback");
    return static_cast<int>(surge::error::mouse_button_event_unbinding);
  }

  glfwSetScrollCallback(window, mouse_scroll_event);
  if (glfwGetError(nullptr) != GLFW_NO_ERROR) {
    log_warn("Unable to bind mouse scroll event callback");
    return static_cast<int>(surge::error::mouse_scroll_event_unbinding);
  }

  return 0;
}

auto fpb::unbind_callbacks(GLFWwindow *window) noexcept -> int {
  log_info("Unbinding interaction callbacks");

  glfwSetKeyCallback(window, nullptr);
  if (glfwGetError(nullptr) != GLFW_NO_ERROR) {
    log_warn("Unable to unbind keyboard event callback");
  }

  glfwSetMouseButtonCallback(window, nullptr);
  if (glfwGetError(nullptr) != GLFW_NO_ERROR) {
    log_warn("Unable to unbind mouse button event callback");
  }

  glfwSetScrollCallback(window, nullptr);
  if (glfwGetError(nullptr) != GLFW_NO_ERROR) {
    log_warn("Unable to unbind mouse scroll event callback");
  }

  return 0;
}

void fpb::state_transition() noexcept {
  using namespace globals;

  const auto a_empty_b_empty{state_a == state::no_state && state_b == state::no_state};
  const auto a_full_b_empty{state_a != state::no_state && state_b == state::no_state};

  if (!(a_empty_b_empty || a_full_b_empty)) {
    state_a = state_b;
    state_b = state::no_state;
  }
}

void fpb::state_update(GLFWwindow *window, double dt) noexcept {
  switch (globals::state_a) {

  case state::prepare:
    update_state_prepare(window, dt);
    break;

  case state::play:
    update_state_play(window, dt);
    break;

  case state::score:
    update_state_score(window, dt);
    break;

  default:
    break;
  }
}

void fpb::update_state_prepare(GLFWwindow *window, double dt) noexcept {
  using namespace surge;
  using namespace surge::atom;

  // Window dims
  const auto [ww, wh] = window::get_dims(window);

  // Texture handles
  static const auto bckg_texture{
      globals::tdb.find("resources/static/background-day.png").value_or(0)};
  static const auto bird_sheet{globals::tdb.find("resources/sheets/bird_red.png").value_or(0)};

  // Background model
  const auto bckg_model{sprite::place(glm::vec2{0.0f}, glm::vec2{ww, wh}, 0.1f)};

  // Database reset
  globals::sdb.reset();

  // Background
  globals::sdb.add(bckg_texture, bckg_model, 1.0);

  // Bird position (Velocity Verlet update)
  const auto fdt{static_cast<float>(dt)};
  const auto dt2{static_cast<float>(dt * dt)};

  const glm::vec2 bbox_size{34.0f, 24.0f};
  const auto x0{ww / 2.0f - bbox_size[0] / 2.0f};
  const auto y0{wh / 2.0f - bbox_size[1] / 2.0f};

  static float y_n{y0};
  static float vy_n{0.0f};

  // auto a = [&](float y) -> float { return -50.0f * (y - y0); };
  auto a = [&](float) -> float {
    // return (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) ? -100.0f : 50.0f;
    return 50.0f - 1.0f * vy_n;
  };
  const auto a_n{a(y_n)};

  y_n = y_n + vy_n * fdt + 0.5f * a_n * dt2;
  const auto a_np1{a(y_n)};
  vy_n = vy_n + 0.5f * (a_n + a_np1) * fdt;

  const auto bird_model{sprite::place(glm::vec2{x0, y_n}, bbox_size, 0.2f)};

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

  // Push to database
  globals::sdb.add_view(bird_sheet, bird_model, bird_frame_view, glm::vec2{141.0f, 26.0f}, 1.0f);
}

void fpb::update_state_play(GLFWwindow *, double) noexcept {}

void fpb::update_state_score(GLFWwindow *, double) noexcept {}

extern "C" SURGE_MODULE_EXPORT auto on_load(GLFWwindow *window) noexcept -> int {
  using namespace fpb;
  using namespace surge;
  using namespace surge::atom;

  // Bind callbacks
  const auto bind_callback_stat{bind_callbacks(window)};
  if (bind_callback_stat != 0) {
    return bind_callback_stat;
  }

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
  const auto [ww, wh] = window::get_dims(window);
  const auto projection{glm::ortho(0.0f, ww, wh, 0.0f, 0.0f, 1.0f)};
  const auto view{glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                              glm::vec3(0.0f, 1.0f, 0.0f))};

  // PV UBO
  globals::pv_ubo = pv_ubo::buffer::create();
  globals::pv_ubo.update_all(&projection, &view);

  // Load game resources
  // All textures
  texture::create_info ci{};
  ci.filtering = renderer::texture_filtering::nearest;
  globals::tdb.add(ci, "resources/static/background-day.png", "resources/sheets/bird_red.png");

  // First state
  globals::state_b = fpb::state::prepare;
  state_transition();

  return 0;
}

extern "C" SURGE_MODULE_EXPORT auto on_unload(GLFWwindow *window) noexcept -> int {
  using namespace fpb;

  globals::pv_ubo.destroy();
  globals::sdb.destroy();

  globals::tdb.destroy();

  // Unbind callbacks
  const auto unbind_callback_stat{unbind_callbacks(window)};
  if (unbind_callback_stat != 0) {
    return unbind_callback_stat;
  }

  return 0;
}

extern "C" SURGE_MODULE_EXPORT auto draw(GLFWwindow *) noexcept -> int {
  globals::pv_ubo.bind_to_location(2);

  globals::sdb.draw();

  return 0;
}

extern "C" SURGE_MODULE_EXPORT auto update(GLFWwindow *window, double dt) noexcept -> int {
  using namespace fpb;
  using namespace surge;
  using namespace surge::atom;

  // Update states
  state_transition();
  state_update(window, dt);

  return 0;
}

extern "C" SURGE_MODULE_EXPORT void keyboard_event(GLFWwindow *, int, int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_button_event(GLFWwindow *, int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_scroll_event(GLFWwindow *, double, double) noexcept {}