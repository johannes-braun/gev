#pragma once
#include <gev/image.hpp>
#include <gev/vma.hpp>
#include <vulkan/vulkan.hpp>
#include <span>

namespace gev
{
  class buffer
  {
  public:
    buffer(VmaMemoryUsage usage, VmaAllocationCreateFlags flags, vk::BufferCreateInfo const& create);

    template<typename T>
    void load_data(vk::ArrayProxy<T const> data)
    {
      load_data(data.data(), data.size() * sizeof(T));
    }

    void copy_to(vk::CommandBuffer c, buffer const& other,
      std::uint32_t size = VK_WHOLE_SIZE, std::uint32_t src_offset = 0, std::uint32_t dst_offset = 0);
    void copy_to(vk::CommandBuffer c, image const& other, vk::ImageAspectFlagBits aspect, int mip_layer = 0);

    vk::Buffer get_buffer() const;
    std::size_t size() const noexcept;

  private:
    void load_data(void const* data, std::uint32_t size, std::uint32_t offset = 0);

    std::size_t _size;
    vk::UniqueBuffer _buffer;
    unique_allocation _allocation;
    bool _host_visible;
  };
}