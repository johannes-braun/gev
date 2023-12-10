#pragma once

#include <functional>
#include <stdexcept>

namespace gev
{
  template<typename T>
  class per_frame
  {
  public:
    void set_generator(std::function<T(int index)> gen)
    {
      _generator = std::move(gen);
    }

    T& operator[](int index)
    {
      return get(index);
    }

    T const& operator[](int index) const
    {
      if (index >= _per_frame.size() || !_per_frame[index].second)
        throw std::out_of_range("Index out of range.");

      return _per_frame.first;
    }

    void reset()
    {
      _per_frame.clear();
    }

  private:
    T& get(int index)
    {
      if (index >= _per_frame.size())
        _per_frame.resize(index + 1);

      auto& result = _per_frame[index];

      if (!result.second)
      {
        result.first = _generator(index);
        result.second = true;
      }

      return result.first;
    }

    std::function<T(int index)> _generator;
    std::vector<std::pair<T, bool>> _per_frame;
  };
}