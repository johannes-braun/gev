#include <gev/engine.hpp>
#include <gev/game/mesh.hpp>
#include <ranges>
#include <rnu/obj.hpp>

namespace gev::game
{
  mesh::mesh(std::filesystem::path const& path)
  {
    auto const data = rnu::load_obj(path);

    if (data.has_value())
    {
      auto const& actual_data = data.value();

      rnu::triangulated_object_t tri;
      for (auto const& d : actual_data)
      {
        for (auto const& t : rnu::triangulate(d))
          rnu::join_into(tri, t);
      }
      init(tri);
    }
  }

  mesh::mesh(rnu::triangulated_object_t const& tri)
  {
    init(tri);
  }

  void mesh::load(rnu::triangulated_object_t const& tri)
  {
    init(tri);
  }

  void mesh::make_skinned(std::span<scenery::joint const> joints)
  {
    _joints_buffer = gev::buffer::device_local(joints.size() * sizeof(scenery::joint),
      vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer);
    _joints_buffer->load_data<scenery::joint>(joints);
  }

  void mesh::init(rnu::triangulated_object_t const& tri)
  {
    _num_indices = 0;
    _joints_buffer.reset();

    if (tri.indices.empty() || tri.positions.empty())
      return;

    gev::engine::get().device().waitIdle();

    if (!_index_buffer || tri.indices.size() * sizeof(std::uint32_t) > _index_buffer->size())
    {
      _index_buffer = gev::buffer::device_local(tri.indices.size() * sizeof(std::uint32_t),
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferDst);
    }
    if (!_vertex_buffer || tri.positions.size() * sizeof(rnu::vec4) > _vertex_buffer->size())
    {
      _vertex_buffer = gev::buffer::device_local(tri.positions.size() * sizeof(rnu::vec4),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferDst);
    }
    if (!_normal_buffer || tri.normals.size() * sizeof(rnu::vec3) > _normal_buffer->size())
    {
      _normal_buffer = gev::buffer::device_local(tri.normals.size() * sizeof(rnu::vec3),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    }
    if (!_texcoords_buffer || tri.texcoords.size() * sizeof(rnu::vec2) > _texcoords_buffer->size())
    {
      _texcoords_buffer = gev::buffer::device_local(tri.texcoords.size() * sizeof(rnu::vec2),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    }

    _num_indices = tri.indices.size();
    _index_buffer->load_data<std::uint32_t>(tri.indices);

    std::vector<rnu::vec4> vec4_positions(tri.positions.size());

    rnu::vec4 min(std::numeric_limits<float>::max());
    rnu::vec4 max(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < vec4_positions.size(); ++i)
    {
      auto point = vec4_positions[i] = rnu::vec4(tri.positions[i], 1);
      min = rnu::min(min, point);
      max = rnu::max(max, point);
    }
    _bounds.position = min;
    _bounds.size = max - min;

    _vertex_buffer->load_data<rnu::vec4>(vec4_positions);
    _normal_buffer->load_data<rnu::vec3>(tri.normals);
    _texcoords_buffer->load_data<rnu::vec2>(tri.texcoords);
  }

  void mesh::draw(vk::CommandBuffer c, std::uint32_t instance_count, std::uint32_t base_instance)
  {
    if (_num_indices == 0)
      return;

    if (_joints_buffer)
    {
      vk::VertexInputAttributeDescription2EXT const attributes[] = {
        {0u, 0u, vk::Format::eR32G32B32Sfloat, 0},
        {1u, 1u, vk::Format::eR32G32B32Sfloat, 0},
        {2u, 2u, vk::Format::eR32G32Sfloat, 0},
        {3u, 3u, vk::Format::eR16G16B16A16Uint, 0},
        {4u, 3u, vk::Format::eR32G32B32A32Sfloat, sizeof(rnu::vec4ui16)},
      };
      vk::VertexInputBindingDescription2EXT const bindings[] = {
        {0u, sizeof(rnu::vec4), vk::VertexInputRate::eVertex, 1},
        {1u, sizeof(rnu::vec3), vk::VertexInputRate::eVertex, 1},
        {2u, sizeof(rnu::vec2), vk::VertexInputRate::eVertex, 1},
        {3u, sizeof(scenery::joint), vk::VertexInputRate::eVertex, 1},
      };
      c.setVertexInputEXT(bindings, attributes);
      c.bindVertexBuffers(0,
        {_vertex_buffer->get_buffer(), _normal_buffer->get_buffer(), _texcoords_buffer->get_buffer(),
          _joints_buffer->get_buffer()},
        {0ull, 0ull, 0ull, 0ull});
    }
    else
    {
      vk::VertexInputAttributeDescription2EXT const attributes[] = {
        {0u, 0u, vk::Format::eR32G32B32Sfloat, 0},
        {1u, 1u, vk::Format::eR32G32B32Sfloat, 0},
        {2u, 2u, vk::Format::eR32G32Sfloat, 0},
      };
      vk::VertexInputBindingDescription2EXT const bindings[] = {
        {0u, sizeof(rnu::vec4), vk::VertexInputRate::eVertex, 1},
        {1u, sizeof(rnu::vec3), vk::VertexInputRate::eVertex, 1},
        {2u, sizeof(rnu::vec2), vk::VertexInputRate::eVertex, 1},
      };
      c.setVertexInputEXT(bindings, attributes);
      c.bindVertexBuffers(0,
        {_vertex_buffer->get_buffer(), _normal_buffer->get_buffer(), _texcoords_buffer->get_buffer()},
        {0ull, 0ull, 0ull});
    }

    c.bindIndexBuffer(_index_buffer->get_buffer(), 0, vk::IndexType::eUint32);
    c.drawIndexed(_num_indices, instance_count, 0, 0, base_instance);
  }

  rnu::box3f const& mesh::bounds() const
  {
    return _bounds;
  }

  std::uint32_t mesh::num_indices() const
  {
    return _num_indices;
  }

  gev::buffer const& mesh::index_buffer() const
  {
    return *_index_buffer;
  }

  gev::buffer const& mesh::vertex_buffer() const
  {
    return *_vertex_buffer;
  }

  gev::buffer const& mesh::normal_buffer() const
  {
    return *_normal_buffer;
  }

  gev::buffer const& mesh::texcoords_buffer() const
  {
    return *_texcoords_buffer;
  }

  gev::buffer const* mesh::joints_buffer() const
  {
    return _joints_buffer.get();
  }
}    // namespace gev::game