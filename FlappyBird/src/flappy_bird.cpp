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

} // namespace globals

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

  // Window dims
  const auto [ww, wh] = window::get_dims(window);

  // Texture handles
  static const auto bckg_texture{
      globals::tdb.find("resources/static/background-day.png").value_or(0)};
  static const auto bird_sheet{globals::tdb.find("resources/sheets/bird_red.png").value_or(0)};

  // Background model
  const auto bckg_model{sprite::place(glm::vec2{0.0f}, glm::vec2{ww, wh}, 0.1f)};

  // Updates
  globals::sdb.reset();

  // Background
  globals::sdb.add(bckg_texture, bckg_model, 1.0);

  // Bird position
  const auto bird_model{update_bird_model(window, dt)};

  // Bird flap
  const auto bird_frame_view{update_bird_animation(dt)};

  globals::sdb.add_view(bird_sheet, bird_model, bird_frame_view, glm::vec2{141.0f, 26.0f}, 1.0f);

  return 0;
}

extern "C" SURGE_MODULE_EXPORT void keyboard_event(GLFWwindow *, int, int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_button_event(GLFWwindow *, int, int, int) noexcept {}

extern "C" SURGE_MODULE_EXPORT void mouse_scroll_event(GLFWwindow *, double, double) noexcept {}

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

auto fpb::update_bird_animation(double dt) noexcept -> glm::vec4 {
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

  return frame_views[frame_idx];
}

auto fpb::update_bird_model(GLFWwindow *window, double dt) -> glm::mat4 {
  using namespace surge;
  using namespace surge::atom;

  const auto [ww, wh] = window::get_dims(window);

  const auto dt2{static_cast<float>(dt * dt)};

  const glm::vec2 bbox_size{34.0f, 24.0f};
  const auto x0{ww / 2.0f};

  static float y_center{wh / 2.0f};
  static float y_n{y_center - 5.0f};
  static float y_nm1{y_n};

  const float F_over_m{-50.0f * (y_n - y_center)};

  // Verlet method update
  const float y_np1{2 * y_n - y_nm1 + dt2 * F_over_m};
  y_nm1 = y_n;
  y_n = y_np1;

  const glm::vec2 pos{x0 - bbox_size[0] / 2.0f, y_np1 - bbox_size[0] / 2.0f};

  return sprite::place(pos, bbox_size, 0.2f);
}