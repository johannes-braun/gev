#include <algorithm>
#include <gev/scenery/component.hpp>
#include <gev/scenery/entity.hpp>
#include <print>
#include <gev/scenery/entity_manager.hpp>

namespace gev::scenery
{
  entity::entity(std::size_t id) : _id(id) {}

  std::size_t entity::id() const
  {
    return _id;
  }

  void entity::add(std::shared_ptr<component> c)
  {
    c->_parent = unsafe_shared_from_this<entity>();
    _components.push_back(std::move(c));

    std::sort(_components.begin(), _components.end(),
      [&](std::shared_ptr<component> const& lhs, std::shared_ptr<component> const& rhs)
      { return typeid(*lhs).before(typeid(*rhs)); });
  }

  std::shared_ptr<entity> entity::find_by_id(std::size_t id)
  {
    if (id == _id)
      return unsafe_shared_from_this<entity>();

    for (auto const& c : _children)
    {
      if (auto const res = c->find_by_id(id))
        return res;
    }
    return nullptr;
  }

  void entity::activate(bool propagate_to_children) const
  {
    for (auto& comp : _components)
    {
      if (comp->_active)
        comp->activate();
    }

    if (propagate_to_children)
    {
      for (auto& e : _children)
      {
        if (e->_active)
          e->activate();
      }
    }
  }

  void entity::erase(std::shared_ptr<component> comp)
  {
    auto const found = std::find(begin(_components), end(_components), comp);

    if (found != end(_components))
    {
      found->get()->despawn();
      _components.erase(found);
    }
  }

  void entity::despawn() const
  {
    deactivate();
    for (auto const& c : _components)
      c->despawn();

    for (auto& e : _children)
      e->despawn();
  }

  void entity::deactivate() const
  {
    for (auto& comp : _components)
    {
      if (comp->_active)
        comp->deactivate();
    }

    for (auto& e : _children)
    {
      if (e->_active)
        e->deactivate();
    }
  }

  void entity::set_active(bool active)
  {
    _active = active;
    if (is_inherited_active())
      activate();
    else if (!active)
      deactivate();
  }

  bool entity::is_inherited_active() const
  {
    if (!_active)
      return false;

    if (_parent.expired())
      return _active;

    auto parent = _parent.lock();
    return _active && parent->is_inherited_active();
  }

  bool entity::is_active() const
  {
    return _active;
  }

  void entity::spawn() const
  {
    for (auto const& c : _components)
      c->spawn();
    activate(false);
    for (auto const& c : _children)
      if (c->_active)
        c->spawn();
  }

  void entity::early_update() const
  {
    if (!_active)
      return;

    for (auto const& c : _components)
      if (c->_active)
        c->early_update();

    for (auto const& c : _children)
      c->early_update();
  }

  void entity::update() const
  {
    if (!_active)
      return;

    for (auto const& c : _components)
      if (c->_active)
        c->update();

    for (auto const& c : _children)
      c->update();
  }

  enum class ref_type : std::uint8_t
  {
    direct,
    ref
  };

  void entity::serialize(gev::serializer& base, std::ostream& out)
  {
    write_size(_id, out);
    write_typed(_active, out);
    write_typed(local_transform, out);

    write_size(_components.size(), out);
    for (auto const& c : _components)
    {
      base.write(out, c);
    }

    write_size(_children.size(), out);
    for (auto const& c : _children)
      base.write_direct_or_reference(out, c);
  }

  void entity::deserialize(gev::serializer& base, std::istream& in)
  {
    auto tmp_manager = std::make_shared<entity_manager>();
    
    read_size(_id, in);
    read_typed(_active, in);
    read_typed(local_transform, in);

    std::size_t num = 0;
    read_size(num, in);
    _components.reserve(num);
    for (size_t i = 0; i < num; ++i)
    {
      auto const comp = as<component>(base.read(in));
      comp->_parent = unsafe_shared_from_this<entity>();
      _components.emplace_back(comp);
    }

    num = 0;
    read_size(num, in);
    _children.reserve(num);
    auto const self_ptr = unsafe_shared_from_this<entity>();
    for (size_t i = 0; i < num; ++i)
    {
      auto const child = base.read_direct_or_reference(in);
      tmp_manager->reparent(as<entity>(child), self_ptr);
    }
  }

  void entity::collides(
    std::shared_ptr<entity> other, float distance, rnu::vec3 point_on_self, rnu::vec3 point_on_other) const
  {
    if (!_active)
      return;

    for (auto const& c : _components)
      if (c->_active)
        c->collides(other, distance, point_on_self, point_on_other);

    for (auto const& c : _children)
      c->collides(other, distance, point_on_self, point_on_other);
  }

  void entity::fixed_update(double time, double delta) const
  {
    if (!_active)
      return;

    for (auto const& c : _components)
      if (c->_active)
        c->fixed_update(time, delta);

    for (auto const& c : _children)
      c->fixed_update(time, delta);
  }

  void entity::late_update() const
  {
    if (!_active)
      return;

    for (auto const& c : _components)
      if (c->_active)
        c->late_update();

    for (auto const& c : _children)
      c->late_update();
  }

  std::span<std::shared_ptr<entity> const> entity::children() const
  {
    return _children;
  }

  void entity::apply_transform()
  {
    _global_transform = _global_transform.matrix() * local_transform.matrix();
    for (auto const& c : _children)
    {
      c->_global_transform = _global_transform;
      c->apply_transform();
    }
  }

  transform const& entity::global_transform() const
  {
    return _global_transform;
  }

  std::shared_ptr<entity> entity::parent() const
  {
    return _parent.lock();
  }

  std::shared_ptr<entity_manager> entity::manager() const
  {
    return _manager.lock();
  }
}    // namespace gev::scenery