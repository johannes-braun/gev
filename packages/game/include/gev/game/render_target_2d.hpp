#pragma once

#include <gev/image.hpp>

namespace gev::game
{
  class render_target_2d
  {
  public:
    render_target_2d(vk::Extent2D size, vk::Format format, vk::ImageUsageFlags usage,
      vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
    render_target_2d(std::shared_ptr<gev::image> image);
    render_target_2d(std::shared_ptr<gev::image> image, vk::ImageView view);
    std::shared_ptr<gev::image> const& image() const;
    vk::ImageView view() const;

  private:
    std::shared_ptr<gev::image> _image;
    vk::UniqueImageView _owning_view;
    vk::ImageView _view;
  };
}