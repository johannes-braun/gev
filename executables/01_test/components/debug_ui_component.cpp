#include "debug_ui_component.hpp"

#include "../entity_ids.hpp"
#include "skin_component.hpp"
#include "sound_component.hpp"

#include <gev/engine.hpp>
#include <gev/scenery/entity_manager.hpp>

debug_ui_component::debug_ui_component(std::string name) : _name(std::move(name)) {}

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

void debug_ui_component::serialize(gev::serializer& base, std::ostream& out)
{
  gev::scenery::component::serialize(base, out);
  write_string(_name, out);
}

void debug_ui_component::deserialize(gev::serializer& base, std::istream& in)
{
  gev::scenery::component::deserialize(base, in);
  read_string(_name, in);
}