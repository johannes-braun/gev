#pragma once

#include <filesystem>
#include <gev/scenery/animation.hpp>
#include <unordered_map>
#include <vector>

namespace gev::scenery
{
  struct image_data
  {
    std::size_t width;
    std::size_t height;
    std::vector<std::uint8_t> pixels;
  };

  struct material
  {
    image_data diffuse_map;
    struct
    {
      rnu::vec4 color;
      uint32_t has_texture;
    } data;
  };

  struct joint
  {
    rnu::vec4ui16 indices;
    rnu::vec4 weights;
  };

  struct node_data
  {
    int mesh;
    std::string name;
  };

  struct geometry_data
  {
    std::vector<std::uint32_t> indices;
    std::vector<rnu::vec3> positions;
    std::vector<rnu::vec3> normals;
    std::vector<rnu::vec2> texcoords;
    std::vector<joint> joints;
    std::size_t material_id;
  };

  struct gltf_data
  {
    transform_tree nodes;
    std::vector<material> materials;
    std::vector<skin> skins;
    std::vector<std::vector<geometry_data>> geometries;
    std::unordered_map<std::string, joint_animation> animations;
  };

  gltf_data load_gltf(std::filesystem::path const& path);
}    // namespace gev::scenery