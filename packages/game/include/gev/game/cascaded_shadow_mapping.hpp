#pragma once

#include <cstddef>
#include <gev/game/camera.hpp>
#include <vector>
#include <gev/game/blur.hpp>

namespace gev::game
{
  class cascaded_shadow_mapping
  {
  public:
    cascaded_shadow_mapping(vk::Extent2D size, std::size_t num_cascades = 3, float split_lambda = 0.7);

    void render(vk::CommandBuffer c, gev::game::mesh_renderer& r, rnu::vec3 direction);

    void enable();
    void disable();

  private:
    struct cascade
    {
      std::shared_ptr<gev::game::camera> camera;
      std::shared_ptr<gev::game::renderer> renderer;
      std::shared_ptr<gev::game::shadow_map_instance> instance;
      float split;
    };

    vk::Extent2D _size;
    std::vector<cascade> _cascades;
    std::unique_ptr<blur> _blur1;
    std::unique_ptr<blur> _blur2;
    std::unique_ptr<gev::image> _blur_dst;
    vk::UniqueImageView _blur_dst_view;
    float _split_lambda;
  };
}    // namespace gev::game