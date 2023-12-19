#include <algorithm>
#include <gev/scenery/component.hpp>
#include <gev/scenery/entity.hpp>

namespace gev::scenery
{
  entity::entity(std::size_t id) : _id(id) {}

  std::size_t entity::id() const
  {
    return _id;
  }

  void entity::add(std::shared_ptr<component> c)
  {
    c->_parent = shared_from_this();
    _components.push_back(std::move(c));

    std::sort(_components.begin(), _components.end(),
      [&](std::shared_ptr<component> const& lhs, std::shared_ptr<component> const& rhs)
      { return typeid(*lhs).before(typeid(*rhs)); });
  }

  std::shared_ptr<entity> entity::find_by_id(std::size_t id)
  {
    if (id == _id)
      return shared_from_this();

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
}    // namespace gev::scenery