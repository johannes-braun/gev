#pragma once

#include <memory>
#include <GLFW/glfw3.h>

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
}