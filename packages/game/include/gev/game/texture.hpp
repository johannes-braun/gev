#pragma once

#include <filesystem>
#include <gev/image.hpp>
#include <gev/res/serializer.hpp>
#include <optional>

namespace gev::game
{
  enum class color_scheme
  {
    rgba,
    grayscale
  };

  class texture : public serializable
  {
  public:
    texture() = default;
    texture(color_scheme scheme, std::span<std::uint8_t const> data, std::uint32_t width, std::uint32_t height, std::optional<std::uint32_t> mip_levels = std::nullopt);
    texture(color_scheme scheme, std::span<float const> data, std::uint32_t width, std::uint32_t height, std::optional<std::uint32_t> mip_levels = std::nullopt);
    texture(std::filesystem::path const& image);
    texture(std::filesystem::path const& posx, std::filesystem::path const& negx, std::filesystem::path const& posy,
      std::filesystem::path const& negy, std::filesystem::path const& posz, std::filesystem::path const& negz);

    gev::image const& image() const;
    vk::ImageView view() const;
    vk::Sampler sampler() const;

    void bind(vk::DescriptorSet set, std::uint32_t binding);

    void serialize(serializer& base, std::ostream& out) override;
    void deserialize(serializer& base, std::istream& in) override;

  private:
    enum class sampler_type
    {
      default_texture,
      cubemap
    };

    void create(vk::ImageViewType view_type, vk::ArrayProxy<std::filesystem::path> const& paths);

    std::unique_ptr<gev::image> _texture;
    vk::UniqueImageView _texture_view;
    vk::Sampler _sampler;

    std::size_t _texel_size;
    sampler_type _sampler_type;
  };
}    // namespace gev::game