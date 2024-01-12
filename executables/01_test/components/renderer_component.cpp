#include "renderer_component.hpp"

void renderer_component::spawn()
{
  if (!_shader)
    set_shader(gev::game::shaders::standard);
}

void renderer_component::early_update()
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

void renderer_component::set_mesh(std::shared_ptr<gev::game::mesh> value)
{
  _mesh = std::move(value);
  update_mesh();
}

void renderer_component::set_shader(gev::resource_id shader)
{
  _shader_id = shader;
  _shader = gev::service<gev::game::shader_repo>()->get(shader);
  update_mesh();
}

gev::resource_id renderer_component::get_shader()
{
  return _shader_id;
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

void renderer_component::serialize(gev::serializer& base, std::ostream& out)
{
  gev::scenery::component::serialize(base, out);

  base.write_direct_or_reference(out, _material);
  base.write_direct_or_reference(out, _mesh);

  write_typed(_shader_id, out);
}

void renderer_component::deserialize(gev::serializer& base, std::istream& in)
{
  gev::scenery::component::deserialize(base, in);

  auto const material = base.read_direct_or_reference(in);
  if (material)
    set_material(as<gev::game::material>(material));

  auto const mesh = base.read_direct_or_reference(in);
  if (mesh)
    set_mesh(as<gev::game::mesh>(mesh));

  read_typed(_shader_id, in);
  _shader = gev::service<gev::game::shader_repo>()->get(_shader_id);
}

void renderer_component::try_instantiate()
{
  if (_mesh && _shader && _material)
    _mesh_instance = _renderer->batch(_shader, _material)->instantiate(_mesh, owner()->global_transform());
}