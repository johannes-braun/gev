#pragma once
#include <gev/image.hpp>
#include <gev/vma.hpp>
#include <memory>
#include <span>
#include <vulkan/vulkan.hpp>

namespace gev
{
  class buffer
  {
  public:
    static std::unique_ptr<buffer> device_local(std::size_t size, vk::BufferUsageFlags usage);
    static std::unique_ptr<buffer> host_accessible(std::size_t size, vk::BufferUsageFlags usage);
    static std::unique_ptr<buffer> host_local(std::size_t size, vk::BufferUsageFlags usage);

    buffer(VmaMemoryUsage usage, VmaAllocationCreateFlags flags, vk::BufferCreateInfo const& create);

    template<typename T>
    void load_data(vk::ArrayProxy<T const> data)
    {
      load_data(data.data(), data.size() * sizeof(T));
    }
    void load_data(void const* data, std::uint32_t size, std::uint32_t offset = 0);

    template<typename T>
    std::vector<T> get_data(std::size_t offset = 0)
    {
      std::vector<T> data(_size / sizeof(T));
      get_data(data.data(), _size, offset);
      return data;
    }

    void get_data(void* data, std::uint32_t size, std::uint32_t offset = 0);

    void copy_to(vk::CommandBuffer c, buffer const& other, std::uint32_t size = VK_WHOLE_SIZE,
      std::uint32_t src_offset = 0, std::uint32_t dst_offset = 0);
    void copy_to(vk::CommandBuffer c, image const& other, vk::ImageAspectFlagBits aspect, int mip_level = 0,
      std::size_t buffer_offset = 0);
    void copy_from(vk::CommandBuffer c, image const& other, vk::ImageAspectFlagBits aspect, int mip_level = 0, std::size_t buffer_offset = 0);

    vk::Buffer get_buffer() const;
    std::size_t size() const noexcept;

  private:
    std::size_t _size;
    vk::UniqueBuffer _buffer;
    unique_allocation _allocation;
    bool _host_visible;
  };

  void buffer_barrier(vk::CommandBuffer c, buffer const& buf, vk::PipelineStageFlags from_stage, 
    vk::AccessFlags from, vk::PipelineStageFlags to_stage, vk::AccessFlags to);
}    // namespace gev