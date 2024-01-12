#pragma once

#include <gev/scenery/entity.hpp>
#include <gev/res/serializer.hpp>
#include <memory>

namespace gev::scenery
{
  class component : public std::enable_shared_from_this<component>, public serializable
  {
    friend class entity;

  public:
    virtual ~component() = default;

    virtual void spawn() {}
    virtual void despawn() {}
    virtual void early_update() {}
    virtual void fixed_update(double time, double delta) {}
    virtual void update() {}
    virtual void late_update() {}
    virtual void activate() {}
    virtual void deactivate() {}
    virtual void collides(
      std::shared_ptr<entity> other, float distance, rnu::vec3 point_on_self, rnu::vec3 point_on_other){}

    virtual void serialize(serializer& base, std::ostream& out) override
    {
      write_typed(_active, out);
    }
    virtual void deserialize(serializer& base, std::istream& in) override
    {
      read_typed(_active, in);
    }

  public:
    std::shared_ptr<entity> owner() const;
    void set_active(bool active);
    bool is_inherited_active() const;
    bool is_active() const;

  private:
    std::weak_ptr<entity> _parent;
    bool _active = true;
  };
}    // namespace gev::scenery