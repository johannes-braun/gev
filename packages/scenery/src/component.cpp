#include <gev/scenery/component.hpp>

namespace gev::scenery
{
  std::shared_ptr<entity> component::owner() const
  {
    return _parent.lock();
  }

  void component::set_active(bool active)
  {
    _active = active;
    if (is_inherited_active())
      activate();
    else if (!active)
      deactivate();
  }

  bool component::is_inherited_active() const
  {
    auto parent = owner();
    return _active && (!parent || parent->is_inherited_active());
  }

  bool component::is_active() const
  {
    return _active;
  }
}    // namespace gev::scenery