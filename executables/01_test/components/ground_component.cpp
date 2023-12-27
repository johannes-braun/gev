#include "ground_component.hpp"

void ground_component::spawn()
{
  _renderer = owner()->get<renderer_component>();
  _mesh = std::make_shared<gev::game::mesh>(_tri);
  _renderer->set_material(std::make_shared<gev::game::material>());
  _renderer->get_material()->set_roughness(0.6);

  _tri.indices.clear();
  _tri.positions.clear();
  _tri.normals.clear();
  _tri.texcoords.clear();

  auto w = 128;
  auto h = 128;
  auto c = 0;
  std::vector<float> hs(w * h);

  generate_mesh(hs, w, h, 10.0f, 1.f);
  _mesh->load(_tri);
  _renderer->set_mesh(_mesh);
}

void ground_component::generate_mesh(std::span<float> heights, int rx, int ry, float hscale, float scale)
{
  float off_x = rx * 0.5 * scale;
  float off_y = rx * 0.5 * scale;

  for (int i = 0; i < rx; ++i)
  {
    for (int j = 0; j < ry; ++j)
    {
      auto const h = heights[i * ry + j] * hscale;
      rnu::vec3 const p(i * scale - off_x, h, j * scale - off_y);
      _tri.positions.push_back(p);
      _tri.normals.emplace_back(0, 1, 0);
      _tri.texcoords.emplace_back(i / float(rx), j / float(ry));
    }
  }

  for (int i = 0; i < rx - 1; ++i)
  {
    for (int j = 0; j < ry - 1; ++j)
    {
      auto const idx00 = std::uint32_t(i * ry + j);
      auto const idx01 = std::uint32_t(i * ry + (j + 1));
      auto const idx10 = std::uint32_t((i + 1) * ry + j);
      auto const idx11 = std::uint32_t((i + 1) * ry + (j + 1));

      _tri.indices.insert(_tri.indices.end(), {idx00, idx01, idx10, idx10, idx01, idx11});

      rnu::vec3 const p00 = _tri.positions[idx00];
      rnu::vec3 const p01 = _tri.positions[idx01];
      rnu::vec3 const p10 = _tri.positions[idx10];
      rnu::vec3 const p11 = _tri.positions[idx11];

      _tri.normals[idx00] = rnu::cross(p01 - p00, p10 - p00);
      _tri.normals[idx11] = rnu::cross(p10 - p11, p01 - p11);
    }
  }

  auto const prx_a = _tri.positions[(rx - 1) * ry];
  auto const prx_b = _tri.positions[(rx - 1) * ry + 1];
  auto const prx_c = _tri.positions[(rx - 2) * ry];
  _tri.normals[(rx - 1) * ry] = rnu::cross(prx_c - prx_a, prx_b - prx_a);

  auto const pry_a = _tri.positions[ry - 1];
  auto const pry_b = _tri.positions[ry - 2];
  auto const pry_c = _tri.positions[2 * ry - 1];
  _tri.normals[ry - 1] = rnu::cross(pry_c - pry_a, pry_b - pry_a);
}