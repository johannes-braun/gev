#include "object.hpp"
#include <rnu/obj.hpp>
#include <gev/engine.hpp>
#include <ranges>

object::object(std::filesystem::path const& path)
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

object::object(rnu::triangulated_object_t const& tri)
{
  init(tri);
}

void object::init(rnu::triangulated_object_t const& tri)
{
  _index_buffer = std::make_unique<gev::buffer>(
    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    0,
    vk::BufferCreateInfo()
    .setQueueFamilyIndices(gev::engine::get().queues().graphics_family)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setSize(tri.indices.size() * sizeof(std::uint32_t))
    .setUsage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));
  _vertex_buffer = std::make_unique<gev::buffer>(
    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    0,
    vk::BufferCreateInfo()
    .setQueueFamilyIndices(gev::engine::get().queues().graphics_family)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setSize(tri.positions.size() * sizeof(rnu::vec4))
    .setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst));
  _normal_buffer = std::make_unique<gev::buffer>(
    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    0,
    vk::BufferCreateInfo()
    .setQueueFamilyIndices(gev::engine::get().queues().graphics_family)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setSize(tri.normals.size() * sizeof(rnu::vec3))
    .setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));
  _texcoords_buffer = std::make_unique<gev::buffer>(
    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    0,
    vk::BufferCreateInfo()
    .setQueueFamilyIndices(gev::engine::get().queues().graphics_family)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setSize(tri.texcoords.size() * sizeof(rnu::vec2))
    .setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));

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

void object::draw(vk::CommandBuffer c)
{
  c.bindIndexBuffer(_index_buffer->get_buffer(), 0, vk::IndexType::eUint32);
  c.bindVertexBuffers(0, { _vertex_buffer->get_buffer(), _normal_buffer->get_buffer(), _texcoords_buffer->get_buffer() }, { 0ull, 0ull, 0ull });
  c.drawIndexed(_num_indices, 1, 0, 0, 0);
}

rnu::box3f const& object::bounds() const
{
  return _bounds;
}

std::uint32_t object::num_indices() const
{
  return _num_indices;
}

gev::buffer const& object::index_buffer() const
{
  return *_index_buffer;
}

gev::buffer const& object::vertex_buffer() const
{
  return *_vertex_buffer;
}

gev::buffer const& object::normal_buffer() const
{
  return *_normal_buffer;
}

gev::buffer const& object::texcoords_buffer() const
{
  return *_texcoords_buffer;
}
