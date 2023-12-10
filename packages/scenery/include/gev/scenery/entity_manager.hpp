#pragma once

#include <gev/scenery/entity.hpp>
#include <span>

namespace gev::scenery
{
  class entity_manager
  {
  public:
    std::shared_ptr<entity> instantiate(std::shared_ptr<entity> parent = nullptr);
    void reparent(std::shared_ptr<entity>const& target, std::shared_ptr<entity> new_parent = nullptr);

    std::span<std::shared_ptr<entity> const> root_entities() const
    {
      return _root_entities;
    }

  private:
    void remove_from_parent(std::shared_ptr<entity>const& target);
    void add_to_parent(std::shared_ptr<entity>const& target, std::shared_ptr<entity> parent = nullptr);

    std::vector<std::shared_ptr<entity>> _root_entities;
  };
}