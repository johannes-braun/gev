#include <gev/engine.hpp>
#include <gev/game/formats.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/shader.hpp>
#include <gev/pipeline.hpp>
#include <gev_game_shaders_files.hpp>
#include <rnu/math/math.hpp>

namespace gev::game
{
  shader::shader() = default;

  void shader::invalidate()
  {
    _force_rebuild = true;
  }

  void shader::bind(vk::CommandBuffer c, pass_id pass)
  {
    c.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline(pass));
    for (auto const& [i, set] : _global_bindings)
      attach(c, set, i);
  }

  vk::Pipeline shader::pipeline(pass_id pass)
  {
    if (_force_rebuild || !_layout)
    {
      _pipelines.clear();
      _force_rebuild = false;
      _layout = rebuild_layout();
    }

    auto& p = _pipelines[pass];
    if (!p)
      p = rebuild(pass);

    return p.get();
  }

  vk::PipelineLayout shader::layout() const
  {
    return _layout.get();
  }

  void shader::attach_always(vk::DescriptorSet set, std::uint32_t index)
  {
    _global_bindings[index] = set;
  }

  void shader::attach(vk::CommandBuffer c, vk::DescriptorSet set, std::uint32_t index)
  {
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _layout.get(), index, set, nullptr);
  }

  class default_shader : public shader
  {
  public:
    default_shader(bool is_skinned) : _skinned(is_skinned) {}

  protected:
    vk::UniquePipelineLayout rebuild_layout() override
    {
      if (_skinned)
      {
        auto const& default_layouts = layouts::defaults();
        return gev::create_pipeline_layout({default_layouts.camera_set_layout(), default_layouts.material_set_layout(),
          default_layouts.object_set_layout(), default_layouts.shadow_map_layout(),
          default_layouts.environment_set_layout(), default_layouts.skinning_set_layout()});
      }
      else
      {
        auto const& default_layouts = layouts::defaults();
        return gev::create_pipeline_layout({default_layouts.camera_set_layout(), default_layouts.material_set_layout(),
          default_layouts.object_set_layout(), default_layouts.shadow_map_layout(),
          default_layouts.environment_set_layout()});
      }
    }

    vk::UniquePipeline rebuild(pass_id pass) override
    {
      auto const vertex_shader = _skinned ?
        create_shader(load_spv(gev_game_shaders::shaders::shader2_rig_vert)) :
        create_shader(load_spv(gev_game_shaders::shaders::shader2_vert));
      auto const fragment_shader = pass == pass_id::shadow ?
        create_shader(load_spv(gev_game_shaders::shaders::depth_only_frag)) :
        create_shader(load_spv(gev_game_shaders::shaders::shader2_frag));

      vk::SpecializationMapEntry pass_id(0, 0, sizeof(int));
      vk::SpecializationMapEntry const entries[] = {pass_id};
      int const pass_values[] = {int(std::size_t(pass))};
      vk::SpecializationInfo info;
      info.setMapEntries(entries);
      info.setData<int>(pass_values);

      auto builder =
        gev::simple_pipeline_builder::get(layout())
          .stage(vk::ShaderStageFlagBits::eVertex, vertex_shader, info)
          .stage(vk::ShaderStageFlagBits::eFragment, fragment_shader, info)
          .depth_attachment(gev::engine::get().depth_format())
          .stencil_attachment(gev::engine::get().depth_format())
          .dynamic_states({vk::DynamicState::eCullMode, vk::DynamicState::eRasterizationSamplesEXT,
            vk::DynamicState::eVertexInputEXT});

      if (pass == pass_id::forward)
        builder.color_attachment(formats::forward_pass);
      else if (pass == pass_id::shadow)
        builder.color_attachment(formats::shadow_pass);
      return builder.build();
    }

  private:
    bool _skinned = false;
  };

  std::shared_ptr<shader> shader::make_default()
  {
    return std::make_shared<default_shader>(false);
  }

  std::shared_ptr<shader> shader::make_skinned()
  {
    return std::make_shared<default_shader>(true);
  }

  shader_repo::shader_repo()
  {
    emplace(shaders::standard, gev::game::shader::make_default());
    emplace(shaders::skinned, gev::game::shader::make_skinned());
  }

  void shader_repo::invalidate_all() const
  {
    for (auto const& [id, sh] : _resources)
      sh->invalidate();
  }
}    // namespace gev::game