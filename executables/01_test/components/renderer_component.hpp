#pragma once

#include <gev/game/material.hpp>
#include <gev/game/mesh.hpp>
#include <gev/game/mesh_batch.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/scenery/component.hpp>

class renderer_component : public gev::scenery::component
{
public:
  void update() override;
  void activate() override;
  void deactivate() override;

  std::shared_ptr<gev::game::mesh> const& get_mesh() const;
  std::shared_ptr<gev::game::material> const& get_material() const;
  std::shared_ptr<gev::game::mesh_renderer> get_renderer() const;
  void set_renderer(std::shared_ptr<gev::game::mesh_renderer> value);
  void set_mesh(std::shared_ptr<gev::game::mesh> value);
  void set_material(std::shared_ptr<gev::game::material> value);

  void set_shader(std::shared_ptr<gev::game::shader> shader);
  std::shared_ptr<gev::game::shader> const& get_shader();

private:
  void update_mesh();
  void try_instantiate();
  void try_destroy();

  std::weak_ptr<gev::game::mesh_renderer> _renderer;
  std::shared_ptr<gev::game::mesh_instance> _mesh_instance;
  std::shared_ptr<gev::game::mesh> _mesh;
  std::shared_ptr<gev::game::material> _material;
  std::shared_ptr<gev::game::shader> _shader;
};