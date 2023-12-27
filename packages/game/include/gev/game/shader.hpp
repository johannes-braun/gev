#pragma once

#include <rnu/algorithm/hash.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <gev/repo.hpp>

namespace gev::game
{
  enum class pass_id : std::size_t
  {
    forward = 0,
    shadow = 1,

    num_passes
  };

  class shader
  {
  public:
    static std::shared_ptr<shader> make_default();
    static std::shared_ptr<shader> make_skinned();

    shader();

    void invalidate();
    void bind(vk::CommandBuffer c, pass_id pass);
    void attach(vk::CommandBuffer c, vk::DescriptorSet set, std::uint32_t index);
    void attach_always(vk::DescriptorSet set, std::uint32_t index);

    vk::Pipeline pipeline(pass_id pass);
    vk::PipelineLayout layout() const;

  protected:
    virtual vk::UniquePipelineLayout rebuild_layout() = 0;
    virtual vk::UniquePipeline rebuild(pass_id pass) = 0;

  private:
    std::vector<std::pair<std::uint32_t, vk::DescriptorSet>> _global_bindings;
    bool _force_rebuild = false;
    std::vector<vk::UniquePipeline> _pipelines;
    vk::UniquePipelineLayout _layout;
  };

  class shader_id
  {
  public:
    template<std::size_t S>
    constexpr shader_id(char const (&str)[S]) : shader_id(std::string_view(str))
    {
    }

    constexpr shader_id(std::string_view str) : _id(0)
    {
      for (auto c : str)
        rnu::hash_combine(_id, c);
    }

    constexpr shader_id(std::size_t id) : _id(id) {}

    constexpr std::size_t get() const
    {
      return _id;
    }

  private:
    std::size_t _id;
  };

  class shader_repo : public repo<shader>
  {
  public:
    shader_repo();

    void invalidate_all() const;
  };

  namespace shaders
  {
    constexpr static resource_id standard = "DEFAULT";
    constexpr static resource_id skinned = "SKINNED";
  }
}    // namespace gev::game