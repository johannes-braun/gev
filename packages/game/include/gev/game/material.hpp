#pragma once

#include <gev/engine.hpp>
#include <gev/game/texture.hpp>
#include <rnu/math/math.hpp>

namespace gev::game
{
  class mesh_renderer;
  class material
  {
  public:
    void load_diffuse(std::shared_ptr<texture> dt);
    void load_roughness(std::shared_ptr<texture> rt);

    void set_diffuse(rnu::vec4 color);
    void set_roughness(float r);

    void set_two_sided(bool enable);
    void bind(vk::CommandBuffer c, vk::PipelineLayout layout, std::uint32_t binding);

  private:
    void bind_or_unbind(std::shared_ptr<texture> const& t, std::uint32_t binding, std::uint32_t flags);

    struct material_info
    {
      constexpr static std::uint32_t has_diffuse = 0x1;
      constexpr static std::uint32_t has_roughness = 0x2;

      rnu::vec4ui8 diffuse = {255, 255, 255, 255};
      std::uint16_t roughness = rnu::to_half(0.2);
      std::uint16_t metadata = 0;
      std::uint32_t flags = 0x0;
    } _data;

    bool _two_sided = false;
    vk::DescriptorSet _texture_descriptor;
    std::shared_ptr<texture> _diffuse;
    std::shared_ptr<texture> _roughness;

    bool _update_buffer = false;
    std::shared_ptr<gev::buffer> _data_buffer;
  };
}    // namespace gev::game