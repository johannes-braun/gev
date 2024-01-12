#include <gev/game/render_target_2d.hpp>

namespace gev::game
{
  render_target_2d::render_target_2d(
    vk::Extent2D size, vk::Format format, vk::ImageUsageFlags usage, vk::SampleCountFlagBits samples)
  {
    _image =
      gev::image_creator::get()
        .size(size.width, size.height)
        .type(vk::ImageType::e2D)
        .samples(samples)
        .usage(usage)
        .format(format)
        .build();
    _owning_view = _image->create_view(vk::ImageViewType::e2D);
    _view = _owning_view.get();
  }

  render_target_2d::render_target_2d(std::shared_ptr<gev::image> image)
  {
    _image = std::move(image);
    _owning_view = _image->create_view(vk::ImageViewType::e2D);
    _view = _owning_view.get();
  }

  render_target_2d::render_target_2d(std::shared_ptr<gev::image> image, vk::ImageView view)
  {
    _image = std::move(image);
    _view = view;
  }

  std::shared_ptr<gev::image> const& render_target_2d::image() const
  {
    return _image;
  }

  vk::ImageView render_target_2d::view() const
  {
    return _view;
  }
}    // namespace gev::game