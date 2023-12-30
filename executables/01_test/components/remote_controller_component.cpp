#include "remote_controller_component.hpp"

#include "../collision_masks.hpp"
#include "../entity_ids.hpp"
#include "skin_component.hpp"

#include <gev/engine.hpp>
#include <gev/scenery/entity_manager.hpp>

void remote_controller_component::own()
{
  _is_owned = true;
}

void remote_controller_component::spawn()
{
  _collider = owner()->get<gev::scenery::collider_component>();
  _skin =
    owner()
      ->find_where([](std::shared_ptr<gev::scenery::entity> const& e) { return e->get<skin_component>() != nullptr; })
      ->get<skin_component>();
}

void remote_controller_component::update()
{
  if (_is_owned)
  {
    if (auto const l = _collider.lock())
    {
      auto const anim = _skin.lock();

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
        fwd = normalize(fwd) * 12;
        auto const gt = player->global_transform();
        auto const lat = look_at(gt.position, gt.position + fwd, rnu::vec3(0, 1, 0));
        _target_rotation = lat;

        if (anim)
          anim->set_animation("Running");
      }
      else
      {
        if (anim)
          anim->set_animation("Idle");
      }
      _smooth.to(fwd);

      auto const vel = l->get_velocity();
      l->set_velocity(_smooth.value().x, vel.y, _smooth.value().z);

      auto const collision_system = gev::service<gev::scenery::collision_system>();
      auto dst = owner()->global_transform();
      dst.position += rnu::vec3(0, -0.265, 0);
      auto const sphere = std::make_unique<btSphereShape>(0.24f);
      auto const downcast = collision_system->sweep(
        sphere.get(), owner()->global_transform(), dst, collisions::player, collisions::all ^ collisions::player);
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
