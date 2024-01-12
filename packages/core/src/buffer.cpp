#include <gev/buffer.hpp>
#include <gev/engine.hpp>

namespace gev
{
  std::unique_ptr<buffer> buffer::device_local(std::size_t size, vk::BufferUsageFlags usage)
  {
    return std::make_unique<buffer>(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0,
      vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive).setSize(size).setUsage(usage));
  }

  std::unique_ptr<buffer> buffer::host_accessible(std::size_t size, vk::BufferUsageFlags usage)
  {
    return std::make_unique<buffer>(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive).setSize(size).setUsage(usage));
  }

  std::unique_ptr<buffer> buffer::host_local(std::size_t size, vk::BufferUsageFlags usage)
  {
    return std::make_unique<buffer>(VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive).setSize(size).setUsage(usage));
  }

  buffer::buffer(VmaMemoryUsage usage, VmaAllocationCreateFlags flags, vk::BufferCreateInfo const& create)
  {
    VkBufferCreateInfo const& info = create;
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = usage;
    alloc_info.flags = flags;
    _size = static_cast<std::size_t>(create.size);

    auto const& allocator = engine::get().allocator();

    VkBuffer buf;
    VmaAllocation alloc;
    VmaAllocationInfo alloc_result_info;
    auto const result = vmaCreateBuffer(allocator.get(), &info, &alloc_info, &buf, &alloc, &alloc_result_info);
    _allocation = wrap_allocation(allocator, alloc);
    _buffer =
      vk::UniqueBuffer(buf, vk::ObjectDestroy<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(engine::get().device()));

    auto const mem_properties = engine::get().physical_device().getMemoryProperties();
    auto const memory_flags = mem_properties.memoryTypes[alloc_result_info.memoryType].propertyFlags;
    _host_visible =
      (memory_flags & vk::MemoryPropertyFlagBits::eHostVisible) == vk::MemoryPropertyFlagBits::eHostVisible;
  }

  void buffer::load_data(void const* data, std::uint32_t size, std::uint32_t offset)
  {
    if (_host_visible)
    {
      auto const& allocator = engine::get().allocator();

      auto const buffer_size = size;
      VmaAllocationInfo ai{};
      vmaGetAllocationInfo(allocator.get(), _allocation.get(), &ai);

      void* memory = nullptr;
      vmaMapMemory(allocator.get(), _allocation.get(), &memory);
      std::byte* dst = static_cast<std::byte*>(memory) + offset;
      std::memcpy(dst, data, size);
      vmaFlushAllocation(allocator.get(), _allocation.get(), offset, size);
      vmaUnmapMemory(allocator.get(), _allocation.get());
    }
    else
    {
      auto const pool = engine::get().queues().transfer_command_pool.get();

      auto const staging_buffer = host_local(size, vk::BufferUsageFlagBits::eTransferSrc);
      staging_buffer->load_data(data, size);

      engine::get().execute_once([&](auto c) { staging_buffer->copy_to(c, *this, size, 0, offset); }, pool, true);
    }
  }

  void buffer::get_data(void* data, std::uint32_t size, std::uint32_t offset)
  {
    if (_host_visible)
    {
      auto const& allocator = engine::get().allocator();

      auto const buffer_size = size;
      VmaAllocationInfo ai{};
      vmaGetAllocationInfo(allocator.get(), _allocation.get(), &ai);

      void* memory = nullptr;
      vmaMapMemory(allocator.get(), _allocation.get(), &memory);
      std::byte* src = static_cast<std::byte*>(memory) + offset;
      std::memcpy(data, src, size);
      vmaUnmapMemory(allocator.get(), _allocation.get());
    }
    else
    {
      auto const pool = engine::get().queues().transfer_command_pool.get();

      auto const staging_buffer = host_local(size, vk::BufferUsageFlagBits::eTransferDst);
      engine::get().execute_once([&](auto c) { copy_to(c, *staging_buffer, size, 0, offset); }, pool, true);
      staging_buffer->get_data(data, size, offset);
    }
  }

  void buffer::copy_from(
    vk::CommandBuffer c, image const& other, vk::ImageAspectFlagBits aspect, int mip_level, std::size_t buffer_offset)
  {
    auto const extends = other.extent();

    auto const w = std::max(extends.width >> mip_level, 1u);
    auto const h = std::max(extends.height >> mip_level, 1u);
    auto const d = std::max(extends.depth >> mip_level, 1u);

    vk::ImageMemoryBarrier2 bar_l0 = other.make_from_barrier();
    bar_l0.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    bar_l0.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    bar_l0.dstQueueFamilyIndex = gev::engine::get().queues().transfer_family;
    bar_l0.dstAccessMask = vk::AccessFlagBits2::eTransferRead;

    vk::DependencyInfo dep;
    dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
    dep.setImageMemoryBarriers(bar_l0);
    c.pipelineBarrier2(dep);

    vk::BufferImageCopy region;
    region.bufferImageHeight = h;
    region.bufferOffset = buffer_offset;
    region.bufferRowLength = w;
    region.imageExtent = vk::Extent3D(w, h, d);
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageSubresource = vk::ImageSubresourceLayers(aspect, mip_level, 0, other.array_layers());
    c.copyImageToBuffer(other.get_image(), vk::ImageLayout::eTransferSrcOptimal, _buffer.get(), region);

    bar_l0 = other.make_to_barrier();

    if (bar_l0.newLayout != vk::ImageLayout::eUndefined)
    {
      bar_l0.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
      bar_l0.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
      bar_l0.srcQueueFamilyIndex = gev::engine::get().queues().transfer_family;
      bar_l0.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
      dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
      dep.setImageMemoryBarriers(bar_l0);
      c.pipelineBarrier2(dep);
    }
  }

  void buffer::copy_to(
    vk::CommandBuffer c, image const& other, vk::ImageAspectFlagBits aspect, int mip_level, std::size_t buffer_offset)
  {
    auto const extends = other.extent();

    auto const w = std::max(extends.width >> mip_level, 1u);
    auto const h = std::max(extends.height >> mip_level, 1u);
    auto const d = std::max(extends.depth >> mip_level, 1u);

    vk::ImageMemoryBarrier2 bar_l0 = other.make_from_barrier();
    bar_l0.newLayout = vk::ImageLayout::eTransferDstOptimal;
    bar_l0.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    bar_l0.dstQueueFamilyIndex = gev::engine::get().queues().transfer_family;
    bar_l0.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;

    vk::DependencyInfo dep;
    dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
    dep.setImageMemoryBarriers(bar_l0);
    c.pipelineBarrier2(dep);

    vk::BufferImageCopy region;
    region.bufferImageHeight = h;
    region.bufferOffset = buffer_offset;
    region.bufferRowLength = w;
    region.imageExtent = vk::Extent3D(w, h, d);
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageSubresource = vk::ImageSubresourceLayers(aspect, mip_level, 0, other.array_layers());
    c.copyBufferToImage(_buffer.get(), other.get_image(), vk::ImageLayout::eTransferDstOptimal, region);

    bar_l0 = other.make_to_barrier();

    if (bar_l0.newLayout != vk::ImageLayout::eUndefined)
    {
      bar_l0.oldLayout = vk::ImageLayout::eTransferDstOptimal;
      bar_l0.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
      bar_l0.srcQueueFamilyIndex = gev::engine::get().queues().transfer_family;
      bar_l0.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
      dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
      dep.setImageMemoryBarriers(bar_l0);
      c.pipelineBarrier2(dep);
    }
  }

  void buffer::copy_to(
    vk::CommandBuffer c, buffer const& other, std::uint32_t size, std::uint32_t src_offset, std::uint32_t dst_offset)
  {
    auto const& allocator = engine::get().allocator();
    VmaAllocationInfo ai{};
    vmaGetAllocationInfo(allocator.get(), _allocation.get(), &ai);
    size = size != VK_WHOLE_SIZE ? size : (ai.size - src_offset);

    auto barr =
      vk::BufferMemoryBarrier()
        .setBuffer(_buffer.get())
        .setOffset(src_offset)
        .setSize(size)
        .setSrcAccessMask({})
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    auto barr2 =
      vk::BufferMemoryBarrier(barr)
        .setBuffer(other.get_buffer())
        .setOffset(dst_offset)
        .setSize(size)
        .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

    c.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer,
      vk::DependencyFlagBits::eByRegion, nullptr, {barr}, nullptr);
    c.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
      vk::DependencyFlagBits::eByRegion, nullptr, {barr2}, nullptr);
    c.copyBuffer(_buffer.get(), other.get_buffer(), vk::BufferCopy(src_offset, dst_offset, size));

    barr.setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setDstAccessMask(vk::AccessFlagBits::eHostWrite)
      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barr2.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setDstAccessMask({})
      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    c.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
      vk::DependencyFlagBits::eByRegion, nullptr, {barr}, nullptr);
    c.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe,
      vk::DependencyFlagBits::eByRegion, nullptr, {barr2}, nullptr);
  }

  vk::Buffer buffer::get_buffer() const
  {
    return _buffer.get();
  }

  std::size_t buffer::size() const noexcept
  {
    return _size;
  }

  void buffer_barrier(vk::CommandBuffer c, buffer const& buf, vk::PipelineStageFlags from_stage, vk::AccessFlags from,
    vk::PipelineStageFlags to_stage, vk::AccessFlags to)
  {
    auto const barrier =
      vk::BufferMemoryBarrier()
        .setBuffer(buf.get_buffer())
        .setOffset(0)
        .setSize(buf.size())
        .setSrcAccessMask(from)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstAccessMask(to)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    c.pipelineBarrier(from_stage, to_stage, vk::DependencyFlagBits::eByRegion, nullptr, {barrier}, nullptr);
  }
}    // namespace gev