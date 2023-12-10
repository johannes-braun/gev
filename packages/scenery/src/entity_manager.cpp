#include <gev/scenery/entity_manager.hpp>

namespace gev::scenery
{
  std::shared_ptr<entity> entity_manager::instantiate(std::shared_ptr<entity> parent)
  {
    auto r = std::make_shared<entity>();
    add_to_parent(r, parent);
    return r;
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
      parent->_children.erase(std::remove(begin(_root_entities), end(_root_entities), target));
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
}