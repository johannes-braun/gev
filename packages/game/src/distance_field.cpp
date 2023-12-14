#include <gev/game/distance_field.hpp>
#include <gev/engine.hpp>

namespace gev::game
{
  distance_field::distance_field(std::shared_ptr<gev::image> texture, rnu::box3f bounds)
    : _image(std::move(texture)), _bounds(bounds)
  {
    _image_view = _image->create_view(vk::ImageViewType::e3D);

    _sampler = gev::engine::get().device().createSamplerUnique(vk::SamplerCreateInfo()
      .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
      .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
      .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
      .setAnisotropyEnable(false)
      .setCompareEnable(false)
      .setMagFilter(vk::Filter::eLinear)
      .setMinFilter(vk::Filter::eLinear)
      .setMipmapMode(vk::SamplerMipmapMode::eNearest)
      .setUnnormalizedCoordinates(false)
      .setMinLod(0)
      .setMaxLod(1000)
    );
  }

  std::shared_ptr<gev::image> const& distance_field::image() const
  {
    return _image;
  }

  rnu::box3f const& distance_field::bounds() const
  {
    return _bounds;
  }

  vk::ImageView distance_field::image_view() const
  {
    return _image_view.get();
  }

  vk::Sampler distance_field::sampler() const
  {
    return _sampler.get();
  }
}