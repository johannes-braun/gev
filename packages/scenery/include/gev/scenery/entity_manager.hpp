#pragma once

#include <gev/scenery/entity.hpp>
#include <span>

namespace gev::scenery
{
  class entity_manager : public std::enable_shared_from_this<entity_manager>
  {
  public:
    std::shared_ptr<entity> instantiate(std::shared_ptr<entity> parent = nullptr);
    void add(std::shared_ptr<entity> existing, std::shared_ptr<entity> parent = nullptr);
    std::shared_ptr<entity> instantiate(std::size_t id, std::shared_ptr<entity> parent = nullptr);
    void reparent(std::shared_ptr<entity> const& target, std::shared_ptr<entity> new_parent = nullptr);

    std::span<std::shared_ptr<entity> const> root_entities() const
    {
      return _root_entities;
    }

    void destroy(std::shared_ptr<entity> e);

    void spawn() const;
    void despawn();
    void early_update() const;
    void fixed_update(double time, double delta) const;
    void update() const;
    void late_update() const;
    void apply_transform() const;
    std::shared_ptr<entity> find_by_id(std::size_t id);

  private:
    void assign_manager_impl(std::shared_ptr<entity> existing);

    void remove_from_parent(std::shared_ptr<entity> const& target);
    void add_to_parent(std::shared_ptr<entity> const& target, std::shared_ptr<entity> parent = nullptr);

    std::vector<std::shared_ptr<entity>> _root_entities;
  };
}    // namespace gev::scenery