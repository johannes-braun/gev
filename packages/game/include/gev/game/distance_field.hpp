#pragma once

#include <memory>
#include <gev/image.hpp>
#include <rnu/math/math.hpp>

namespace gev::game
{
  class distance_field
  {
  public:
    distance_field(std::shared_ptr<gev::image> texture, rnu::box3f bounds);

    std::shared_ptr<gev::image> const& image() const;
    rnu::box3f const& bounds() const;
    vk::ImageView image_view() const;
    vk::Sampler sampler() const;

  private:
    std::shared_ptr<gev::image> _image;
    rnu::box3f _bounds;
    vk::UniqueImageView _image_view;
    vk::UniqueSampler _sampler;
  };
}