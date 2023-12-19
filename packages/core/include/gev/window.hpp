#pragma once

#include <GLFW/glfw3.h>
#include <memory>

namespace gev
{
  struct glfw_window_deleter
  {
    void operator()(GLFWwindow* window) const
    {
      glfwDestroyWindow(window);
    }
  };
  using unique_window = std::unique_ptr<GLFWwindow, glfw_window_deleter>;
}    // namespace gev