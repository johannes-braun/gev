#pragma once

#include <memory>
#include <gev/image.hpp>
#include "environment_shader.hpp"
#include <gev/game/mesh_renderer.hpp>
#include "main_controls.hpp"

class environment
{
public:
  constexpr static std::uint32_t diffuse_size = 8;

  environment(vk::Format cube_format = vk::Format::eB10G11R11UfloatPack32);

  void render(vk::CommandBuffer c, std::int32_t x, std::int32_t y, std::uint32_t w, std::uint32_t h);

private:
  void render_cube(vk::CommandBuffer c);

  vk::Format _format;
  std::shared_ptr<gev::image> _cubemap;
  vk::UniqueImageView _cubemap_view;
  std::shared_ptr<gev::image> _diffuse_cubemap;
  vk::UniqueImageView _diffuse_cubemap_view;

  std::shared_ptr<environment_shader> _forward_shader;
  std::shared_ptr<environment_cube_shader> _cube_shader;
  std::shared_ptr<environment_blur_shader> _blur_shader;

  gev::service_proxy<main_controls> _controls;
  gev::service_proxy<gev::game::renderer> _renderer;

  vk::DescriptorSet _environment_set;
};