#pragma once

#include <gev/buffer.hpp>
#include <gev/scenery/component.hpp>
#include <gev/scenery/gltf.hpp>
#include <gev/game/shader.hpp>
#include <gev/engine.hpp>

class skin_component : public gev::scenery::component
{
public:
  skin_component() = default;
  skin_component(gev::resource_id shader_id, gev::scenery::skin skin, gev::scenery::transform_tree tree,
    std::unordered_map<std::string, gev::scenery::joint_animation> animations);

  vk::DescriptorSet skin_descriptor() const;
  void early_update() override;

  void set_animation(std::string name);
  std::unordered_map<std::string, gev::scenery::joint_animation> const& animations() const;

  void serialize(gev::serializer& base, std::ostream& out) override;
  void deserialize(gev::serializer& base, std::istream& in) override;

private:
  void apply_child_transform(gev::scenery::entity& e);

  std::string _current_animation = "";
  bool _running = false;
  
  gev::resource_id _shader_id;
  vk::DescriptorSet _joints;
  std::unique_ptr<gev::buffer> _joints_buffer;
  gev::scenery::skin _skin;
  gev::scenery::transform_tree _tree;
  std::unordered_map<std::string, gev::scenery::joint_animation> _animations;
  gev::service_proxy<gev::game::shader_repo> _shader_repo;
};