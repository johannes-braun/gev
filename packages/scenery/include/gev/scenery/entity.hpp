#pragma once

#include <vector>
#include <memory>
#include <span>
#include <gev/scenery/transform.hpp>

namespace gev::scenery
{
  class component;

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
      auto const iter = std::find_if(begin(_components), end(_components), [&](std::shared_ptr<component> const& c) {
        return typeid(T) == typeid(*c);
        });
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

    std::size_t id() const;
    void spawn() const;
    void early_update() const;
    void update() const;
    void late_update() const;
    void apply_transform();
    std::span<std::shared_ptr<entity> const> children() const;

    transform const& global_transform() const;

  private:
    std::size_t _id;
    std::weak_ptr<entity> _parent;

    transform _global_transform;
    std::vector<std::shared_ptr<component>> _components;
    std::vector<std::shared_ptr<entity>> _children;
  };
}