#pragma once

#include <vulkan/vulkan.hpp>

namespace gev::game::formats
{
  static constexpr vk::Format forward_pass = vk::Format::eR16G16B16A16Sfloat;
  static constexpr vk::Format shadow_pass = vk::Format::eR32G32Sfloat;
}    // namespace gev::game::formats