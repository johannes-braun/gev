#pragma once

#include <filesystem>
#include <gev/buffer.hpp>
#include <rnu/obj.hpp>

class object
{
public:
  object(std::filesystem::path const& path);
  object(rnu::triangulated_object_t const& tri);
  void draw(vk::CommandBuffer c);

  std::uint32_t num_indices() const;
  gev::buffer const& index_buffer() const;
  gev::buffer const& vertex_buffer() const;
  gev::buffer const& normal_buffer() const;
  gev::buffer const& texcoords_buffer() const;
  rnu::box3f const& bounds() const;

private:
  void init(rnu::triangulated_object_t const& obj);

  rnu::box3f _bounds;
  std::uint32_t _num_indices = 0;
  std::unique_ptr<gev::buffer> _index_buffer;
  std::unique_ptr<gev::buffer> _vertex_buffer;
  std::unique_ptr<gev::buffer> _normal_buffer;
  std::unique_ptr<gev::buffer> _texcoords_buffer;
};
