#include "skin_component.hpp"

#include "bone_component.hpp"

#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/mesh_renderer.hpp>

skin_component::skin_component(gev::resource_id shader_id, gev::scenery::skin skin, gev::scenery::transform_tree tree,
  std::unordered_map<std::string, gev::scenery::joint_animation> animations)
  : _shader_id(shader_id), _skin(std::move(skin)), _tree(std::move(tree)), _animations(std::move(animations))
{
}

vk::DescriptorSet skin_component::skin_descriptor() const
{
  return _joints;
}

void skin_component::early_update()
{
  if (!_joints)
  {
    auto const& layouts = gev::game::layouts::defaults();
    _joints = gev::engine::get().get_descriptor_allocator().allocate(layouts.skinning_set_layout());
  }

  _shader_repo->get(_shader_id)->attach_always(_joints, gev::game::mesh_renderer::skin_set);
  auto const iter = _animations.find(_current_animation);
  if (iter != _animations.end())
  {
    if (!_running)
    {
      _running = true;
      iter->second.start();
    }
    _tree.animate(iter->second, std::chrono::duration<double>(gev::engine::get().current_frame().delta_time));

    if (auto const p = owner()->parent())
      apply_child_transform(*p);
    else
      apply_child_transform(*owner());
  }

  auto const& mats = _skin.apply_global_transforms(_tree);

  if (!_joints_buffer)
  {
    _joints_buffer = gev::buffer::host_local(mats.size() * sizeof(rnu::mat4), vk::BufferUsageFlagBits::eStorageBuffer);
    gev::update_descriptor(_joints, 0, *_joints_buffer, vk::DescriptorType::eStorageBuffer);
  }
  _joints_buffer->load_data<rnu::mat4>(mats);
}

void skin_component::set_animation(std::string name)
{
  if (_current_animation != name)
  {
    _current_animation = std::move(name);
    _running = false;
  }
}

std::unordered_map<std::string, gev::scenery::joint_animation> const& skin_component::animations() const
{
  return _animations;
}

void skin_component::apply_child_transform(gev::scenery::entity& e)
{
  if (auto const bone = e.get<bone_component>())
  {
    bone->owner()->local_transform = _tree.nodes()[_skin.joint_node(bone->index())].transformation.matrix();
  }

  for (auto const& c : e.children())
    apply_child_transform(*c);
}

void skin_component::serialize(gev::serializer& base, std::ostream& out)
{
  gev::scenery::component::serialize(base, out);

  write_typed(_shader_id, out);
  write_string(_current_animation, out);
  _skin.serialize(base, out);
  _tree.serialize(base, out);

  write_size(_animations.size(), out);
  for (auto& [name, anim] : _animations)
  {
    write_string(name, out);
    anim.serialize(base, out);
  }
}

void skin_component::deserialize(gev::serializer& base, std::istream& in)
{
  gev::scenery::component::deserialize(base, in);

  read_typed(_shader_id, in);
  read_string(_current_animation, in);
  _skin.deserialize(base, in);
  _tree.deserialize(base, in);

  std::size_t count = 0ull;
  read_size(count, in);
  for (std::size_t i = 0; i < count; ++i)
  {
    std::string name;
    read_string(name, in);
    _animations[name].deserialize(base, in);
  }
  _running = false;
}