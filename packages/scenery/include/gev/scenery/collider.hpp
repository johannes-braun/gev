#pragma once

#include <bullet/btBulletCollisionCommon.h>
#include <bullet/btBulletDynamicsCommon.h>
#include <gev/scenery/component.hpp>
#include <rnu/math/math.hpp>
#include <variant>
#include <cassert>

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

  struct capsule_shape
  {
    float radius;
    float height;
  };

  struct sphere_shape
  {
    float radius;
  };

  struct box_shape
  {
    rnu::vec3 extents;
  };

  struct static_plane_shape
  {
    rnu::vec3 normal;
    float offset;
  };

  struct indexed_mesh_shape
  {
    std::vector<std::uint32_t> indices;
    std::vector<rnu::vec3> positions;
  };

  class collision_shape : public gev::serializable
  {
  public:
    void set(sphere_shape s)
    {
      _instance = std::make_unique<btSphereShape>(s.radius);
      _shape = std::move(s);
    }
    void set(box_shape s)
    {
      btVector3 extents(s.extents.x, s.extents.y, s.extents.z);
      _instance = std::make_unique<btBoxShape>(extents);
      _shape = std::move(s);
    }
    void set(static_plane_shape s)
    {
      btVector3 normal(s.normal.x, s.normal.y, s.normal.z);
      _instance = std::make_unique<btStaticPlaneShape>(normal, s.offset);
      _shape = std::move(s);
    }
    void set(capsule_shape s)
    {
      _instance = std::make_unique<btCapsuleShape>(s.radius, s.height);
      _shape = std::move(s);
    }
    void set(indexed_mesh_shape s)
    {
      int* tris = reinterpret_cast<int*>(s.indices.data());
      btScalar* pos = s.positions.data()[0].data();
      _optional_vertex_array = std::make_unique<btTriangleIndexVertexArray>(int(s.indices.size() / 3), tris,
        int(3 * sizeof(std::uint32_t)), int(s.positions.size()), pos, int(sizeof(s.positions[0])));
      _instance = std::make_unique<btBvhTriangleMeshShape>(_optional_vertex_array.get(), false);
      _shape = std::move(s);
    }
    void set(std::nullptr_t)
    {
      _instance.reset();
      _shape = std::monostate{};
    }

    void serialize(serializer& base, std::ostream& out) override
    {
      struct
      {
        void operator()(std::monostate const& s) const {}
        void operator()(sphere_shape const& s) const
        {
          write_typed(s.radius, out);
        }
        void operator()(box_shape const& s) const
        {
          write_typed(s.extents, out);
        }
        void operator()(capsule_shape const& s) const
        {
          write_typed(s.radius, out);
          write_typed(s.height, out);
        }
        void operator()(static_plane_shape const& s) const
        {
          write_typed(s.normal, out);
          write_typed(s.offset, out);
        }
        void operator()(indexed_mesh_shape const& s) const
        {
          write_vector(s.indices, out);
          write_vector(s.positions, out);
        }

        std::ostream& out;
      } write{out};
      std::size_t const index = _shape.index();
      write_size(index, out);
      std::visit(write, _shape);
    }

    void deserialize(serializer& base, std::istream& in) override
    {
      std::size_t index = 0;
      read_size(index, in);
      switch (index)
      {
        case 0:
        {
          set(nullptr);
        }
        break;
        case 1:
        {
          sphere_shape s;
          read_typed(s.radius, in);
          set(s);
        }
        break;
        case 2:
        {
          box_shape s;
          read_typed(s.extents, in);
          set(s);
        }
        break;
        case 3:
        {
          capsule_shape s;
          read_typed(s.radius, in);
          read_typed(s.height, in);
          set(s);
        }
        break;
        case 4:
        {
          static_plane_shape s;
          read_typed(s.normal, in);
          read_typed(s.offset, in);
          set(s);
        }
        break;
        case 5:
        {
          indexed_mesh_shape s;
          read_vector(s.indices, in);
          read_vector(s.positions, in);
          set(s);
        }
        break;
      }
    }

    btCollisionShape* get_shape() const
    {
      return _instance.get();
    }

  private:
    using shape =
      std::variant<std::monostate, sphere_shape, box_shape, capsule_shape, static_plane_shape, indexed_mesh_shape>;

    std::unique_ptr<btTriangleIndexVertexArray> _optional_vertex_array;
    std::unique_ptr<btCollisionShape> _instance;
    shape _shape;
  };

  class collision_system
  {
  public:
    static std::shared_ptr<collision_system> get_default();

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
    collider_component();
    collider_component(std::shared_ptr<collision_shape> obj,
      float mass,
      bool is_static,
      int group,
      int mask);

    void activate() override;
    void deactivate() override;
    void set_shape(std::shared_ptr<collision_shape> shape);
    void fixed_update(double time, double delta) override;
    void update() override;
    rnu::vec3 get_velocity() const;
    void set_velocity(float x, float y, float z);

    void serialize(serializer& base, std::ostream& out) override;
    void deserialize(serializer& base, std::istream& in) override;

  private:
    std::weak_ptr<collision_system> _system;
    std::shared_ptr<collision_shape> _collision_object;
    std::unique_ptr<btMotionState> _motion_state;
    std::unique_ptr<btRigidBody> _rigid_body;
    transform _last_transform;
    int _group = 0;
    int _mask = 0;
  };
}    // namespace gev::scenery