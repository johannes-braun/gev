#pragma once
#include "renderer_component.hpp"

#include <bullet/btBulletCollisionCommon.h>
#include <gev/game/mesh.hpp>
#include <gev/scenery/component.hpp>
#include <memory>
#include <rnu/math/math.hpp>
#include <rnu/obj.hpp>
#include <span>

class ground_component : public gev::scenery::component
{
public:
  void spawn();

private:
  void generate_mesh(std::span<float> heights, int rx, int ry, float hscale, float scale);

  rnu::vec4 _color = {0.3, 0.4, 0.2, 1};
  int _xpos = 0;
  rnu::triangulated_object_t _tri;
  std::shared_ptr<renderer_component> _renderer;
  std::shared_ptr<gev::game::mesh> _mesh;

  std::shared_ptr<btTriangleIndexVertexArray> _tris;
  std::shared_ptr<btBvhTriangleMeshShape> _collider;
};