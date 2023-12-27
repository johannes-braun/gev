#include <bullet/btBulletCollisionCommon.h>
#include <bullet/btBulletDynamicsCommon.h>
#include <gev/scenery/collider.hpp>
#include <memory>

namespace gev::scenery
{
  class physics_world : public btDiscreteDynamicsWorld
  {
  public:
    physics_world(btDispatcher* dispatcher, btBroadphaseInterface* pairCache, btConstraintSolver* constraintSolver,
      btCollisionConfiguration* collisionConfiguration)
      : btDiscreteDynamicsWorld(dispatcher, pairCache, constraintSolver, collisionConfiguration)
    {
    }

    int stepSimulation(
      btScalar timeStep, int maxSubSteps = 1, btScalar fixedTimeStep = btScalar(1.) / btScalar(60.)) override
    {
      saveKinematicState(timeStep);
      applyGravity();
      internalSingleStepSimulation(timeStep);
      clearForces();
      return 1;
    }

    void setTimeStepRemainder(btScalar time)
    {
      m_localTime = time;
    }
  };

  void handle_collisions(btDynamicsWorld* world, btScalar timeStep)
  {
    int numManifolds = world->getDispatcher()->getNumManifolds();

    for (int i = 0; i < numManifolds; ++i)
    {
      btPersistentManifold* contactManifold = world->getDispatcher()->getManifoldByIndexInternal(i);

      int numContacts = contactManifold->getNumContacts();
      if (!numContacts)
        continue;

      for (int j = 0; j < numContacts; ++j)
      {
        btManifoldPoint& pt = contactManifold->getContactPoint(j);

        if (pt.getDistance() < 0.0f)
        {
          btCollisionObject const* body0 = contactManifold->getBody0();
          btCollisionObject const* body1 = contactManifold->getBody1();

          auto* col0 = static_cast<collider_component*>(body0->getUserPointer());
          auto* col1 = static_cast<collider_component*>(body1->getUserPointer());

          auto const on_a = pt.getPositionWorldOnA();
          auto const on_b = pt.getPositionWorldOnB();
          rnu::vec3 const on_a_r{on_a.x(), on_a.y(), on_a.z()};
          rnu::vec3 const on_b_r{on_b.x(), on_b.y(), on_b.z()};

          if (auto o0 = col0->owner())
            o0->collides(col1->owner(), pt.getDistance(), on_a_r, on_b_r);
          if (auto o1 = col1->owner())
            o1->collides(col0->owner(), pt.getDistance(), on_b_r, on_a_r);
          break;
        }
      }
    }
  }

  collision_system::collision_system()
  {
    _config = std::make_unique<btDefaultCollisionConfiguration>();
    _dispatcher = std::make_unique<btCollisionDispatcher>(_config.get());
    _broad_phase = std::make_unique<btDbvtBroadphase>();
    _solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    _world = std::make_unique<physics_world>(_dispatcher.get(), _broad_phase.get(), _solver.get(), _config.get());

    _world->setGravity({0, -9.81, 0});

    _world->setInternalTickCallback(handle_collisions, this);
  }

  raycast_result collision_system::sweep(btConvexShape const* shape, transform from, transform to, int group, int mask)
  {
    btTransform from_world;
    btTransform to_world;
    from_world.setOrigin(btVector3{from.position.x, from.position.y, from.position.z});
    from_world.setRotation(btQuaternion{from.rotation.w, from.rotation.x, from.rotation.y, from.rotation.z});
    to_world.setOrigin(btVector3{to.position.x, to.position.y, to.position.z});
    to_world.setRotation(btQuaternion{to.rotation.w, to.rotation.x, to.rotation.y, to.rotation.z});

    btCollisionWorld::ClosestConvexResultCallback closest(from_world.getOrigin(), to_world.getOrigin());
    closest.m_collisionFilterMask = mask;
    closest.m_collisionFilterGroup = group;
    _world->convexSweepTest(shape, from_world, to_world, closest);

    raycast_result result;
    result.hits = closest.hasHit();
    if (result.hits)
    {
      result.target = static_cast<collider_component*>(closest.m_hitCollisionObject->getUserPointer())->owner();
      result.fraction = closest.m_closestHitFraction;
      result.position = {closest.m_hitPointWorld.x(), closest.m_hitPointWorld.y(), closest.m_hitPointWorld.z()};
      result.normal = {closest.m_hitNormalWorld.x(), closest.m_hitNormalWorld.y(), closest.m_hitNormalWorld.z()};
    }

    return result;
  }

  raycast_result collision_system::raycast(rnu::vec3 from, rnu::vec3 to, int group, int mask)
  {
    btVector3 const from_world{from.x, from.y, from.z};
    btVector3 const to_world{to.x, to.y, to.z};
    btCollisionWorld::ClosestRayResultCallback closest(from_world, to_world);
    closest.m_collisionFilterMask = mask;
    closest.m_collisionFilterGroup = group;
    _world->rayTest({from.x, from.y, from.z}, {to.x, to.y, to.z}, closest);

    raycast_result result;
    result.hits = closest.hasHit();
    if (result.hits)
    {
      result.target = static_cast<collider_component*>(closest.m_collisionObject->getUserPointer())->owner();
      result.fraction = closest.m_closestHitFraction;
      result.position = {closest.m_hitPointWorld.x(), closest.m_hitPointWorld.y(), closest.m_hitPointWorld.z()};
      result.normal = {closest.m_hitNormalWorld.x(), closest.m_hitNormalWorld.y(), closest.m_hitNormalWorld.z()};
    }
    return result;
  }

  void collision_system::fixed_step(double fixed_step)
  {
    _world->stepSimulation(fixed_step);
  }

  void collision_system::sync(float remainingTime)
  {
    static_cast<physics_world*>(_world.get())->setTimeStepRemainder(remainingTime);
    _world->synchronizeMotionStates();
  }

  void collision_system::add(btRigidBody* obj, int group, int mask)
  {
    _world->addRigidBody(obj, group, mask);
  }

  void collision_system::erase(btRigidBody* obj)
  {
    _world->removeRigidBody(obj);
  }

  collider_component::collider_component(
    std::shared_ptr<collision_system> system, std::shared_ptr<btCollisionShape> obj, float mass, bool is_static, int group, int mask)
    : _system(std::move(system)), _collision_object(std::move(obj)), _group(group), _mask(mask)
  {
    _motion_state = std::make_unique<btDefaultMotionState>();
    _rigid_body = std::make_unique<btRigidBody>(mass, _motion_state.get(), _collision_object.get());
    _rigid_body->setUserPointer(this);

    if (is_static)
      _rigid_body->setCollisionFlags(_rigid_body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
  }

  void collider_component::activate()
  {
    if (auto const s = _system.lock())
    {
      btTransform tf;
      tf.setFromOpenGLMatrix(owner()->global_transform().matrix().data());
      _rigid_body->setWorldTransform(tf);
      _rigid_body->setLinearVelocity({0, 0, 0});
      s->add(_rigid_body.get(), _group, _mask);
      _last_transform = owner()->local_transform;
    }
  }

  void collider_component::deactivate()
  {
    if (auto const s = _system.lock())
      s->erase(_rigid_body.get());
  }

  void collider_component::fixed_update(double time, double delta) {}

  void collider_component::update()
  {
    if (auto const o = owner())
    {
      if (_last_transform != o->local_transform)
      {
        btTransform tf = _rigid_body->getWorldTransform();
        if ((_last_transform.position != o->local_transform.position).any())
        {
          tf.setOrigin(
            {o->global_transform().position.x, o->global_transform().position.y, o->global_transform().position.z});
        }
        if (_last_transform.rotation == o->local_transform.rotation)
        {
          tf.setRotation({o->global_transform().rotation.w, o->global_transform().rotation.x,
            o->global_transform().rotation.y, o->global_transform().rotation.z});
        }
        _rigid_body->setWorldTransform(tf);
      }
      // else
      {
        rnu::mat4 inv_parent(1.0f);
        if (auto const par = o->parent())
        {
          inv_parent = inverse(par->global_transform().matrix());
        }

        btTransform const& tf = _rigid_body->getWorldTransform();

        rnu::mat4 next_global;
        tf.getOpenGLMatrix(next_global.data());

        auto const r = o->local_transform.rotation;
        o->local_transform = inv_parent * next_global;
        o->local_transform.rotation = r;
      }
      _last_transform = o->local_transform;
    }
  }

  rnu::vec3 collider_component::get_velocity() const
  {
    auto const& vel = _rigid_body->getLinearVelocity();
    return {vel.x(), vel.y(), vel.z()};
  }

  void collider_component::set_velocity(float x, float y, float z)
  {
    auto const p = owner()->global_transform().position;
    _rigid_body->setActivationState(ACTIVE_TAG);
    _rigid_body->setLinearVelocity({x, y, z});
  }
}    // namespace gev::scenery