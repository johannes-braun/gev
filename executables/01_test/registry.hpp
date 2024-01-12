#pragma once

#include <gev/scenery/component.hpp>
#include <string>
#include <unordered_map>

namespace gev::scenery
{
  template<typename T>
  std::shared_ptr<component> create_component()
  {
    return std::make_shared<T>();
  }

  class registry
  {
  public:
    template<typename T>
    void register_component(std::string const& name)
    {
      auto& info = _component_infos[name];
      info.create = &create_component<T>;
    }

    std::shared_ptr<component> create(std::string const& name)
    {
      auto const iter = _component_infos.find(name);
      if (iter != _component_infos.end())
        return iter->second.create();
      return nullptr;
    }

  private:
    struct info
    {
      std::shared_ptr<component> (*create)();
    };

    std::unordered_map<std::string, info> _component_infos;
  };
}    // namespace gev::scenery