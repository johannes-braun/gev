#include "skin_component.hpp"

#include "bone_component.hpp"

#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/layouts.hpp>

skin_component::skin_component(gev::scenery::skin skin, gev::scenery::transform_tree tree,
  std::unordered_map<std::string, gev::scenery::joint_animation> animations)
  : _skin(std::move(skin)), _tree(std::move(tree)), _animations(std::move(animations))
{
  auto const& layouts = gev::game::layouts::defaults();
  _joints = gev::engine::get().get_descriptor_allocator().allocate(layouts.skinning_set_layout());
}

vk::DescriptorSet skin_component::skin_descriptor() const
{
  return _joints;
}

void skin_component::early_update()
{
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
    bone->owner()->local_transform = _tree.nodes()[_skin.joint_node(bone->index())].matrix();
  }

  for (auto const& c : e.children())
    apply_child_transform(*c);
}