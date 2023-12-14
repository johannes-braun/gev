#include <gev/game/material.hpp>
#include <gev/game/mesh_renderer.hpp>

namespace gev::game
{
  void material::load_diffuse(std::shared_ptr<texture> dt)
  {
    _diffuse = std::move(dt);
    if(_texture_descriptor)
      _diffuse->bind(_texture_descriptor, 0);
  }

  void material::load_roughness(std::shared_ptr<texture> rt)
  {
    _roughness = std::move(rt);
    if(_texture_descriptor)
      _roughness->bind(_texture_descriptor, 1);
  }

  void material::set_two_sided(bool enable)
  {
    _two_sided = enable;
  }

  void material::bind(frame const& f, mesh_renderer& r)
  {
    if (!_texture_descriptor)
    {
      _texture_descriptor = gev::engine::get().get_descriptor_allocator().allocate(r.material_set_layout());
      _diffuse->bind(_texture_descriptor, 0);
      _roughness->bind(_texture_descriptor, 1);
    }

    f.command_buffer.setCullMode(_two_sided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack);
    f.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, r.pipeline_layout(), mesh_renderer::material_set, _texture_descriptor, nullptr);
  }
}