#include "debug_ui_component.hpp"

#include "../entity_ids.hpp"
#include "sound_component.hpp"
#include "skin_component.hpp"

#include <gev/engine.hpp>
#include <gev/scenery/entity_manager.hpp>

debug_ui_component::debug_ui_component(std::string name) : _name(std::move(name)) {}

void debug_ui_component::spawn()
{
  _collider = owner()->get<gev::scenery::collider_component>();
}

void debug_ui_component::update()
{
  if (owner()->id() == entities::player_entity)
  {
    if (auto const l = _collider.lock())
    {
      auto const win = gev::engine::get().window();
      auto const u = (glfwGetKey(win, GLFW_KEY_U) == GLFW_PRESS) ? 1.0f : 0.0f;
      auto const h = (glfwGetKey(win, GLFW_KEY_H) == GLFW_PRESS) ? 1.0f : 0.0f;
      auto const j = (glfwGetKey(win, GLFW_KEY_J) == GLFW_PRESS) ? 1.0f : 0.0f;
      auto const k = (glfwGetKey(win, GLFW_KEY_K) == GLFW_PRESS) ? 1.0f : 0.0f;
      auto const player = owner()->manager()->find_by_id(entities::player_camera);
      auto const forward = player->global_transform().forward();
      auto const right = player->global_transform().right();

      auto const horz = (h - k) * right;
      auto const vert = (u - j) * forward;

      auto fwd = horz + vert;
      fwd.y = 0;
      if (fwd.x != 0 || fwd.z != 0)
      {
        fwd = normalize(fwd) * 6;
        auto const gt = player->global_transform();
        auto const lat = look_at(gt.position, gt.position + fwd, rnu::vec3(0, 1, 0));
        _target_rotation = lat;
      }
      _smooth.to(fwd);

      auto const vel = l->get_velocity();
      l->set_velocity(_smooth.value().x, vel.y, _smooth.value().z);

      auto const collision_system = gev::service<gev::scenery::collision_system>();
      auto dst = owner()->global_transform();
      dst.position += rnu::vec3(0, -0.265, 0);
      auto const sphere = std::make_unique<btSphereShape>(0.24f);
      auto const downcast = collision_system->sweep(sphere.get(),
        owner()->global_transform(), 
        dst, collisions::player, collisions::all ^ collisions::player);
      _is_grounded = downcast.hits;

      if (_is_grounded && glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS)
      {
        l->set_velocity(_smooth.value().x, 5, _smooth.value().z);
      }
    }

    owner()->local_transform.rotation = rnu::slerp(
      owner()->local_transform.rotation, _target_rotation, 10 * gev::engine::get().current_frame().delta_time);
    _smooth.update(10 * gev::engine::get().current_frame().delta_time);
  }
}

void debug_ui_component::ui()
{
  ImGui::Text("Name: %s", _name.c_str());

  bool active = owner()->is_active();
  if (ImGui::Checkbox("Active", &active))
  {
    owner()->set_active(active);
  }

  ImGui::DragFloat3("Position", owner()->local_transform.position.data(), 0.01);

  auto euler = gev::scenery::to_euler(owner()->local_transform.rotation);
  if (ImGui::DragFloat3("Orientation", euler.data(), 0.01))
  {
    owner()->local_transform.rotation = gev::scenery::from_euler(euler);
  }

  ImGui::DragFloat3("Scale", owner()->local_transform.scale.data(), 0.01);

  auto const global = owner()->global_transform();
  ImGui::Text("Global:");
  ImGui::Text("Position: (%.3f, %.3f, %.3f)", global.position.x, global.position.y, global.position.z);
  ImGui::Text("Orientation: (%.3f, %.3f, %.3f, %.3f)", global.rotation.w, global.rotation.x, global.rotation.y,
    global.rotation.z);
  ImGui::Text("Scale: (%.3f, %.3f, %.3f)", global.scale.x, global.scale.y, global.scale.z);

  if (auto const snd = owner()->get<sound_component>())
  {
    if (ImGui::CollapsingHeader("Sound Component"))
    {
      if (ImGui::Button("Play sound"))
        snd->play_sound("sound.ogg");

      if (snd->is_playing())
      {
        ImGui::SameLine();
        ImGui::Text("Playing...");
      }

      auto l = snd->is_looping();
      if (ImGui::Checkbox("Looping", &l))
        snd->set_looping(l);

      auto v = snd->volume();
      if (ImGui::SliderFloat("Volume", &v, 0.0f, 1.0f))
        snd->set_volume(v);

      auto p = snd->pitch();
      if (ImGui::SliderFloat("Pitch", &p, 0.0f, 4.0f))
        snd->set_pitch(p);
    }
  }
  if (auto const anim = owner()->get<skin_component>())
  {
    if (ImGui::CollapsingHeader("Skin Component"))
    {
      for (auto const& [name, _] : anim->animations())
      {
        if (ImGui::Button(name.c_str()))
          anim->set_animation(name);
      }
      if (ImGui::Button("None"))
        anim->set_animation("");
    }
  }

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.2f, 0.1f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.0f, 1.0f));
  if (ImGui::Button("Destroy"))
  {
    owner()->manager()->destroy(owner());
  }
  ImGui::PopStyleColor(3);
}

std::string const& debug_ui_component::name() const
{
  return _name;
}