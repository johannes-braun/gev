#include "renderer_component.hpp"

void renderer_component::spawn()
{
  _renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
}

void renderer_component::update()
{
  if (_mesh && _material)
  {
    auto ptr = _renderer.lock();
    if(ptr)
      ptr->enqueue(_mesh, _material, owner()->global_transform().matrix());
  }
}

std::shared_ptr<gev::game::mesh> const& renderer_component::get_mesh() const
{
  return _mesh;
}

std::shared_ptr<gev::game::material> const& renderer_component::get_material() const
{
  return _material;
}

void renderer_component::set_renderer(std::shared_ptr<gev::game::mesh_renderer> value)
{
  _renderer = std::move(value);
}

void renderer_component::set_mesh(std::shared_ptr<gev::game::mesh> value)
{
  _mesh = std::move(value);
}

void renderer_component::set_material(std::shared_ptr<gev::game::material> value)
{
  _material = std::move(value);
}