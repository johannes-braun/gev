#include "renderer_component.hpp"

void renderer_component::update()
{
  if (_mesh_instance)
    _mesh_instance->update_transform(owner()->global_transform().matrix());
}

void renderer_component::deactivate()
{
  update_mesh();
}

void renderer_component::activate()
{
  update_mesh();
}

std::shared_ptr<gev::game::mesh> const& renderer_component::get_mesh() const
{
  return _mesh;
}

std::shared_ptr<gev::game::material> const& renderer_component::get_material() const
{
  return _material;
}

std::shared_ptr<gev::game::mesh_renderer> renderer_component::get_renderer() const
{
  if (_renderer.expired())
    return nullptr;
  return _renderer.lock();
}

void renderer_component::set_renderer(std::shared_ptr<gev::game::mesh_renderer> value)
{
  _renderer = std::move(value);
  update_mesh();
}

void renderer_component::set_mesh(std::shared_ptr<gev::game::mesh> value)
{
  _mesh = std::move(value);
  update_mesh();
}

// void renderer_component::set_df(std::shared_ptr<gev::game::distance_field_instance> value)
//{
//   _df = std::move(value);
// }

void renderer_component::set_shader(std::shared_ptr<gev::game::shader> shader)
{
  _shader = shader;
  update_mesh();
}

std::shared_ptr<gev::game::shader> const& renderer_component::get_shader()
{
  return _shader;
}

void renderer_component::set_material(std::shared_ptr<gev::game::material> value)
{
  _material = std::move(value);
  update_mesh();
}

void renderer_component::update_mesh()
{
  try_destroy();
  if (is_inherited_active())
    try_instantiate();
}

void renderer_component::try_destroy()
{
  if (_mesh_instance)
  {
    _mesh_instance->destroy();
    _mesh_instance = nullptr;
  }
}

void renderer_component::try_instantiate()
{
  if (_mesh && _shader && _material && !_renderer.expired())
  {
    auto r = _renderer.lock();
    _mesh_instance = r->batch(_shader, _material)->instantiate(_mesh, owner()->global_transform());
  }
}