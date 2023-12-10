#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>
#include <gev/vma.hpp>

namespace gev
{
  std::uint32_t mip_levels_for(int w, int h, int d);

  class image
  {
  public:
    image(vk::ImageCreateInfo const& create);
    image(vk::Image image, vk::Format format,
      vk::Extent3D extent,
      uint32_t mip_levels = 1,
      uint32_t array_layers = 1,
      vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

    void generate_mipmaps(vk::CommandBuffer c);

    void layout(
      vk::CommandBuffer c,
      std::optional<vk::ImageLayout> layout = std::nullopt,
      std::optional<vk::PipelineStageFlags2> stage_mask = std::nullopt,
      std::optional<vk::AccessFlags2> access_mask = std::nullopt,
      std::optional<std::uint32_t> queue_family_index = std::nullopt
    );

    void layout_hint(
      std::optional<vk::ImageLayout> layout = std::nullopt,
      std::optional<vk::PipelineStageFlags2> stage_mask = std::nullopt,
      std::optional<vk::AccessFlags2> access_mask = std::nullopt,
      std::optional<std::uint32_t> queue_family_index = std::nullopt
    );

    vk::UniqueImageView create_view(vk::ImageViewType type,
      std::optional<std::uint32_t> base_layer = std::nullopt,
      std::optional<std::uint32_t> num_layers = std::nullopt,
      std::optional<std::uint32_t> base_level = std::nullopt,
      std::optional<std::uint32_t> num_levels = std::nullopt) const;

    vk::ImageAspectFlags aspect_flags() const;
    vk::Image get_image() const;
    vk::Format format() const;
    vk::Extent3D extent() const;
    uint32_t mip_levels() const;
    uint32_t array_layers() const;
    vk::SampleCountFlagBits samples() const;

    vk::ImageMemoryBarrier2 make_to_barrier() const;
    vk::ImageMemoryBarrier2 make_from_barrier() const;

  private:
    vk::ImageAspectFlags _aspect_flags;
    vk::Format _format = vk::Format::eUndefined;
    vk::Extent3D _extent = {};
    uint32_t _mip_levels = {};
    uint32_t _array_layers = {};
    vk::SampleCountFlagBits _samples = vk::SampleCountFlagBits::e1;
    vk::Image _external_image;
    vk::UniqueImage _image;
    unique_allocation _allocation;

    vk::PipelineStageFlags2 _stage_mask = vk::PipelineStageFlagBits2::eNone;
    vk::AccessFlags2 _access_mask = vk::AccessFlagBits2::eNone;
    vk::ImageLayout _layout = vk::ImageLayout::eUndefined;
    std::uint32_t _queue_family_index = VK_QUEUE_FAMILY_EXTERNAL;
  };
}