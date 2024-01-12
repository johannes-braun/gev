#include <gev/game/camera.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/pipeline.hpp>
#include <gev_game_shaders_files.hpp>
#include <gev/game/samplers.hpp>

namespace gev::game
{
  mesh_renderer::mesh_renderer()
  {
    _batches = std::make_shared<batch_map>();
  }

  std::shared_ptr<mesh_batch> mesh_renderer::batch(
    std::shared_ptr<shader> const& shader, std::shared_ptr<material> const& material)
  {
    auto& b = (*_batches)[shader][material];
    if (!b)
      b = std::make_shared<mesh_batch>();
    return b;
  }

  void mesh_renderer::set_environment_map(vk::DescriptorSet set)
  {
    _environment_set = set;
  }

  void mesh_renderer::set_shadow_maps(vk::DescriptorSet set)
  {
    _shadow_map_set = set;
  }

  void mesh_renderer::sync(vk::CommandBuffer c)
  {
    for (auto const& b : *_batches)
    {
      for (auto const& m : b.second)
        m.second->try_flush_buffer(c);
    }
  }

  void mesh_renderer::render(vk::CommandBuffer c, camera const& cam, std::int32_t x, std::int32_t y, std::uint32_t w,
    std::uint32_t h,
    pass_id pass, vk::SampleCountFlagBits samples)
  {
    for (auto const& [shader, batch] : *_batches)
    {
      shader->bind(c, pass);
      c.setViewport(0, vk::Viewport(float(x), float(y), float(w), float(h), 0.f, 1.f));
      c.setScissor(0, vk::Rect2D({x, y}, {w, h}));
      c.setRasterizationSamplesEXT(samples);

      shader->attach(c, cam.descriptor(), camera_set);
      shader->attach(c, _shadow_map_set, shadow_maps_set);
      shader->attach(c, _environment_set, environment_set);

      int draw_calls = 0;
      for (auto const& b : batch)
      {
        b.first->bind(c, shader->layout(), material_set);
        shader->attach(c, b.second->descriptor(), object_info_set);
        b.second->render(c);
      }
    }
  }
}    // namespace gev::game