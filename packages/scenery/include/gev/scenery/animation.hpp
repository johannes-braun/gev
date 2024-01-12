#pragma once

#include <any>
#include <chrono>
#include <optional>
#include <rnu/algorithm/smooth.hpp>
#include <rnu/math/math.hpp>
#include <span>
#include <variant>
#include <vector>
#include <gev/res/serializer.hpp>
#include <gev/scenery/transform.hpp>

namespace gev::scenery
{
  template<typename T>
  struct interpolate_value
  {
    T operator()(T const& prev, T const& next, float t) const
    {
      return prev + (next - prev) * t;
    }
  };

  template<typename T>
  struct interpolate_value<rnu::quat_t<T>>
  {
    using quat_type = rnu::quat_t<T>;

    quat_type operator()(quat_type const& prev, quat_type const& next, float t) const
    {
      return rnu::slerp(prev, next, t);
    }
  };

  template<typename T>
  auto const interp(T a, T b, float t)
  {
    return interpolate_value<T>{}(a, b, t);
  }

  struct transform_node
  {
    std::string name;
    std::size_t mesh_reference;
    ptrdiff_t children_offset;
    size_t num_children;
    ptrdiff_t parent;
    transform transformation;
  };

  enum class animation_target : std::uint8_t
  {
    location,
    rotation,
    scale
  };

  class animation : public gev::serializable
  {
  public:
    animation() = default;
    animation(animation_target target, size_t node_index, std::vector<rnu::vec3> vec3_checkpoints,
      std::vector<rnu::quat> quat_checkpoints, std::vector<float> timestamps);
    
    float duration() const;

    void transform(float time, float mix_factor, std::vector<transform_node>& nodes) const;

    void serialize(serializer& base, std::ostream& out) override;
    void deserialize(serializer& base, std::istream& in) override;

  private:
    size_t _node_index;
    animation_target _target;
    std::vector<rnu::vec3> _vec3_checkpoints;
    std::vector<rnu::quat> _quat_checkpoints;
    std::vector<float> _timestamps;
  };

  class joint_animation : public gev::serializable
  {
  public:
    struct anim_info
    {
      std::vector<animation> animation;
      bool one_shot;
    };

    void set(std::vector<animation> anim, bool one_shot = true);

    void start(double offset = 0.0f);

    void update(std::chrono::duration<double> d, std::vector<transform_node>& nodes);

    void serialize(serializer& base, std::ostream& out) override;
    void deserialize(serializer& base, std::istream& in) override;

  private:
    anim_info _anim;
    size_t _current;
    double _time = 0;
    float _longest_duration = 0.0f;
    rnu::smooth<double> _ramp_up;
  };

  class transform_tree : public serializable
  {
  public:
    transform_tree() = default;
    transform_tree(std::vector<transform_node> nodes);

    void animate(joint_animation& animation, std::chrono::duration<double> delta);
    rnu::mat4 const& global_transform(size_t node) const;
    std::span<transform_node const> nodes() const;
    void serialize(serializer& base, std::ostream& out) override;
    void deserialize(serializer& base, std::istream& in) override;

  private:
    void recompute_globals();

    std::vector<transform_node> _nodes;
    std::vector<rnu::mat4> _global_matrices;
  };

  class skin : public serializable
  {
  public:
    skin() = default;
    skin(size_t root, std::vector<std::uint32_t> joint_nodes, std::vector<rnu::mat4> joint_matrices);

    std::uint32_t joint_node(size_t index) const;
    std::optional<std::uint32_t> find_joint_index(size_t node_index) const;
    rnu::mat4 const& joint_matrix(size_t index) const;
    size_t size() const;

    std::uint32_t root_node() const;

    std::vector<rnu::mat4>& apply_global_transforms(transform_tree const& tree);
    
    void serialize(serializer& base, std::ostream& out) override;
    void deserialize(serializer& base, std::istream& in) override;

  private:
    size_t _root_node;
    std::vector<std::uint32_t> _joint_nodes;
    std::vector<rnu::mat4> _joint_matrices;
    std::vector<rnu::mat4> _global_joint_matrices;
  };
}    // namespace gev::scenery