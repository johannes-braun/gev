#include <gev/game/sync_buffer.hpp>

namespace gev::game
{
  sync_buffer::sync_buffer(std::size_t size, vk::BufferUsageFlags usage, std::size_t num_frames)
    : _buffer(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0,
        vk::BufferCreateInfo()
          .setSharingMode(vk::SharingMode::eExclusive)
          .setUsage(vk::BufferUsageFlagBits::eTransferDst | usage)
          .setSize(size)),
      _staging_buffer(VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferCreateInfo()
          .setSharingMode(vk::SharingMode::eExclusive)
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSize(num_frames * size)),
      _num_frames(num_frames)
  {
  }

  void sync_buffer::next_frame()
  {
    _current_frame = (_current_frame + 1) % _num_frames;
  }

  void sync_buffer::sync(vk::CommandBuffer c)
  {
    if (_dirty)
    {
      _staging_buffer.copy_to(c, _buffer, _buffer.size(), _current_frame * _buffer.size(), 0u);
      _dirty = false;
      next_frame();
    }
  }

  gev::buffer const& sync_buffer::buffer() const
  {
    return _buffer;
  }
  
  gev::buffer const& sync_buffer::staging_buffer() const
  {
    return _staging_buffer;
  }

  std::size_t sync_buffer::size() const
  {
    return _buffer.size();
  }

  void sync_buffer::load_data(void const* data, std::uint32_t size, std::uint32_t offset)
  {
    _staging_buffer.load_data(data, size, offset + _current_frame * _buffer.size());
    _dirty = true;
  }
}    // namespace gev::game