#include <gev/buffer.hpp>
#include <gev/game/texture.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <gev/engine.hpp>
#include <ranges>
#include <rnu/math/packing.hpp>
#include <stb_image.h>

namespace gev::game
{
  texture::texture(std::span<std::uint8_t const> data, std::uint32_t width, std::uint32_t height)
  {
    int mip_levels = gev::mip_levels_for(width, height, 1);

    _texture =
      gev::image_creator::get()
        .format(vk::Format::eR8G8B8A8Unorm)
        .size(width, height)
        .type(vk::ImageType::e2D)
        .levels(mip_levels)
        .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eTransferSrc)
        .build();
    _texture_view = _texture->create_view(vk::ImageViewType::e2D);

    auto const staging_buffer = gev::buffer::host_local(width * height * 4, vk::BufferUsageFlagBits::eTransferSrc);

    staging_buffer->load_data<std::uint8_t const>(data);

    gev::engine::get().execute_once([&](auto c)
      { staging_buffer->copy_to(c, *_texture, vk::ImageAspectFlagBits::eColor); },
      gev::engine::get().queues().graphics_command_pool.get(), true);

    gev::engine::get().execute_once(
      [&](auto c)
      {
        _texture->generate_mipmaps(c);
        _texture->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eVertexShader,
          vk::AccessFlagBits2::eShaderSampledRead);
      },
      gev::engine::get().queues().graphics_command_pool.get(), true);

    _sampler = gev::engine::get().device().createSamplerUnique(
      vk::SamplerCreateInfo()
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setAnisotropyEnable(true)
        .setMaxAnisotropy(16.0)
        .setCompareEnable(false)
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setUnnormalizedCoordinates(false)
        .setMinLod(0)
        .setMaxLod(1000));
  }

  texture::texture(std::filesystem::path const& image)
  {
    if (!std::filesystem::exists(image))
      throw std::invalid_argument("File does not exist.");

    int width, height, comp;
    std::unique_ptr<stbi_uc, decltype(&free)> image_data(
      stbi_load(image.string().c_str(), &width, &height, &comp, 4), &free);
    int mip_levels = gev::mip_levels_for(width, height, 1);

    _texture =
      gev::image_creator::get()
        .format(vk::Format::eR8G8B8A8Unorm)
        .size(width, height)
        .type(vk::ImageType::e2D)
        .levels(mip_levels)
        .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eTransferSrc)
        .build();
    _texture_view = _texture->create_view(vk::ImageViewType::e2D);

    auto const staging_buffer = gev::buffer::host_local(width * height * 4, vk::BufferUsageFlagBits::eTransferSrc);

    staging_buffer->load_data<stbi_uc>(std::span{image_data.get(), std::size_t(width * height * 4)});

    gev::engine::get().execute_once(
      [&](auto c)
      {
        buffer_barrier(c, *staging_buffer, vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostWrite,
          vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferRead);
        staging_buffer->copy_to(c, *_texture, vk::ImageAspectFlagBits::eColor);
        _texture->layout(c, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eAllTransfer,
          vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite);

        _texture->generate_mipmaps(c);
        _texture->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eVertexShader,
          vk::AccessFlagBits2::eShaderSampledRead);
      },
      gev::engine::get().queues().graphics_command_pool.get(), true);

    _sampler = gev::engine::get().device().createSamplerUnique(
      vk::SamplerCreateInfo()
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setAnisotropyEnable(true)
        .setMaxAnisotropy(16.0)
        .setCompareEnable(false)
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setUnnormalizedCoordinates(false)
        .setMinLod(0)
        .setMaxLod(1000));
  }

  texture::texture(std::filesystem::path const& posx, std::filesystem::path const& negx,
    std::filesystem::path const& posy, std::filesystem::path const& negy, std::filesystem::path const& posz,
    std::filesystem::path const& negz)
  {
    std::vector<std::uint16_t> data;
    int width, height, comp;
    for (auto const& path : {posx, negx, posy, negy, posz, negz})
    {
      if (!std::filesystem::exists(path))
        throw std::invalid_argument("File does not exist.");

      std::unique_ptr<float, decltype(&free)> image_data(
        stbi_loadf(path.string().c_str(), &width, &height, &comp, 4), &free);
      std::size_t length = width * height * 4;
      auto r = std::span(image_data.get(), image_data.get() + length) |
        std::views::transform([&](float f) -> std::uint16_t { return rnu::to_half(f); });
      data.insert(data.end(), begin(r), end(r));
    }
    int mip_levels = gev::mip_levels_for(width, height, 1);

    _texture =
      gev::image_creator::get()
        .flags(vk::ImageCreateFlagBits::eCubeCompatible)
        .format(vk::Format::eR8G8B8A8Unorm)
        .layers(6)
        .size(width, height)
        .type(vk::ImageType::e2D)
        .levels(mip_levels)
        .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eTransferSrc)
        .build();
    _texture_view = _texture->create_view(vk::ImageViewType::eCube);

    gev::buffer staging_buffer(VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      vk::BufferCreateInfo()
        .setSharingMode(vk::SharingMode::eExclusive)
        .setSize(data.size() * sizeof(data[0]))
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc));

    staging_buffer.load_data<std::uint16_t>(data);

    gev::engine::get().execute_once([&](auto c)
      { staging_buffer.copy_to(c, *_texture, vk::ImageAspectFlagBits::eColor); },
      gev::engine::get().queues().graphics_command_pool.get(), true);

    gev::engine::get().execute_once(
      [&](auto c)
      {
        _texture->generate_mipmaps(c);
        _texture->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eVertexShader,
          vk::AccessFlagBits2::eShaderSampledRead);
      },
      gev::engine::get().queues().graphics_command_pool.get(), true);

    _sampler = gev::engine::get().device().createSamplerUnique(
      vk::SamplerCreateInfo()
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setAnisotropyEnable(false)
        .setCompareEnable(false)
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setUnnormalizedCoordinates(false)
        .setMinLod(0)
        .setMaxLod(1000));
  }

  void texture::bind(vk::DescriptorSet set, std::uint32_t binding)
  {
    gev::update_descriptor(set, binding,
      vk::DescriptorImageInfo()
        .setImageView(_texture_view.get())
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSampler(_sampler.get()),
      vk::DescriptorType::eCombinedImageSampler);
  }

  gev::image const& texture::image() const
  {
    return *_texture;
  }

  vk::ImageView texture::view() const
  {
    return _texture_view.get();
  }
}    // namespace gev::game