#pragma once

#include <gev/scenery/entity.hpp>
#include <memory>

namespace gev::scenery
{
  class component : public std::enable_shared_from_this<component>
  {
    friend class entity;
  public:
    virtual void spawn() {}
    virtual void early_update() {}
    virtual void update() {}
    virtual void late_update() {}

  public:
    std::shared_ptr<entity> owner() const;

  private:
    std::weak_ptr<entity> _parent;
  };
}