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

  _joint_transforms.resize(_skin.size());
  if (!_joints_buffer)
  {
    _joints_buffer =
      gev::buffer::host_local(_joint_transforms.size() * sizeof(rnu::mat4), vk::BufferUsageFlagBits::eStorageBuffer);
    gev::update_descriptor(_joints, 0, *_joints_buffer, vk::DescriptorType::eStorageBuffer);
  }
  _joints_buffer->load_data<rnu::mat4>(_joint_transforms);
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

    apply_child_transform(*owner());
    owner()->apply_transform();
  }
  collect_child_transform(*owner());
  _joints_buffer->load_data<rnu::mat4>(_joint_transforms);
}

void skin_component::update()
{
  if (ImGui::Begin("ANIMATOR"))
  {
    for (auto const& [name, _] : _animations)
    {
      if (ImGui::Button(name.c_str()))
      {
        _current_animation = name;
        _running = false;
      }
    }
    if (ImGui::Button("None"))
    {
      _current_animation = "";
      _running = false;
    }
  }
  ImGui::End();
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

void skin_component::collect_child_transform(gev::scenery::entity& e)
{
  if (auto const bone = e.get<bone_component>())
  {
    _joint_transforms[bone->index()] = bone->owner()->global_transform().matrix() * _skin.joint_matrix(bone->index());
  }

  for (auto const& c : e.children())
    collect_child_transform(*c);
}