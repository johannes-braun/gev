#include <gev/scenery/entity_manager.hpp>

namespace gev::scenery
{
  std::shared_ptr<entity> entity_manager::instantiate(std::shared_ptr<entity> parent)
  {
    return instantiate(~0, std::move(parent));
  }

  std::shared_ptr<entity> entity_manager::instantiate(std::size_t id, std::shared_ptr<entity> parent)
  {
    auto r = std::make_shared<entity>(id);
    add_to_parent(r, parent);
    return r;
  }

  std::shared_ptr<entity> entity_manager::find_by_id(std::size_t id)
  {
    if (id == ~0)
      return nullptr;

    for (auto const& c : _root_entities)
    {
      auto const found = c->find_by_id(id);
      if (found)
        return found;
    }
    return nullptr;
  }

  void entity_manager::reparent(std::shared_ptr<entity> const& target, std::shared_ptr<entity> new_parent)
  {
    remove_from_parent(target);
    add_to_parent(target, new_parent);
  }

  void entity_manager::remove_from_parent(std::shared_ptr<entity>const& target)
  {
    if (target->_parent.expired())
    {
      // this was a root node. remove from list
      _root_entities.erase(std::remove(begin(_root_entities), end(_root_entities), target));
    }
    else
    {
      auto const parent = target->_parent.lock();
      // this was not a root node. remove from parent's children list
      parent->_children.erase(std::remove(begin(parent->_children), end(parent->_children), target));
    }
    target->_parent.reset();
  }

  void entity_manager::add_to_parent(std::shared_ptr<entity>const& target, std::shared_ptr<entity> parent)
  {
    if (!parent)
      _root_entities.emplace_back(target);
    else
    {
      auto&& r = parent->_children.emplace_back(target);
      r->_parent = parent;
    }
  }

  void entity_manager::apply_transform() const
  {
    for (auto const& c : _root_entities)
    {
      c->_global_transform = {};
      c->apply_transform();
    }
  }

  void entity_manager::spawn() const
  {
    for (auto const& c : _root_entities)
      c->spawn();
  }

  void entity_manager::early_update() const
  {
    for (auto const& c : _root_entities)
      c->early_update();
  }

  void entity_manager::update() const
  {
    for (auto const& c : _root_entities)
      c->update();
  }

  void entity_manager::late_update() const
  {
    for (auto const& c : _root_entities)
      c->late_update();
  }
}