#include <gev/buffer.hpp>
#include <gev/game/samplers.hpp>
#include <gev/game/texture.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <gev/engine.hpp>
#include <ranges>
#include <rnu/math/packing.hpp>
#include <stb_image.h>

namespace gev::game
{
  texture::texture(color_scheme scheme, std::span<float const> data, std::uint32_t width, std::uint32_t height,
    std::optional<std::uint32_t> mip_levels)
  {
    std::uint32_t const levels = mip_levels ? *mip_levels : gev::mip_levels_for(width, height, 1);

    std::uint32_t components = 0;
    vk::Format format;
    switch (scheme)
    {
      case gev::game::color_scheme::rgba:
        components = 4;
        format = vk::Format::eR32G32B32A32Sfloat;
        _texel_size = 16;
        break;
      case gev::game::color_scheme::grayscale:
        components = 1;
        format = vk::Format::eR32Sfloat;
        _texel_size = 4;
        break;
      default: break;
    }
    assert(components != 0);

    _texture =
      gev::image_creator::get()
        .format(format)
        .size(width, height)
        .type(vk::ImageType::e2D)
        .levels(levels)
        .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eTransferSrc)
        .build();
    _texture_view = _texture->create_view(vk::ImageViewType::e2D);

    auto const staging_buffer =
      gev::buffer::host_local(width * height * components, vk::BufferUsageFlagBits::eTransferSrc);

    staging_buffer->load_data<float const>(data);

    gev::engine::get().execute_once([&](auto c)
      { staging_buffer->copy_to(c, *_texture, vk::ImageAspectFlagBits::eColor); },
      gev::engine::get().queues().graphics_command_pool.get(), true);

    if (levels > 1)
    {
      gev::engine::get().execute_once(
        [&](auto c)
        {
          _texture->generate_mipmaps(c);
          _texture->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eVertexShader,
            vk::AccessFlagBits2::eShaderSampledRead);
        },
        gev::engine::get().queues().graphics_command_pool.get(), true);
    }

    _sampler = samplers::defaults().texture();
    _sampler_type = sampler_type::default_texture;
  }

  texture::texture(color_scheme scheme, std::span<std::uint8_t const> data, std::uint32_t width, std::uint32_t height,
    std::optional<std::uint32_t> mip_levels)
  {
    std::uint32_t const levels = mip_levels ? *mip_levels : gev::mip_levels_for(width, height, 1);

    std::uint32_t components = 0;
    vk::Format format;
    switch (scheme)
    {
      case gev::game::color_scheme::rgba:
        components = 4;
        format = vk::Format::eR8G8B8A8Unorm;
        _texel_size = 4;
        break;
      case gev::game::color_scheme::grayscale:
        components = 1;
        format = vk::Format::eR8Unorm;
        _texel_size = 1;
        break;
      default: break;
    }
    assert(components != 0);

    _texture =
      gev::image_creator::get()
        .format(format)
        .size(width, height)
        .type(vk::ImageType::e2D)
        .levels(levels)
        .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eTransferSrc)
        .build();
    _texture_view = _texture->create_view(vk::ImageViewType::e2D);

    auto const staging_buffer =
      gev::buffer::host_local(width * height * components, vk::BufferUsageFlagBits::eTransferSrc);

    staging_buffer->load_data<std::uint8_t const>(data);

    gev::engine::get().execute_once([&](auto c)
      { staging_buffer->copy_to(c, *_texture, vk::ImageAspectFlagBits::eColor); },
      gev::engine::get().queues().graphics_command_pool.get(), true);

    if (levels > 1)
    {
      gev::engine::get().execute_once(
        [&](auto c)
        {
          _texture->generate_mipmaps(c);
          _texture->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eVertexShader,
            vk::AccessFlagBits2::eShaderSampledRead);
        },
        gev::engine::get().queues().graphics_command_pool.get(), true);
    }

    _sampler = samplers::defaults().texture();
    _sampler_type = sampler_type::default_texture;
  }

  texture::texture(std::filesystem::path const& image)
  {
    if (!std::filesystem::exists(image))
      throw std::invalid_argument("File does not exist.");

    int width, height, comp;
    std::unique_ptr<stbi_uc, decltype(&free)> image_data(
      stbi_load(image.string().c_str(), &width, &height, &comp, 4), &free);
    int mip_levels = gev::mip_levels_for(width, height, 1);

    _texel_size = 4;
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

    _sampler = samplers::defaults().texture();
    _sampler_type = sampler_type::default_texture;
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

    _texel_size = 8;
    _texture =
      gev::image_creator::get()
        .flags(vk::ImageCreateFlagBits::eCubeCompatible)
        .format(vk::Format::eR16G16B16A16Sfloat)
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

    _sampler = samplers::defaults().cubemap();
    _sampler_type = sampler_type::cubemap;
  }

  void texture::bind(vk::DescriptorSet set, std::uint32_t binding)
  {
    gev::update_descriptor(set, binding,
      vk::DescriptorImageInfo()
        .setImageView(_texture_view.get())
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSampler(_sampler),
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

  vk::Sampler texture::sampler() const
  {
    return _sampler;
  }

  void texture::serialize(serializer& base, std::ostream& out)
  {
    auto const buf = gev::buffer::host_local(_texture->size_bytes(), vk::BufferUsageFlagBits::eTransferDst);

    gev::engine::get().execute_once(
      [&](auto const& c)
      {
        std::uint32_t width = _texture->extent().width;
        std::uint32_t height = _texture->extent().height;
        auto const layers = _texture->array_layers();
        auto const mips = _texture->mip_levels();

        std::size_t offset = 0;
        for (std::uint32_t mip = 0; mip < mips; ++mip)
        {
          buf->copy_from(c, *_texture, vk::ImageAspectFlagBits::eColor, mip, offset);

          offset += width * height * layers * _texel_size;
          width >>= 1;
          height >>= 1;
        }
      },
      gev::engine::get().queues().transfer_command_pool.get(), true);

    write_typed(_texture->format(), out);
    write_typed(_texture->extent().width, out);
    write_typed(_texture->extent().height, out);
    write_typed(_texture->array_layers(), out);
    write_typed(_texture->mip_levels(), out);
    write_typed(_sampler_type, out);
    write_typed(_texel_size, out);
    write_vector(buf->get_data<char>(), out);
  }

  void texture::deserialize(serializer& base, std::istream& in)
  {
    vk::Format format{};
    vk::Extent3D size{1, 1, 1};
    std::uint32_t layers{};
    std::uint32_t levels{};
    std::vector<char> data{};

    read_typed(format, in);
    read_typed(size.width, in);
    read_typed(size.height, in);
    read_typed(layers, in);
    read_typed(levels, in);
    read_typed(_sampler_type, in);
    read_typed(_texel_size, in);
    read_vector(data, in);

    auto image_creator = gev::image_creator::get();

    switch (_sampler_type)
    {
      case sampler_type::default_texture: _sampler = samplers::defaults().texture(); break;
      case sampler_type::cubemap:
        _sampler = samplers::defaults().cubemap();
        image_creator.flags(vk::ImageCreateFlagBits::eCubeCompatible);
        break;
      default: throw std::runtime_error("Invalid sampler type.");
    }

    _texture =
      image_creator.format(format)
        .layers(layers)
        .size(size.width, size.height)
        .type(vk::ImageType::e2D)
        .levels(levels)
        .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
          vk::ImageUsageFlagBits::eTransferSrc)
        .build();

    switch (_sampler_type)
    {
      case sampler_type::default_texture: _texture_view = _texture->create_view(vk::ImageViewType::e2D); break;
      case sampler_type::cubemap: _texture_view = _texture->create_view(vk::ImageViewType::eCube); break;
      default: throw std::runtime_error("Invalid sampler type.");
    }

    assert(data.size() == _texture->size_bytes());

    auto const buf = gev::buffer::host_local(data.size(), vk::BufferUsageFlagBits::eTransferSrc);
    buf->load_data<char>(data);

    gev::engine::get().execute_once(
      [&](auto const& c)
      {
        _texture->layout(c, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer,
          vk::AccessFlagBits2::eTransferWrite);

        std::uint32_t width = size.width;
        std::uint32_t height = size.height;

        std::size_t offset = 0;
        for (std::uint32_t mip = 0; mip < levels; ++mip)
        {
          buf->copy_to(c, *_texture, vk::ImageAspectFlagBits::eColor, mip, offset);

          offset += width * height * layers * _texel_size;
          width >>= 1;
          height >>= 1;
        }

        _texture->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader,
          vk::AccessFlagBits2::eShaderSampledRead);
      },
      gev::engine::get().queues().transfer_command_pool.get(), true);
  }
}    // namespace gev::game