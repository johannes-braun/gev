#include <gev/scenery/entity.hpp>
#include <gev/scenery/component.hpp>
#include <algorithm>

namespace gev::scenery
{
  void entity::add(std::shared_ptr<component> c)
  {
    c->_parent = shared_from_this();
    _components.push_back(std::move(c));

    std::sort(_components.begin(), _components.end(), [&](std::shared_ptr<component> const& lhs, std::shared_ptr<component> const& rhs) {
      return typeid(*lhs).before(typeid(*rhs));
      });
  }

  void entity::spawn() const
  {
    for (auto const& c : _components)
      c->spawn();
  }

  void entity::early_update() const
  {
    for (auto const& c : _components)
      c->early_update();
  }

  void entity::update() const
  {
    for (auto const& c : _components)
      c->update();
  }

  void entity::late_update() const
  {
    for (auto const& c : _components)
      c->late_update();
  }

  std::span<std::shared_ptr<entity> const> entity::children() const
  {
    return _children;
  }
}