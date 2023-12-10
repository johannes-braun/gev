#include <gev/scenery/component.hpp>

namespace gev::scenery
{
  std::shared_ptr<entity> const& component::owner() const
  {
    return _parent;
  }
}