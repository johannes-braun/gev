#pragma once

#include <gev/scenery/component.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/game/mesh.hpp>
#include <gev/game/material.hpp>

class renderer_component : public gev::scenery::component
{
public:
  void spawn() override;
  void update() override;

  std::shared_ptr<gev::game::mesh> const& get_mesh() const;
  std::shared_ptr<gev::game::material> const& get_material() const;
  void set_renderer(std::shared_ptr<gev::game::mesh_renderer> value);
  void set_mesh(std::shared_ptr<gev::game::mesh> value);
  void set_material(std::shared_ptr<gev::game::material> value);

private:
  std::weak_ptr<gev::game::mesh_renderer> _renderer;
  std::shared_ptr<gev::game::mesh> _mesh;
  std::shared_ptr<gev::game::material> _material;
};