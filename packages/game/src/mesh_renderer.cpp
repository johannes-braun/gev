#include <gev/game/camera.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/pipeline.hpp>
#include <gev_game_shaders_files.hpp>

namespace gev::game
{
  mesh_renderer::mesh_renderer()
  {
    _shadow_map_holder = std::make_shared<shadow_map_holder>();

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

  std::shared_ptr<shadow_map_holder> mesh_renderer::get_shadow_map_holder() const
  {
    return _shadow_map_holder;
  }

  void mesh_renderer::set_camera(std::shared_ptr<camera> cam)
  {
    _camera = std::move(cam);
  }

  std::shared_ptr<camera> const& mesh_renderer::get_camera() const
  {
    return _camera;
  }

  void mesh_renderer::child_renderer(mesh_renderer& r, bool share_batches, bool share_shadow_maps)
  {
    if (share_batches)
      r._batches = _batches;
    if (share_shadow_maps)
      r._shadow_map_holder = _shadow_map_holder;
  }

  void mesh_renderer::try_flush(vk::CommandBuffer c)
  {
    if (_camera)
      _camera->sync(c);
    _shadow_map_holder->try_flush_buffer(c);
    try_flush_batches(c);
  }

  void mesh_renderer::try_flush_batches(vk::CommandBuffer c)
  {
    for (auto const& b : *_batches)
    {
      for (auto const& m : b.second)
        m.second->try_flush_buffer(c);
    }
  }

  void mesh_renderer::render(vk::CommandBuffer c, std::int32_t x, std::int32_t y, std::uint32_t w, std::uint32_t h,
    pass_id pass, vk::SampleCountFlagBits samples)
  {
    for (auto const& [shader, batch] : *_batches)
    {
      shader->bind(c, pass);
      c.setViewport(0, vk::Viewport(float(x), float(y), float(w), float(h), 0.f, 1.f));
      c.setScissor(0, vk::Rect2D({x, y}, {w, h}));
      c.setRasterizationSamplesEXT(samples);

      shader->attach(c, _camera->descriptor(), camera_set);
      shader->attach(c, _shadow_map_holder->descriptor(), shadow_maps_set);

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