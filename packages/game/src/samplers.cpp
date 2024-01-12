#include <gev/engine.hpp>
#include <gev/game/samplers.hpp>
#include <memory>

namespace gev::game
{
  samplers const& samplers::defaults()
  {
    static std::weak_ptr<samplers> init = gev::engine::get().services().register_service<samplers>();
    return *init.lock();
  }

  samplers::samplers()
  {
    _cubemap_sampler = gev::engine::get().device().createSamplerUnique(
      vk::SamplerCreateInfo()
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
        .setAnisotropyEnable(true)
        .setMaxAnisotropy(16.0)
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setUnnormalizedCoordinates(false)
        .setMinLod(0)
        .setMaxLod(1000));

    _texture_sampler = gev::engine::get().device().createSamplerUnique(
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

  vk::Sampler samplers::cubemap() const
  {
    return _cubemap_sampler.get();
  }

  vk::Sampler samplers::texture() const
  {
    return _texture_sampler.get();
  }
}    // namespace gev::game