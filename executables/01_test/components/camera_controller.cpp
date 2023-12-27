#include "camera_controller.hpp"

void camera_controller::spawn()
{
  _camera = owner()->get<camera_component>();
  if (auto c = _camera.lock())
  {
    auto const r = gev::service<gev::game::mesh_renderer>();
    r->set_camera(c->camera());
  }
}

void camera_controller::update()
{
  _base_camera.set_position(owner()->global_transform().position);
  _base_camera.replace_rotation(owner()->global_transform().rotation);

  auto const window = gev::engine::get().window();
  auto const w = gev::engine::get().swapchain_size().width;
  auto const h = gev::engine::get().swapchain_size().height;
  auto const delta = gev::engine::get().current_frame().delta_time;
  bool const allow_input = !(ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard);

  _axises.to(rnu::vec3{glfwGetKey(window, GLFW_KEY_W) - glfwGetKey(window, GLFW_KEY_S),
               glfwGetKey(window, GLFW_KEY_A) - glfwGetKey(window, GLFW_KEY_D),
               glfwGetKey(window, GLFW_KEY_Q) - glfwGetKey(window, GLFW_KEY_E)} *
    (5.f + 10 * glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)));
  _axises.update(20 * delta);

  _base_camera.axis(float(delta), _axises.value().x, _axises.value().y, _axises.value().z);

  double mx, my;
  glfwGetCursorPos(window, &mx, &my);
  if (!_down && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) && allow_input)
  {
    _last_cursor_position = {mx, my};
    _dst_cursor_offset = {mx, my};
    _down = true;
    _cursor = rnu::smooth(rnu::vec2{mx, my});
  }
  else if (!allow_input || (_down && !glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)))
  {
    _base_camera.mouse(0, 0, false);
    _down = false;
  }

  if (_down)
  {
    float dx = mx - _last_cursor_position.x;
    float dy = my - _last_cursor_position.y;
    _dst_cursor_offset.x += dx * _mouse_sensitivity;
    _dst_cursor_offset.y += dy * _mouse_sensitivity;
    glfwSetCursorPos(window, _last_cursor_position.x, _last_cursor_position.y);
    _cursor.to(_dst_cursor_offset);
  }
  _cursor.update(20 * delta);

  _base_camera.mouse(_cursor.value().x, _cursor.value().y, !_cursor.finished());

  owner()->local_transform.position = _base_camera.position();
  owner()->local_transform.rotation = _base_camera.rotation();

  if (!_camera.expired())
  {
    auto const size = gev::engine::get().swapchain_size();
    _camera.lock()->camera()->set_projection(
      rnu::cameraf::projection(rnu::radians(60.0f), size.width / float(size.height), 0.01f, 1000.0f, false, true));
  }
}