#pragma once

#include <gev/game/material.hpp>
#include <gev/game/mesh.hpp>
#include <gev/game/mesh_batch.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/scenery/component.hpp>

class renderer_component : public gev::scenery::component
{
public:
  void spawn() override;
  void early_update() override;
  void activate() override;
  void deactivate() override;

  std::shared_ptr<gev::game::mesh> const& get_mesh() const;
  std::shared_ptr<gev::game::material> const& get_material() const;
  void set_mesh(std::shared_ptr<gev::game::mesh> value);
  void set_material(std::shared_ptr<gev::game::material> value);

  void set_shader(gev::resource_id shader);
  gev::resource_id get_shader();

  void serialize(gev::serializer& base, std::ostream& out) override;
  void deserialize(gev::serializer& base, std::istream& in) override;

private:
  void update_mesh();
  void try_instantiate();
  void try_destroy();

  gev::service_proxy<gev::game::mesh_renderer> _renderer;
  std::shared_ptr<gev::game::mesh_instance> _mesh_instance;
  std::shared_ptr<gev::game::mesh> _mesh;
  std::shared_ptr<gev::game::material> _material;
  gev::resource_id _shader_id;
  std::shared_ptr<gev::game::shader> _shader;
};