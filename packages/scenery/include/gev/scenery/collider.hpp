#pragma once

#include <bullet/btBulletCollisionCommon.h>
#include <bullet/btBulletDynamicsCommon.h>
#include <gev/scenery/component.hpp>
#include <rnu/math/math.hpp>

namespace gev::scenery
{
  struct raycast_result
  {
    bool hits;
    std::shared_ptr<entity> target;
    rnu::vec3 normal;
    rnu::vec3 position;
    float fraction;
  };

  class collision_system
  {
  public:
    collision_system();
    void fixed_step(double fixed_step);
    void sync(float remainingTime);
    void add(btRigidBody* obj, int group, int mask);
    void erase(btRigidBody* obj);
    raycast_result raycast(rnu::vec3 from, rnu::vec3 to, int group, int mask);
    raycast_result sweep(btConvexShape const* shape, transform from, transform to, int group, int mask);

  private:
    std::unique_ptr<btCollisionConfiguration> _config;
    std::unique_ptr<btBroadphaseInterface> _broad_phase;
    std::unique_ptr<btDispatcher> _dispatcher;
    std::unique_ptr<btConstraintSolver> _solver;
    std::unique_ptr<btDiscreteDynamicsWorld> _world;
  };

  class collider_component : public component
  {
  public:
    collider_component(std::shared_ptr<collision_system> system, std::shared_ptr<btCollisionShape> obj,
      float mass,
      bool is_static,
      int group,
      int mask);

    void activate() override;
    void deactivate() override;
    void fixed_update(double time, double delta) override;
    void update() override;
    rnu::vec3 get_velocity() const;
    void set_velocity(float x, float y, float z);

  private:
    std::weak_ptr<collision_system> _system;
    std::shared_ptr<btCollisionShape> _collision_object;
    std::unique_ptr<btMotionState> _motion_state;
    std::unique_ptr<btRigidBody> _rigid_body;
    transform _last_transform;
    int _group = 0;
    int _mask = 0;
  };
}    // namespace gev::scenery