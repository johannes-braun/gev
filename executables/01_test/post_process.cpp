#include "post_process.hpp"
#include <gev/imgui/imgui.h>

post_process::post_process(vk::Format format) : _format(format) {}

gev::game::render_target_2d const& post_process::process(vk::CommandBuffer c, gev::game::render_target_2d const& in)
{
  if (_buffers.empty() || _buffers[0]->image()->extent() != in.image()->extent())
  {
    _buffers.clear();
    setup({in.image()->extent().width, in.image()->extent().height});
  }

  auto& a = *_buffers[0];
  auto& b = *_buffers[1];

  constexpr float blur_size = 4.2f;
  constexpr float bloom_factor = 0.5f;
  constexpr float gamma = 2.2f;
  constexpr float vignette = 0.44f;
  constexpr float grain = 0.08f;

  _cutoff.apply(c, 1.0f, in, b);
  _blur.apply(c, gev::game::blur_dir::horizontal, blur_size, b, a);
  _blur.apply(c, gev::game::blur_dir::vertical, blur_size, a, b);
  _addition.apply(c, bloom_factor, in, b, a);
  _tonemap.apply(c, gamma, a, b);
  _vignette.apply(c, vignette, b, a);
  _film_grain.apply(c, grain, a, b);

  return b;
}

void post_process::setup(vk::Extent2D extent)
{
  new_buffer(extent);
  new_buffer(extent);
}

void post_process::new_buffer(vk::Extent2D extent)
{
  _buffers.push_back(std::make_unique<gev::game::render_target_2d>(extent, _format,
    vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
      vk::ImageUsageFlagBits::eColorAttachment));
}