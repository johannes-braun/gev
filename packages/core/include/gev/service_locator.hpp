#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

namespace gev
{
  class service_locator
  {
  public:
    template<typename T, typename... Args>
    std::shared_ptr<T> register_service(Args&&... args)
    {
      auto result = std::make_shared<T>(std::forward<Args>(args)...);
      _services[typeid(T).hash_code()] = std::static_pointer_cast<void>(result);
      return result;
    }

    template<typename I, typename T, typename... Args>
    std::shared_ptr<I> register_service(Args&&... args)
    {
      auto result = std::dynamic_pointer_cast<I>(std::make_shared<T>(std::forward<Args>(args)...));
      _services[typeid(I).hash_code()] = std::static_pointer_cast<void>(result);
      return result;
    }

    template<typename I>
    std::shared_ptr<I> resolve()
    {
      auto const iter = _services.find(typeid(I).hash_code());
      if (iter == _services.end())
        return nullptr;
      return std::static_pointer_cast<I>(iter->second);
    }

    void clear() {
      _services.clear();
    }

  private:
    std::unordered_map<std::size_t, std::shared_ptr<void>> _services;
  };
}