#pragma once

#include <gev/engine.hpp>
#include <gev/game/texture.hpp>

namespace gev::game
{
  class mesh_renderer;
  class material
  {
  public:
    void load_diffuse(std::shared_ptr<texture> dt);
    void load_roughness(std::shared_ptr<texture> rt);
    void set_two_sided(bool enable);
    void bind(frame const& f, mesh_renderer& r);

  private:
    bool _two_sided = false;
    vk::DescriptorSet _texture_descriptor;
    std::shared_ptr<texture> _diffuse;
    std::shared_ptr<texture> _roughness;
  };
}