#pragma once

#include <memory>

namespace gev
{
  struct virtual_enable_shared_from_this : std::enable_shared_from_this<virtual_enable_shared_from_this>
  {
    virtual ~virtual_enable_shared_from_this() {}

    template<typename T>
    std::shared_ptr<T> unsafe_shared_from_this()
    {
      return std::static_pointer_cast<T>(shared_from_this());
    }
  };
}