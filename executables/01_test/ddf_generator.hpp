#pragma once

#include "object.hpp"
#include "camera.hpp"

class ddf
{
public:
  ddf(vk::DescriptorSetLayout set_layout, std::unique_ptr<gev::image> image, rnu::box3f bounds);

  std::unique_ptr<gev::image> const& image() const;
  vk::ImageView image_view() const;
  vk::Sampler sampler() const;
  rnu::box3f const& bounds() const;
  vk::DescriptorSet render_descriptor() const;

private:
  std::unique_ptr<gev::image> _image;
  rnu::box3f _bounds;
  vk::DescriptorSet _render_descriptor;
  std::unique_ptr<gev::buffer> _bounds_buffer;
  vk::UniqueImageView _image_view;
  vk::UniqueSampler _sampler;
};

class ddf_generator
{
public:
  ddf_generator(vk::DescriptorSetLayout camera_set_layout);

  std::unique_ptr<ddf> generate(object const& obj, std::uint32_t res);
  void generate(ddf& into, object const& obj);

  void draw(vk::CommandBuffer c, camera& cam, ddf const& field, int frame);

  void recreate_pipelines();

private:
  void generate(vk::CommandBuffer c, ddf& into, vk::ImageView view, object const& obj);
  void pad_bounds(rnu::box3f& box);

  rnu::vec3 _padding = { 0.1 };
  vk::UniqueDescriptorSetLayout _set_layout;
  vk::UniquePipelineLayout _pipeline_layout;
  vk::UniquePipeline _pipeline;
  vk::DescriptorSet _descriptor; 
  std::unique_ptr<gev::buffer> _options_buffer;

  // Rendering
  vk::UniqueDescriptorSetLayout _render_set_layout;
  vk::UniquePipelineLayout _render_pipeline_layout;
  vk::UniquePipeline _render_pipeline;
};