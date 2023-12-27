#pragma once

#include <string_view>
#include <memory>
#include <unordered_map>

namespace gev
{
  class resource_id
  {
  public:
    template<std::size_t S>
    constexpr resource_id(char const (&str)[S]) : resource_id(std::string_view(str))
    {
    }

    constexpr resource_id(std::string_view str) : _id(5381)
    {
      for (auto c : str)
        _id = std::size_t(c) + 33 * _id;
    }

    constexpr resource_id(std::size_t id) : _id(id) {}

    constexpr std::size_t get() const
    {
      return _id;
    }

  private:
    std::size_t _id;
  };

  template<typename TBase>
  class repo
  {
  public:
    void emplace(resource_id id, std::shared_ptr<TBase> s);
    void erase(resource_id id);
    std::shared_ptr<TBase> get(resource_id id) const;

  protected:
    std::unordered_map<std::size_t, std::shared_ptr<TBase>> _resources;
  };

  template<typename TBase>
  void repo<TBase>::emplace(resource_id id, std::shared_ptr<TBase> s)
  {
    _resources.emplace(id.get(), std::move(s));
  }

  template<typename TBase>
  void repo<TBase>::erase(resource_id id)
  {
    _resources.erase(id.get());
  }

  template<typename TBase>
  std::shared_ptr<TBase> repo<TBase>::get(resource_id id) const
  {
    auto const iter = _resources.find(id.get());
    if (iter == _resources.end())
      return nullptr;
    return iter->second;
  }
}