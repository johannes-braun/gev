#pragma once

#include <filesystem>
#include <gev/image.hpp>

class texture
{
public:
  texture(std::filesystem::path const& image);
  texture(std::filesystem::path const& posx,
    std::filesystem::path const& negx,
    std::filesystem::path const& posy,
    std::filesystem::path const& negy,
    std::filesystem::path const& posz,
    std::filesystem::path const& negz);

  gev::image const& image() const;
  vk::ImageView view() const;

  void bind(vk::DescriptorSet set, std::uint32_t binding);

private:
  void create(vk::ImageViewType view_type, vk::ArrayProxy<std::filesystem::path> const& paths);

  std::unique_ptr<gev::image> _texture;
  vk::UniqueImageView _texture_view;
  vk::UniqueSampler _sampler;
};