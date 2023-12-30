#pragma once

#include <gev/buffer.hpp>

namespace gev::game
{
  class sync_buffer
  {
  public:
    sync_buffer(std::size_t size, vk::BufferUsageFlags usage, std::size_t num_frames = 1);

    void next_frame();

    template<typename T>
    void load_data(vk::ArrayProxy<T const> data)
    {
      load_data(data.data(), data.size() * sizeof(T));
    }
    void load_data(void const* data, std::uint32_t size, std::uint32_t offset = 0);

    void sync(vk::CommandBuffer c);
    gev::buffer const& buffer() const;
    gev::buffer const& staging_buffer() const;
    std::size_t size() const;

  private:
    gev::buffer _buffer;
    gev::buffer _staging_buffer;
    std::size_t _current_frame = 0;
    std::size_t _num_frames = 0;
    bool _dirty = false;
  };
}