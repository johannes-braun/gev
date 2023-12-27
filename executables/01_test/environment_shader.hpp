#pragma once
#include <gev/game/shader.hpp>

class environment_shader : public gev::game::shader
{
protected:
  vk::UniquePipelineLayout rebuild_layout() override;
  vk::UniquePipeline rebuild(gev::game::pass_id pass) override;
};