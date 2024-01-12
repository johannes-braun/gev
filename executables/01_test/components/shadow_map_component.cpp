#include "shadow_map_component.hpp"

#include "../main_controls.hpp"

#include <gev/imgui/imgui_impl_vulkan.h>

void shadow_map_component::spawn()
{
  constexpr std::uint32_t size = 2048;
  _csm = std::make_unique<gev::game::cascaded_shadow_mapping>(vk::Extent2D(size, size), 4, 0.6f);
}

void shadow_map_component::activate()
{
  _csm->enable(*gev::service<gev::game::shadow_map_holder>());
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

  auto const dir = owner()->global_transform().forward();
  _csm->render(gev::current_frame().command_buffer, *_controls->main_camera, *_renderer, dir);
}