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
      vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
        vk::BufferUsageFlagBits::eVertexBuffer);
    _joints_buffer->load_data<scenery::joint>(joints);
  }

  void mesh::init(rnu::triangulated_object_t const& tri)
  {
    _num_indices = 0;
    _joints_buffer.reset();

    if (tri.indices.empty() || tri.positions.empty())
      return;

    std::vector<rnu::vec4> vec4_positions(tri.positions.size());

    rnu::vec4 min(std::numeric_limits<float>::max());
    rnu::vec4 max(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < vec4_positions.size(); ++i)
    {
      auto point = vec4_positions[i] = rnu::vec4(tri.positions[i], 1);
      min = rnu::min(min, point);
      max = rnu::max(max, point);
    }
    rnu::box3f bounds;
    bounds.position = min;
    bounds.size = max - min;

    init(bounds, tri.indices, vec4_positions, tri.normals, tri.texcoords);
  }

  void mesh::init(rnu::box3f bounds, std::span<std::uint32_t const> indices, std::span<rnu::vec4 const> positions,
    std::span<rnu::vec3 const> normals, std::span<rnu::vec2 const> texcoords)
  {
    gev::engine::get().device().waitIdle();

    if (!_index_buffer || indices.size() * sizeof(std::uint32_t) > _index_buffer->size())
    {
      _index_buffer = gev::buffer::device_local(indices.size() * sizeof(std::uint32_t),
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc);
    }
    if (!_vertex_buffer || positions.size() * sizeof(rnu::vec4) > _vertex_buffer->size())
    {
      _vertex_buffer = gev::buffer::device_local(positions.size() * sizeof(rnu::vec4),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc);
    }
    if (!_normal_buffer || normals.size() * sizeof(rnu::vec3) > _normal_buffer->size())
    {
      _normal_buffer = gev::buffer::device_local(normals.size() * sizeof(rnu::vec3),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst |
          vk::BufferUsageFlagBits::eTransferSrc);
    }
    if (!_texcoords_buffer || texcoords.size() * sizeof(rnu::vec2) > _texcoords_buffer->size())
    {
      _texcoords_buffer = gev::buffer::device_local(texcoords.size() * sizeof(rnu::vec2),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst |
          vk::BufferUsageFlagBits::eTransferSrc);
    }

    _bounds = bounds;

    _num_indices = indices.size();
    _index_buffer->load_data<std::uint32_t>(indices);
    _vertex_buffer->load_data<rnu::vec4>(positions);
    _normal_buffer->load_data<rnu::vec3>(normals);
    _texcoords_buffer->load_data<rnu::vec2>(texcoords);
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

  void mesh::serialize(serializer& base, std::ostream& out)
  {
    write_typed(_bounds, out);
    write_vector(_index_buffer->get_data<std::uint32_t>(), out);
    write_vector(_vertex_buffer->get_data<rnu::vec4>(), out);
    write_vector(_normal_buffer->get_data<rnu::vec3>(), out);
    write_vector(_texcoords_buffer->get_data<rnu::vec2>(), out);

    if (_joints_buffer)
      write_vector(_joints_buffer->get_data<scenery::joint>(), out);
    else
      write_size(0ull, out);
  }

  void mesh::deserialize(serializer& base, std::istream& in)
  {
    std::vector<std::uint32_t> indices;
    std::vector<rnu::vec4> positions;
    std::vector<rnu::vec3> normals;
    std::vector<rnu::vec2> texcoords;
    std::vector<scenery::joint> joints;

    read_typed(_bounds, in);
    read_vector(indices, in);
    read_vector(positions, in);
    read_vector(normals, in);
    read_vector(texcoords, in);

    read_vector(joints, in);
    init(_bounds, indices, positions, normals, texcoords);

    if (!joints.empty())
      make_skinned(joints);
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