#pragma once

#include <filesystem>
#include <gev/buffer.hpp>
#include <gev/scenery/gltf.hpp>
#include <gev/res/serializer.hpp>
#include <rnu/obj.hpp>

namespace gev::game
{
  class mesh : public serializable
  {
  public:
    mesh() = default;
    mesh(std::filesystem::path const& path);
    mesh(rnu::triangulated_object_t const& tri);

    void draw(vk::CommandBuffer c, std::uint32_t instance_count = 1, std::uint32_t base_instance = 0);

    void make_skinned(std::span<scenery::joint const> joints);
    void load(rnu::triangulated_object_t const& tri);

    std::uint32_t num_indices() const;
    gev::buffer const& index_buffer() const;
    gev::buffer const& vertex_buffer() const;
    gev::buffer const& normal_buffer() const;
    gev::buffer const& texcoords_buffer() const;

    gev::buffer const* joints_buffer() const;

    rnu::box3f const& bounds() const;
    
    void serialize(serializer& base, std::ostream& out) override;
    void deserialize(serializer& base, std::istream& in) override;

  private:
    void init(rnu::triangulated_object_t const& obj);
    void init(rnu::box3f bounds, std::span<std::uint32_t const> indices,
      std::span<rnu::vec4 const> positions,
      std::span<rnu::vec3 const> normals,
      std::span<rnu::vec2 const> texcoords);

    rnu::box3f _bounds;
    std::uint32_t _num_indices = 0;
    std::unique_ptr<gev::buffer> _index_buffer;
    std::unique_ptr<gev::buffer> _vertex_buffer;
    std::unique_ptr<gev::buffer> _normal_buffer;
    std::unique_ptr<gev::buffer> _texcoords_buffer;
    std::unique_ptr<gev::buffer> _joints_buffer;
  };
}    // namespace gev::game