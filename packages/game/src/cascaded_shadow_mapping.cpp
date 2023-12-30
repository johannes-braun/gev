#include <gev/game/cascaded_shadow_mapping.hpp>
#include <gev/game/formats.hpp>

namespace gev::game
{
  static void apply_cascade(gev::game::camera& dst, rnu::mat4 const& view, rnu::mat4 const& proj, rnu::vec3 light_dir,
    float f_start, float f_end)
  {
    auto const inv_vp = inverse(proj * view);

    rnu::vec3 frustum[8] = {
      rnu::vec3(-1.0f, 1.0f, 0.0f),
      rnu::vec3(1.0f, 1.0f, 0.0f),
      rnu::vec3(1.0f, -1.0f, 0.0f),
      rnu::vec3(-1.0f, -1.0f, 0.0f),
      rnu::vec3(-1.0f, 1.0f, 1.0f),
      rnu::vec3(1.0f, 1.0f, 1.0f),
      rnu::vec3(1.0f, -1.0f, 1.0f),
      rnu::vec3(-1.0f, -1.0f, 1.0f),
    };

    for (uint32_t i = 0; i < 8; i++)
    {
      rnu::vec4 corner = inv_vp * rnu::vec4(frustum[i], 1.0f);
      frustum[i] = corner / corner.w;
    }

    for (uint32_t i = 0; i < 4; i++)
    {
      rnu::vec3 dist = frustum[i + 4] - frustum[i];
      frustum[i + 4] = frustum[i] + (dist * f_end);
      frustum[i] = frustum[i] + (dist * f_start);
    }

    rnu::vec3 center;
    for (auto const& point : frustum)
      center += rnu::vec3(point);
    center /= std::size(frustum);

    float radius = 0.0f;
    for (uint32_t i = 0; i < 8; i++)
    {
      float distance = norm(frustum[i] - center);
      radius = std::max(radius, distance);
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    rnu::vec3 e_max = rnu::vec3(radius);
    rnu::vec3 e_min = -e_max;

    float minZ = e_min.z;
    float maxZ = e_max.z;
    constexpr float zMult = 5.0f;
    if (minZ < 0)
    {
      minZ *= zMult;
    }
    else
    {
      minZ /= zMult;
    }
    if (maxZ < 0)
    {
      maxZ /= zMult;
    }
    else
    {
      maxZ *= zMult;
    }

    e_min *= 1.25f;
    e_max *= 1.25f;

    e_min = rnu::vec3i(e_min / 5) * 5;
    e_max = rnu::vec3i(e_max / 5 + 1) * 5;

    center = round(center / 5) * 5;

    gev::scenery::transform transform;
    transform.position = center - light_dir * -minZ;
    transform.rotation = rnu::look_at(transform.position, center, rnu::vec3(0, 1, 0));
    dst.set_transform(transform);
    dst.set_projection(gev::game::ortho(e_min.x, e_max.x, e_min.y, e_max.y, 0.0f, maxZ - minZ));
  }

  cascaded_shadow_mapping::cascaded_shadow_mapping(vk::Extent2D size, std::size_t num_cascades, float split_lambda)
    : _size(size), _split_lambda(split_lambda)
  {
    _cascades.resize(num_cascades);

    for (std::size_t i = 0; i < num_cascades; ++i)
    {
      auto& c = _cascades[i];

      c.camera = std::make_shared<gev::game::camera>();
      c.renderer = std::make_shared<gev::game::renderer>(_size, vk::SampleCountFlagBits::e1);
      c.renderer->set_color_format(vk::Format::eR32G32Sfloat);
      c.renderer->add_color_usage(vk::ImageUsageFlagBits::eStorage);
      c.renderer->set_clear_color({1.0f, 1.0f, 0.0f, 0.0f});
    }

    _blur_tmp = std::make_unique<render_target_2d>(_size, formats::shadow_pass,
      vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc);
    _blur = std::make_unique<blur>();
  }

  void cascaded_shadow_mapping::enable()
  {
    for (auto& c : _cascades)
    {
      if (!c.instance)
      {
        auto const r = gev::service<gev::game::mesh_renderer>();
        auto img = c.renderer->color_target().image();
        auto view = c.renderer->color_target().view();

        c.instance = r->get_shadow_map_holder()->instantiate(img, c.camera->projection_matrix() * c.camera->view());
      }
    }

    _cascades[0].instance->make_csm_root(_cascades.size());
  }

  void cascaded_shadow_mapping::disable()
  {
    for (auto& c : _cascades)
    {
      if (c.instance)
      {
        c.instance->destroy();
        c.instance = nullptr;
      }
    }
  }

  void cascaded_shadow_mapping::render(vk::CommandBuffer cmd, gev::game::mesh_renderer& r, rnu::vec3 direction)
  {
    // SHADOW MAP
    auto const main_camera = r.get_camera();
    float nearClip = rnu::near_plane(main_camera->projection());
    float farClip = rnu::far_plane(main_camera->projection());
    float clipRange = farClip - nearClip;

    float minZ = nearClip;
    float maxZ = nearClip + 100;

    float range = maxZ - minZ;
    float ratio = maxZ / minZ;

    // Calculate split depths based on view camera frustum
    // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    for (uint32_t i = 0; i < _cascades.size(); i++)
    {
      float p = (i + 1) / static_cast<float>(_cascades.size());
      float log = minZ * std::pow(ratio, p);
      float uniform = minZ + range * p;
      float d = _split_lambda * (log - uniform) + uniform;
      _cascades[i].split = (d - nearClip) / clipRange;
    }

    float last_split = 0.0;
    for (auto const& c : _cascades)
    {
      auto const this_split = c.split;
      apply_cascade(
        *c.camera, main_camera->view(), main_camera->projection_matrix(), direction, last_split, this_split);
      c.camera->sync(cmd);
      auto const split_depth = (nearClip + last_split * clipRange) * -1.0f;
      c.instance->set_cascade_split(split_depth);
      c.instance->update_transform(c.camera->projection_matrix() * c.camera->view());
      last_split = this_split;
    }
    r.try_flush(cmd);
    gev::engine::get().device().waitIdle();

    int num = 0;
    for (auto const& c : _cascades)
    {
      r.set_camera(c.camera);

      auto& src = c.renderer->color_target();

      c.renderer->prepare_frame(cmd);
      c.renderer->begin_render(cmd);
      r.render(cmd, 0, 0, _size.width, _size.height, gev::game::pass_id::shadow, vk::SampleCountFlagBits::e1);
      c.renderer->end_render(cmd);

      _blur->apply(cmd, gev::game::blur_dir::horizontal, 1.2f / (num + 1), src, *_blur_tmp);
      _blur->apply(cmd, gev::game::blur_dir::vertical, 1.2f / (num + 1), *_blur_tmp, src);

      src.image()->layout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderSampledRead);

      ++num;
    }
    r.set_camera(main_camera);
  }
}    // namespace gev::game