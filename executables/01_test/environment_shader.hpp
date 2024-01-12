#pragma once
#include <gev/game/shader.hpp>
#include <gev/image.hpp>

class environment_shader : public gev::game::shader
{
protected:
  vk::UniquePipelineLayout rebuild_layout() override;
  vk::UniquePipeline rebuild(gev::game::pass_id pass) override;
};

class environment_cube_shader : public gev::game::shader
{
public:
  environment_cube_shader(vk::Format format);

protected:
  vk::UniquePipelineLayout rebuild_layout() override;
  vk::UniquePipeline rebuild(gev::game::pass_id pass) override;

private:
  vk::Format _format;
};

class environment_blur_shader : public gev::game::shader
{
public:
  environment_blur_shader(vk::Format format);

  void attach_input(vk::CommandBuffer c, std::shared_ptr<gev::image> image, vk::ImageView view, vk::Sampler sampler) const;

protected:
  vk::UniquePipelineLayout rebuild_layout() override;
  vk::UniquePipeline rebuild(gev::game::pass_id pass) override;

private:
  vk::Format _format;
  vk::UniqueDescriptorSetLayout _input_set_layout;
};