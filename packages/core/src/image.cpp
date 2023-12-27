#include <gev/engine.hpp>
#include <gev/image.hpp>

namespace gev
{
  image_creator image_creator::get()
  {
    return image_creator();
  }

  image_creator::image_creator()
  {
    _info.setArrayLayers(1)
      .setExtent({1, 1, 1})
      .setFormat(vk::Format::eR8G8Unorm)
      .setImageType(vk::ImageType::e2D)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setMipLevels(1)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setSharingMode(vk::SharingMode::eExclusive)
      .setTiling(vk::ImageTiling::eOptimal);
  }

  image_creator& image_creator::size(std::uint32_t w, std::uint32_t h, std::uint32_t d)
  {
    _info.setExtent({w, h, d});
    return *this;
  }
  image_creator& image_creator::format(vk::Format fmt)
  {
    _info.setFormat(fmt);
    return *this;
  }
  image_creator& image_creator::type(vk::ImageType t)
  {
    _info.setImageType(t);
    return *this;
  }
  image_creator& image_creator::layers(std::uint32_t lay)
  {
    _info.setArrayLayers(lay);
    return *this;
  }
  image_creator& image_creator::levels(std::uint32_t lev)
  {
    _info.setMipLevels(lev);
    return *this;
  }
  image_creator& image_creator::samples(vk::SampleCountFlagBits s)
  {
    _info.setSamples(s);
    return *this;
  }
  image_creator& image_creator::flags(vk::ImageCreateFlags f)
  {
    _info.flags |= f;
    return *this;
  }
  image_creator& image_creator::usage(vk::ImageUsageFlags flags)
  {
    _info.usage |= flags;
    return *this;
  }

  std::unique_ptr<image> image_creator::build()
  {
    return std::make_unique<image>(_info);
  }

  std::uint32_t mip_levels_for(int w, int h, int d)
  {
    return std::uint32_t(std::log2(std::max(w, std::max(h, d))) + 1);
  }

  vk::ImageAspectFlags get_flags(vk::Format format)
  {
    switch (format)
    {
      case vk::Format::eD16Unorm:
      case vk::Format::eX8D24UnormPack32: return vk::ImageAspectFlagBits::eDepth;
      case vk::Format::eS8Uint: return vk::ImageAspectFlagBits::eStencil;
      case vk::Format::eD32Sfloat:
      case vk::Format::eD16UnormS8Uint:
      case vk::Format::eD24UnormS8Uint:
      case vk::Format::eD32SfloatS8Uint: return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
      default: return vk::ImageAspectFlagBits::eColor;
    }
  }

  image::image(vk::ImageCreateInfo const& create)
  {
    _format = create.format;
    _extent = create.extent;
    _mip_levels = create.mipLevels;
    _array_layers = create.arrayLayers;
    _samples = create.samples;

    VkImageCreateInfo const& info = create;
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto const& allocator = engine::get().allocator();
    VkImage img;
    VmaAllocation alloc;
    auto const result = vmaCreateImage(allocator.get(), &info, &alloc_info, &img, &alloc, nullptr);
    _allocation = wrap_allocation(allocator, alloc);
    _image =
      vk::UniqueImage(img, vk::ObjectDestroy<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(engine::get().device()));
    _external_image = _image.get();
    _aspect_flags = get_flags(_format);
  }

  image::image(vk::Image image, vk::Format format, vk::Extent3D extent, uint32_t mip_levels, uint32_t array_layers,
    vk::SampleCountFlagBits samples)
  {
    _external_image = image;
    _format = format;
    _extent = extent;
    _mip_levels = mip_levels;
    _array_layers = array_layers;
    _samples = samples;
    _aspect_flags = get_flags(_format);
  }

  vk::UniqueImageView image::create_view(vk::ImageViewType type, std::optional<std::uint32_t> base_layer,
    std::optional<std::uint32_t> num_layers, std::optional<std::uint32_t> base_level,
    std::optional<std::uint32_t> num_levels) const
  {
    std::uint32_t actual_base_level = base_level ? base_level.value() : 0;
    std::uint32_t actual_num_levels = num_levels ? num_levels.value() : (_mip_levels - actual_base_level);
    std::uint32_t actual_base_layer = base_layer ? base_layer.value() : 0;
    std::uint32_t actual_num_layers = num_layers ? num_layers.value() : (_array_layers - actual_base_layer);

    auto const flag = (_aspect_flags & vk::ImageAspectFlagBits::eDepth) == vk::ImageAspectFlagBits::eDepth ?
      vk::ImageAspectFlagBits::eDepth :
      vk::ImageAspectFlagBits::eColor;

    return engine::get().device().createImageViewUnique(
      vk::ImageViewCreateInfo()
        .setComponents(vk::ComponentMapping(
          vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA))
        .setFormat(_format)
        .setImage(_external_image)
        .setViewType(type)
        .setSubresourceRange(
          vk::ImageSubresourceRange(flag, actual_base_level, actual_num_levels, actual_base_layer, actual_num_layers)));
  }

  vk::ImageAspectFlags image::aspect_flags() const
  {
    return _aspect_flags;
  }

  vk::Image image::get_image() const
  {
    return _external_image;
  }

  vk::Format image::format() const
  {
    return _format;
  }

  vk::Extent3D image::extent() const
  {
    return _extent;
  }

  uint32_t image::mip_levels() const
  {
    return _mip_levels;
  }

  uint32_t image::array_layers() const
  {
    return _array_layers;
  }

  vk::SampleCountFlagBits image::samples() const
  {
    return _samples;
  }

  vk::ImageMemoryBarrier2 image::make_to_barrier() const
  {
    vk::ImageSubresourceRange actual_range =
      vk::ImageSubresourceRange()
        .setAspectMask(_aspect_flags)
        .setBaseMipLevel(0)
        .setLevelCount(_mip_levels)
        .setBaseArrayLayer(0)
        .setLayerCount(_array_layers);

    vk::ImageMemoryBarrier2 bar;
    bar.newLayout = _layout;
    bar.dstStageMask = _stage_mask;
    bar.image = _external_image;
    bar.dstQueueFamilyIndex = _queue_family_index;
    bar.subresourceRange = actual_range;
    bar.dstAccessMask = _access_mask;
    return bar;
  }

  vk::ImageMemoryBarrier2 image::make_from_barrier() const
  {
    vk::ImageSubresourceRange actual_range =
      vk::ImageSubresourceRange()
        .setAspectMask(_aspect_flags)
        .setBaseMipLevel(0)
        .setLevelCount(_mip_levels)
        .setBaseArrayLayer(0)
        .setLayerCount(_array_layers);

    vk::ImageMemoryBarrier2 bar;
    bar.oldLayout = _layout;
    bar.srcStageMask = _stage_mask;
    bar.image = _external_image;
    bar.srcQueueFamilyIndex = _queue_family_index;
    bar.subresourceRange = actual_range;
    bar.srcAccessMask = _access_mask;
    return bar;
  }

  void image::layout_hint(std::optional<vk::ImageLayout> layout, std::optional<vk::PipelineStageFlags2> stage_mask,
    std::optional<vk::AccessFlags2> access_mask, std::optional<std::uint32_t> queue_family_index)
  {
    vk::ImageLayout actual_layout = layout ? layout.value() : _layout;
    vk::PipelineStageFlags2 actual_stage_mask = stage_mask ? stage_mask.value() : _stage_mask;
    vk::AccessFlags2 actual_access_mask = access_mask ? access_mask.value() : _access_mask;
    std::uint32_t actual_queue_family_index = queue_family_index ? queue_family_index.value() : _queue_family_index;

    _layout = actual_layout;
    _stage_mask = actual_stage_mask;
    _access_mask = actual_access_mask;
    _queue_family_index = actual_queue_family_index;
  }

  void image::layout(vk::CommandBuffer c, std::optional<vk::ImageLayout> layout,
    std::optional<vk::PipelineStageFlags2> stage_mask, std::optional<vk::AccessFlags2> access_mask,
    std::optional<std::uint32_t> queue_family_index)
  {
    vk::ImageSubresourceRange actual_range =
      vk::ImageSubresourceRange()
        .setAspectMask(_aspect_flags)
        .setBaseMipLevel(0)
        .setLevelCount(_mip_levels)
        .setBaseArrayLayer(0)
        .setLayerCount(_array_layers);
    vk::ImageLayout actual_layout = layout ? layout.value() : _layout;
    vk::PipelineStageFlags2 actual_stage_mask = stage_mask ? stage_mask.value() : _stage_mask;
    vk::AccessFlags2 actual_access_mask = access_mask ? access_mask.value() : _access_mask;
    std::uint32_t actual_queue_family_index = queue_family_index ? queue_family_index.value() : _queue_family_index;

    vk::ImageMemoryBarrier2 bar;
    bar.oldLayout = _layout;
    bar.newLayout = actual_layout;
    bar.srcStageMask = _stage_mask;
    bar.dstStageMask = actual_stage_mask;
    bar.image = _external_image;
    bar.srcQueueFamilyIndex = _queue_family_index;
    bar.dstQueueFamilyIndex = actual_queue_family_index;
    bar.subresourceRange = actual_range;
    bar.srcAccessMask = _access_mask;
    bar.dstAccessMask = actual_access_mask;

    vk::DependencyInfo dep;
    dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
    dep.setImageMemoryBarriers(bar);
    c.pipelineBarrier2(dep);

    _layout = actual_layout;
    _stage_mask = actual_stage_mask;
    _access_mask = actual_access_mask;
    _queue_family_index = actual_queue_family_index;
  }

  void image::generate_mipmaps(vk::CommandBuffer c)
  {
    auto const aspect_flags = _aspect_flags;
    auto const current_queue = _queue_family_index;

    std::uint32_t width = _extent.width;
    std::uint32_t height = _extent.height;
    std::uint32_t depth = _extent.depth;
    for (std::uint32_t level = 0; level + 1 < _mip_levels; ++level)
    {
      // Transition image layer N+1 from Undefined to TransferDst
      // Transition image layer N from Undefined to TransferSrc
      // Blit N -> N+1 (linear)
      // Transition image layer N+1 from TransferDst to TransferSrc
      {
        if (level == 0)
        {
          vk::ImageMemoryBarrier2 bar_l0;
          bar_l0.oldLayout = vk::ImageLayout::eUndefined;
          bar_l0.newLayout = vk::ImageLayout::eTransferSrcOptimal;
          bar_l0.srcStageMask = vk::PipelineStageFlagBits2::eNone;
          bar_l0.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
          bar_l0.image = _external_image;
          bar_l0.srcQueueFamilyIndex = current_queue;
          bar_l0.dstQueueFamilyIndex = current_queue;
          bar_l0.subresourceRange.aspectMask = _aspect_flags;
          bar_l0.subresourceRange.baseMipLevel = level;
          bar_l0.subresourceRange.levelCount = 1;
          bar_l0.subresourceRange.baseArrayLayer = 0;
          bar_l0.subresourceRange.layerCount = _array_layers;
          bar_l0.srcAccessMask = vk::AccessFlagBits2::eNone;
          bar_l0.dstAccessMask = vk::AccessFlagBits2::eTransferRead;

          vk::DependencyInfo dep;
          dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
          dep.setImageMemoryBarriers(bar_l0);
          c.pipelineBarrier2(dep);
        }

        vk::ImageMemoryBarrier2 bar_l1;
        bar_l1.oldLayout = vk::ImageLayout::eUndefined;
        bar_l1.newLayout = vk::ImageLayout::eTransferDstOptimal;
        bar_l1.srcStageMask = vk::PipelineStageFlagBits2::eNone;
        bar_l1.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
        bar_l1.image = _external_image;
        bar_l1.srcQueueFamilyIndex = current_queue;
        bar_l1.dstQueueFamilyIndex = current_queue;
        bar_l1.subresourceRange.aspectMask = _aspect_flags;
        bar_l1.subresourceRange.baseMipLevel = level + 1;
        bar_l1.subresourceRange.levelCount = 1;
        bar_l1.subresourceRange.baseArrayLayer = 0;
        bar_l1.subresourceRange.layerCount = _array_layers;
        bar_l1.srcAccessMask = vk::AccessFlagBits2::eNone;
        bar_l1.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;

        vk::DependencyInfo dep;
        dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
        dep.setImageMemoryBarriers(bar_l1);
        c.pipelineBarrier2(dep);
      }

      vk::ImageBlit blit;
      blit.srcSubresource = vk::ImageSubresourceLayers(_aspect_flags, level, 0, _array_layers);
      blit.setSrcOffsets(std::array{
        vk::Offset3D(0, 0, 0), vk::Offset3D(std::max(1u, width), std::max(1u, height), std::max(1u, depth))});
      blit.dstSubresource = vk::ImageSubresourceLayers(_aspect_flags, level + 1, 0, _array_layers);
      blit.setDstOffsets(std::array{vk::Offset3D(0, 0, 0),
        vk::Offset3D(std::max(1u, width >> 1), std::max(1u, height >> 1), std::max(1u, depth >> 1))});

      c.blitImage(_external_image, vk::ImageLayout::eTransferSrcOptimal, _external_image,
        vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

      {
        vk::ImageMemoryBarrier2 bar_l1;
        bar_l1.oldLayout = vk::ImageLayout::eUndefined;
        bar_l1.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        bar_l1.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        bar_l1.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
        bar_l1.image = _external_image;
        bar_l1.srcQueueFamilyIndex = current_queue;
        bar_l1.dstQueueFamilyIndex = current_queue;
        bar_l1.subresourceRange.aspectMask = _aspect_flags;
        bar_l1.subresourceRange.baseMipLevel = level + 1;
        bar_l1.subresourceRange.levelCount = 1;
        bar_l1.subresourceRange.baseArrayLayer = 0;
        bar_l1.subresourceRange.layerCount = _array_layers;
        bar_l1.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        bar_l1.dstAccessMask = vk::AccessFlagBits2::eTransferRead;

        vk::DependencyInfo dep;
        dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
        dep.setImageMemoryBarriers(bar_l1);
        c.pipelineBarrier2(dep);
      }

      height >>= 1;
      width >>= 1;
      depth >>= 1;
      // Transition image layer N from TransferSrc to new_layout
    }

    layout_hint(vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eTransfer,
      vk::AccessFlagBits2::eTransferRead, current_queue);
  }
}    // namespace gev