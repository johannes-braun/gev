#include <gev/game/layouts.hpp>
#include <gev/game/material.hpp>
#include <gev/game/mesh_renderer.hpp>

namespace gev::game
{
  void material::load_diffuse(std::shared_ptr<texture> dt)
  {
    _diffuse = std::move(dt);
    if (_texture_descriptor)
      bind_or_unbind(_diffuse, 0, material_info::has_diffuse);
  }

  void material::load_roughness(std::shared_ptr<texture> rt)
  {
    _roughness = std::move(rt);
    if (_texture_descriptor)
      bind_or_unbind(_roughness, 1, material_info::has_roughness);
  }

  void material::set_diffuse(rnu::vec4 color)
  {
    _data.diffuse = rnu::vec4ui8(rnu::clamp(color, 0.0, 1.0) * 255);
    _update_buffer = true;
  }

  void material::set_roughness(float r)
  {
    _data.roughness = std::clamp(r, 0.f, 1.f);
    _update_buffer = true;
  }

  void material::set_two_sided(bool enable)
  {
    _two_sided = enable;
  }

  void material::bind_or_unbind(std::shared_ptr<texture> const& t, std::uint32_t binding, std::uint32_t flags)
  {
    if (t)
    {
      t->bind(_texture_descriptor, binding);
      _data.flags |= flags;
    }
    else
    {
      _data.flags &= ~flags;
    }
    _update_buffer = true;
  }

  void material::bind(vk::CommandBuffer c, vk::PipelineLayout layout, std::uint32_t binding)
  {
    if (!_texture_descriptor)
    {
      _texture_descriptor =
        gev::engine::get().get_descriptor_allocator().allocate(layouts::defaults().material_set_layout());
      bind_or_unbind(_diffuse, 0, material_info::has_diffuse);
      bind_or_unbind(_roughness, 1, material_info::has_roughness);

      _data_buffer = gev::buffer::host_accessible(sizeof(_data), vk::BufferUsageFlagBits::eStorageBuffer);

      gev::update_descriptor(_texture_descriptor, 2, *_data_buffer, vk::DescriptorType::eStorageBuffer);
      _update_buffer = true;
    }

    if (_update_buffer)
      _data_buffer->load_data<material_info>(_data);

    c.setCullMode(_two_sided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack);
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, binding, _texture_descriptor, nullptr);
  }

  void material::serialize(serializer& base, std::ostream& out)
  {
    base.write_direct_or_reference(out, _diffuse);
    base.write_direct_or_reference(out, _roughness);
    write_typed(_data, out);
    write_typed(_two_sided, out);
  }

  void material::deserialize(serializer& base, std::istream& in) 
  {
    auto const diff = base.read_direct_or_reference(in);
    if (diff)
      load_diffuse(as<texture>(diff));

    auto const rough = base.read_direct_or_reference(in);
    if (rough)
      load_roughness(as<texture>(rough));

    read_typed(_data, in);
    read_typed(_two_sided, in);
    _update_buffer = true;
  }
}    // namespace gev::game