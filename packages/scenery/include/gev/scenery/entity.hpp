#pragma once

#include <gev/scenery/transform.hpp>
#include <memory>
#include <span>
#include <vector>

namespace gev::scenery
{
  class component;
  class entity_manager;

  class entity : public std::enable_shared_from_this<entity>
  {
    friend class entity_manager;

  public:
    transform local_transform;

    entity(std::size_t id);

    void add(std::shared_ptr<component> c);

    template<typename T>
    std::shared_ptr<T> get()
    {
      auto const iter = std::find_if(begin(_components), end(_components),
        [&](std::shared_ptr<component> const& c) { return typeid(T) == typeid(*c); });
      if (iter == end(_components))
        return nullptr;
      return std::static_pointer_cast<T>(*iter);
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> emplace(Args&&... args)
    {
      auto component = std::make_shared<T>(std::forward<Args>(args)...);
      add(component);
      return component;
    }

    std::shared_ptr<entity> find_by_id(std::size_t id);

    template<typename Predicate>
    std::shared_ptr<entity> find_where(Predicate&& pred)
    {
      auto self = shared_from_this();
      if (pred(self))
        return self;

      for (auto const& c : _children)
        if (auto const e = c->find_where<Predicate>(std::forward<Predicate>(pred)))
          return e;

      return nullptr;
    }

    template<typename Component, typename Predicate>
    std::shared_ptr<Component> find_by_component(Predicate&& pred)
    {
      auto const cm = get<Component>();
      if (cm && pred(cm))
        return cm;

      for (auto const& c : _children)
        if (auto const comp = c->find_by_component<Component, Predicate>(std::forward<Predicate>(pred)))
          return comp;
      return nullptr;
    }

    void erase(std::shared_ptr<component> comp);

    std::size_t id() const;
    void spawn() const;
    void despawn() const;
    void early_update() const;
    void update() const;
    void fixed_update(double time, double delta) const;
    void late_update() const;
    void apply_transform();
    void set_active(bool active);
    bool is_inherited_active() const;
    bool is_active() const;
    void collides(std::shared_ptr<entity> other, float distance, rnu::vec3 point_on_self, rnu::vec3 point_on_other) const;

    std::span<std::shared_ptr<entity> const> children() const;

    transform const& global_transform() const;

    std::shared_ptr<entity> parent() const;

    std::shared_ptr<entity_manager> manager() const;

  private:
    void activate(bool propagate_to_children = true) const;
    void deactivate() const;

    std::size_t _id;
    std::weak_ptr<entity> _parent;
    bool _active = true;

    transform _global_transform;
    std::vector<std::shared_ptr<component>> _components;
    std::vector<std::shared_ptr<entity>> _children;
    std::weak_ptr<entity_manager> _manager;
  };
}    // namespace gev::scenery