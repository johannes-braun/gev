#pragma once

#include <gev/buffer.hpp>
#include <gev/scenery/component.hpp>
#include <gev/scenery/gltf.hpp>

class skin_component : public gev::scenery::component
{
public:
  skin_component(gev::scenery::skin skin, gev::scenery::transform_tree tree,
    std::unordered_map<std::string, gev::scenery::joint_animation> animations);

  vk::DescriptorSet skin_descriptor() const;
  void early_update() override;

  void set_animation(std::string name);
  std::unordered_map<std::string, gev::scenery::joint_animation> const& animations() const;

private:
  void apply_child_transform(gev::scenery::entity& e);

  std::string _current_animation = "";
  bool _running = false;

  vk::DescriptorSet _joints;
  std::unique_ptr<gev::buffer> _joints_buffer;
  gev::scenery::skin _skin;
  gev::scenery::transform_tree _tree;
  std::unordered_map<std::string, gev::scenery::joint_animation> _animations;
};