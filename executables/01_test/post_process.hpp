#pragma once

#include <gev/game/addition.hpp>
#include <gev/game/blur.hpp>
#include <gev/game/cutoff.hpp>
#include <gev/game/render_target_2d.hpp>
#include <gev/game/tonemap.hpp>
#include <gev/game/vignette.hpp>
#include <gev/game/film_grain.hpp>
#include <memory>
#include <vector>

class post_process
{
public:
  post_process(vk::Format format);

  gev::game::render_target_2d const& process(vk::CommandBuffer c, gev::game::render_target_2d const& in);

private:
  void setup(vk::Extent2D extent);
  void new_buffer(vk::Extent2D extent);

  vk::Format _format;
  std::vector<std::unique_ptr<gev::game::render_target_2d>> _buffers;

  gev::game::cutoff _cutoff;
  gev::game::blur _blur;
  gev::game::addition _addition;
  gev::game::tonemap _tonemap;
  gev::game::vignette _vignette;
  gev::game::film_grain _film_grain;
};