#include <gev/scenery/animation.hpp>

namespace gev::scenery
{
  animation::animation(animation_target target, size_t node_index, std::vector<rnu::vec3> vec3_checkpoints,
    std::vector<rnu::quat> quat_checkpoints, std::vector<float> timestamps)
    : _node_index(node_index),
      _target(target),
      _vec3_checkpoints(std::move(vec3_checkpoints)),
      _quat_checkpoints(std::move(quat_checkpoints)),
      _timestamps(std::move(timestamps))
  {
  }
  float animation::duration() const
  {
    return _timestamps.empty() ? 0 : _timestamps.back();
  }
  void animation::transform(float time, float mix_factor, std::vector<transform_node>& nodes) const
  {
    if (_timestamps.empty())
      return;

    auto const clamped_time = std::clamp(time, _timestamps.front(), _timestamps.back());

    auto const ts_iter = std::prev(std::upper_bound(begin(_timestamps), end(_timestamps), clamped_time));
    auto const current_ts_index = std::distance(begin(_timestamps), ts_iter);
    auto const next_ts_index = (current_ts_index + 1) % _timestamps.size();

    auto const current_ts = _timestamps[current_ts_index];
    auto const next_ts = _timestamps[next_ts_index];
    auto const i = (next_ts_index == current_ts_index) ? 0.0 : (time - current_ts) / (next_ts - current_ts);

    auto& n = nodes[_node_index];

    switch (_target)
    {
      case animation_target::location:
      {
        auto const& locations = _vec3_checkpoints;
        auto const calc = interp(locations[current_ts_index], locations[next_ts_index], i);
        auto const attenuated = interp(n.transformation.position, calc, mix_factor);

        n.transformation.position = attenuated;
        break;
      }
      case animation_target::rotation:
      {
        auto const& rotations = _quat_checkpoints;
        auto const rot = interp(rotations[current_ts_index], rotations[next_ts_index], i);
        auto const attenuated = interp(n.transformation.rotation, rot, mix_factor);

        n.transformation.rotation = attenuated;
        break;
      }
      case animation_target::scale:
      {
        auto const& scales = _vec3_checkpoints;
        auto const sca = interp(scales[current_ts_index], scales[next_ts_index], i);
        auto const attenuated = interp(n.transformation.scale, sca, mix_factor);

        n.transformation.scale = attenuated;
        break;
      }
    }
  }

  void animation::serialize(serializer& base, std::ostream& out) 
  {
    write_typed(_node_index, out);
    write_typed(_target, out);
    write_vector(_vec3_checkpoints, out);
    write_vector(_quat_checkpoints, out);
    write_vector(_timestamps, out);
  }

  void animation::deserialize(serializer& base, std::istream& in)
  {
    read_typed(_node_index, in);
    read_typed(_target, in);
    read_vector(_vec3_checkpoints, in);
    read_vector(_quat_checkpoints, in);
    read_vector(_timestamps, in);
  }

  skin::skin(size_t root, std::vector<std::uint32_t> joint_nodes, std::vector<rnu::mat4> joint_matrices)
    : _root_node(root), _joint_nodes(std::move(joint_nodes)), _joint_matrices(std::move(joint_matrices))
  {
  }
  std::uint32_t skin::joint_node(size_t index) const
  {
    return _joint_nodes[index];
  }
  std::optional<std::uint32_t> skin::find_joint_index(size_t node_index) const
  {
    auto const iter = std::find(_joint_nodes.begin(), _joint_nodes.end(), node_index);
    if (_joint_nodes.end() == iter)
      return std::nullopt;
    return std::uint32_t(std::distance(_joint_nodes.begin(), iter));
  }
  rnu::mat4 const& skin::joint_matrix(size_t index) const
  {
    return _joint_matrices[index];
  }
  size_t skin::size() const
  {
    return _joint_nodes.size();
  }
  std::uint32_t skin::root_node() const
  {
    return _root_node;
  }
  std::vector<rnu::mat4>& skin::apply_global_transforms(transform_tree const& tree)
  {
    _global_joint_matrices.resize(size(), rnu::mat4(1.0f));

    for (int i = 0; i < size(); ++i)
    {
      auto const& g = tree.global_transform(joint_node(i));
      _global_joint_matrices[i] = g * joint_matrix(i);
    }
    return _global_joint_matrices;
  }

  void skin::serialize(serializer& base, std::ostream& out)
  {
    write_size(_root_node, out);
    write_vector(_joint_nodes, out);
    write_vector(_joint_matrices, out);
    write_vector(_global_joint_matrices, out);
  }

  void skin::deserialize(serializer& base, std::istream& in)
  {
    read_size(_root_node, in);
    read_vector(_joint_nodes, in);
    read_vector(_joint_matrices, in);
    read_vector(_global_joint_matrices, in);
  }

  transform_tree::transform_tree(std::vector<transform_node> nodes)
    : _nodes(std::move(nodes)), _global_matrices(_nodes.size(), rnu::mat4(1.0f))
  {
    recompute_globals();
  }

  void transform_tree::animate(joint_animation& animation, std::chrono::duration<double> delta)
  {
    animation.update(delta, _nodes);
    recompute_globals();
  }

  std::span<transform_node const> transform_tree::nodes() const
  {
    return _nodes;
  }

  rnu::mat4 const& transform_tree::global_transform(size_t node) const
  {
    return _global_matrices[node];
  }

  void transform_tree::recompute_globals()
  {
    for (int i = 0; i < _nodes.size(); ++i)
    {
      _global_matrices[i] = _nodes[i].transformation.matrix();
    }

    for (int i = 0; i < _nodes.size(); ++i)
    {
      if (_nodes[i].parent != -1)
      {
        _global_matrices[i] = _global_matrices[_nodes[i].parent] * _global_matrices[i];
      }
    }
  }

  void transform_tree::serialize(serializer& base, std::ostream& out)
  {
    write_size(_nodes.size(), out);
    for (auto const& node : _nodes)
    {
      write_string(node.name, out);
      write_size(node.mesh_reference, out);
      write_typed(node.children_offset, out);
      write_size(node.num_children, out);
      write_typed(node.parent, out);
      write_typed(node.transformation, out);
    }
    write_vector(_global_matrices, out);
  }

  void transform_tree::deserialize(serializer& base, std::istream& in)
  {
    std::size_t count = 0ull;
    read_size(count, in);
    _nodes.resize(count);
    for (auto& node : _nodes)
    {
      read_string(node.name, in);
      read_size(node.mesh_reference, in);
      read_typed(node.children_offset, in);
      read_size(node.num_children, in);
      read_typed(node.parent, in);
      read_typed(node.transformation, in);
    }
    read_vector(_global_matrices, in);
  }

  void joint_animation::set(std::vector<animation> anim, bool one_shot)
  {
    for (auto const& a : anim)
      _longest_duration = std::max(_longest_duration, a.duration());
    _anim.animation = std::move(anim);
    _anim.one_shot = one_shot;
  }

  void joint_animation::start(double offset)
  {
    _time = offset;
    _ramp_up = 0;
    _ramp_up.to(1.0);
    _current = 0;
  }

  void joint_animation::update(std::chrono::duration<double> d, std::vector<transform_node>& nodes)
  {
    _time += d.count();
    _ramp_up.update(d.count());

    bool animation_finished = false;
    if (_longest_duration <= _time)
      animation_finished = true;

    if (animation_finished)
    {
      if (!_anim.one_shot)
        _time = std::fmodf(_time, _longest_duration);
    }

    for (auto& a : _anim.animation)
      a.transform(_time, float(_ramp_up.value()), nodes);
  }

  void joint_animation::serialize(serializer& base, std::ostream& out) 
  {
    write_typed(_anim.one_shot, out);
    write_typed(_longest_duration, out);
    write_size(_anim.animation.size(), out);
    for (auto& anim : _anim.animation)
    {
      anim.serialize(base, out);
    }
  }

  void joint_animation::deserialize(serializer& base, std::istream& in) 
  {
    read_typed(_anim.one_shot, in);
    read_typed(_longest_duration, in);
    std::size_t count = 0ull;
    read_typed(count, in);
    _anim.animation.resize(count);
    for (auto& anim : _anim.animation)
    {
      anim.deserialize(base, in);
    }
    _time = 0.0;
    _ramp_up = 0;
    _ramp_up.to(1.0);
    _current = 0;
  }
}    // namespace gev::scenery