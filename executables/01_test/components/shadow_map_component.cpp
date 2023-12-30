#include "shadow_map_component.hpp"

#include <gev/imgui/imgui_impl_vulkan.h>

void shadow_map_component::spawn()
{
  constexpr std::uint32_t size = 2048;
  _csm = std::make_unique<gev::game::cascaded_shadow_mapping>(vk::Extent2D(size, size), 4, 0.7f);
}

void shadow_map_component::activate()
{
  _csm->enable();
}

void shadow_map_component::deactivate()
{
  _csm->disable();
}

void shadow_map_component::late_update()
{
  if (auto const p = owner()->parent())
    p->apply_transform();
  else
    owner()->apply_transform();

  gev::frame frame = gev::engine::get().current_frame();
  auto& cmd = frame.command_buffer;

  auto const dir = owner()->global_transform().forward();
  auto const r = gev::service<gev::game::mesh_renderer>();

  _csm->render(cmd, *r, dir);
}