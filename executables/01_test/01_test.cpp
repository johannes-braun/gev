#include "components/bone_component.hpp"
#include "components/camera_component.hpp"
#include "components/camera_controller.hpp"
#include "components/renderer_component.hpp"
#include "components/shadow_map_component.hpp"
#include "components/skin_component.hpp"

#include <gev/audio/audio.hpp>
#include <gev/audio/playback.hpp>
#include <gev/audio/sound.hpp>
#include <gev/buffer.hpp>
#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/camera.hpp>
#include <gev/game/distance_field_generator.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/game/renderer.hpp>
#include <gev/imgui/imgui.h>
#include <gev/per_frame.hpp>
#include <gev/pipeline.hpp>
#include <gev/scenery/component.hpp>
#include <gev/scenery/entity_manager.hpp>
#include <gev/scenery/gltf.hpp>
#include <mdspan>
#include <random>
#include <rnu/algorithm/hash.hpp>
#include <rnu/algorithm/smooth.hpp>
#include <rnu/camera.hpp>
#include <rnu/math/math.hpp>
#include <rnu/obj.hpp>
#include <test_shaders_files.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

class mg_component : public gev::scenery::component
{
public:
  mg_component() {}

  void spawn()
  {
    _renderer = owner()->get<renderer_component>();
    _mesh = std::make_shared<gev::game::mesh>(_tri);
    _renderer->set_material(std::make_shared<gev::game::material>());
    _renderer->get_material()->set_roughness(0.6);

    _tri.indices.clear();
    _tri.positions.clear();
    _tri.normals.clear();
    _tri.texcoords.clear();

    auto w = 512;
    auto h = 512;
    auto c = 0;
    std::vector<float> hs(w * h);

    generate_mesh(hs, w, h, 10.0f, 0.3f);
    _mesh->load(_tri);
    _renderer->set_mesh(_mesh);
  }

  void update()
  {
    if (ImGui::Begin("mg_c"))
    {
      if (ImGui::ColorEdit4("Color", _color.data(), ImGuiColorEditFlags_Float))
      {
        _renderer->get_material()->set_diffuse(_color);
      }

      auto active = _renderer->is_active();
      if (ImGui::Checkbox("Active", &active))
      {
        _renderer->set_active(active);
      }
    }
    ImGui::End();
  }

private:
  void generate_mesh(std::span<float> heights, int rx, int ry, float hscale, float scale)
  {
    float off_x = rx * 0.5 * scale;
    float off_y = rx * 0.5 * scale;

    for (int i = 0; i < rx; ++i)
    {
      for (int j = 0; j < ry; ++j)
      {
        auto const h = heights[i * ry + j] * hscale;
        rnu::vec3 const p(i * scale - off_x, h, j * scale - off_y);
        _tri.positions.push_back(p);
        _tri.normals.emplace_back(0, 1, 0);
        _tri.texcoords.emplace_back(i / float(rx), j / float(ry));
      }
    }

    for (int i = 0; i < rx - 1; ++i)
    {
      for (int j = 0; j < ry - 1; ++j)
      {
        auto const idx00 = std::uint32_t(i * ry + j);
        auto const idx01 = std::uint32_t(i * ry + (j + 1));
        auto const idx10 = std::uint32_t((i + 1) * ry + j);
        auto const idx11 = std::uint32_t((i + 1) * ry + (j + 1));

        _tri.indices.insert(_tri.indices.end(), {idx00, idx01, idx10, idx10, idx01, idx11});

        rnu::vec3 const p00 = _tri.positions[idx00];
        rnu::vec3 const p01 = _tri.positions[idx01];
        rnu::vec3 const p10 = _tri.positions[idx10];
        rnu::vec3 const p11 = _tri.positions[idx11];

        _tri.normals[idx00] = rnu::cross(p01 - p00, p10 - p00);
        _tri.normals[idx11] = rnu::cross(p10 - p11, p01 - p11);
      }
    }

    auto const prx_a = _tri.positions[(rx - 1) * ry];
    auto const prx_b = _tri.positions[(rx - 1) * ry + 1];
    auto const prx_c = _tri.positions[(rx - 2) * ry];
    _tri.normals[(rx - 1) * ry] = rnu::cross(prx_c - prx_a, prx_b - prx_a);

    auto const pry_a = _tri.positions[ry - 1];
    auto const pry_b = _tri.positions[ry - 2];
    auto const pry_c = _tri.positions[2 * ry - 1];
    _tri.normals[ry - 1] = rnu::cross(pry_c - pry_a, pry_b - pry_a);
  }

  rnu::vec4 _color = {1, 1, 1, 1};
  int _xpos = 0;
  rnu::triangulated_object_t _tri;
  std::shared_ptr<renderer_component> _renderer;
  std::shared_ptr<gev::game::mesh> _mesh;
};

class test_component : public gev::scenery::component
{
public:
  test_component(std::string name, std::shared_ptr<gev::audio::sound> sound)
    : _name(std::move(name)), _sound(std::move(sound))
  {
  }

  virtual void update()
  {
    if (_playback)
      if (!_playback->is_playing())
        _playback.reset();
  }

  void play_sound()
  {
    _playback = _sound->open_playback();
    _playback->play();
  }

  void ui()
  {
    ImGui::Text("Name: %s", _name.c_str());

    bool active = owner()->is_active();
    if (ImGui::Checkbox("Active", &active))
    {
      owner()->set_active(active);
    }

    ImGui::DragFloat3("Position", owner()->local_transform.position.data(), 0.01);

    auto euler = gev::scenery::to_euler(owner()->local_transform.rotation);
    if (ImGui::DragFloat3("Orientation", euler.data(), 0.01))
    {
      owner()->local_transform.rotation = gev::scenery::from_euler(euler);
    }

    ImGui::DragFloat3("Scale", owner()->local_transform.scale.data(), 0.01);

    auto const global = owner()->global_transform();
    ImGui::Text("Global:");
    ImGui::Text("Position: (%.3f, %.3f, %.3f)", global.position.x, global.position.y, global.position.z);
    ImGui::Text("Orientation: (%.3f, %.3f, %.3f, %.3f)", global.rotation.w, global.rotation.x, global.rotation.y,
      global.rotation.z);
    ImGui::Text("Scale: (%.3f, %.3f, %.3f)", global.scale.x, global.scale.y, global.scale.z);

    if (_playback)
    {
      ImGui::Text("Playing...");
      auto l = _playback->is_looping();
      if (ImGui::Checkbox("Looping", &l))
        _playback->set_looping(l);
      auto v = _playback->get_volume();
      if (ImGui::DragFloat("Volume", &v, 0.01f, 0.0f, 1.0f))
        _playback->set_volume(v);
      auto p = _playback->get_pitch();
      if (ImGui::DragFloat("Pitch", &p, 0.01f, 0.0f, 5.0f))
        _playback->set_pitch(p);
      if (ImGui::Button("Stop"))
        _playback->stop();
    }
    else
    {
      if (ImGui::Button("Play"))
        play_sound();
    }
  }

  std::string const& name() const
  {
    return _name;
  }

private:
  std::string _name;
  std::shared_ptr<gev::audio::sound> _sound;
  std::shared_ptr<gev::audio::playback> _playback;
};

namespace default_entities
{
  constexpr std::size_t shadow_mapper = 1000;
}

class test01
{
public:
  vk::SampleCountFlagBits _render_samples = vk::SampleCountFlagBits::e4;

  test01()
  {
    gev::engine::get().start("Test 01", 1280, 720);

    gev::engine::get().services().register_service<gev::game::renderer>(
      gev::engine::get().swapchain_size(), _render_samples);
    gev::engine::get().services().register_service<gev::game::mesh_renderer>();
    gev::engine::get().services().register_service<gev::game::distance_field_generator>();

    gev::engine::get().on_resized(
      [](int w, int h)
      {
        auto renderer = gev::engine::get().services().resolve<gev::game::renderer>();
        renderer->set_render_size(vk::Extent2D(std::uint32_t(w), std::uint32_t(h)));
      });
  }

  std::shared_ptr<gev::scenery::entity> _selected_entity;

  void draw_component_tree(std::span<std::shared_ptr<gev::scenery::entity> const> entities)
  {
    for (auto const& e : entities)
    {
      auto nc = e->get<test_component>();

      ImGui::PushID(nc.get());
      auto const name = nc ? nc->name() : "<unnamed object>";

      int flags = _selected_entity == e ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;

      if (e->children().empty())
        flags = int(flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet);

      auto const elem = ImGui::TreeNodeEx(name.c_str(), flags | ImGuiTreeNodeFlags_OpenOnDoubleClick);
      if (ImGui::IsItemClicked())
      {
        if (_selected_entity == e)
          _selected_entity = nullptr;
        else
          _selected_entity = e;
      }

      if (elem)
      {
        draw_component_tree(e->children());
        ImGui::TreePop();
      }

      ImGui::PopID();
    }
  }

  std::shared_ptr<gev::audio::sound> _sound;
  std::shared_ptr<gev::game::shader> _skinned_shader;

  std::shared_ptr<gev::scenery::entity> child_from_node(std::shared_ptr<gev::scenery::entity> e,
    std::uint32_t node_index, gev::scenery::transform_node const& node, gev::scenery::gltf_data const& gltf)
  {
    auto mesh_renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
    auto distance_field_generator = gev::engine::get().services().resolve<gev::game::distance_field_generator>();

    auto ptcl = _entity_manager.instantiate(e);
    ptcl->local_transform = node.matrix();
    auto const data = std::any_cast<gev::scenery::node_data>(node.data);
    ptcl->emplace<test_component>(std::format("{} (m {})", data.name, data.mesh), _sound);

    if (gltf.skins[0].root_node() == node_index)
    {
      _skinned_shader = gev::game::shader::make_skinned();
      auto const s = ptcl->emplace<skin_component>(gltf.skins[0], gltf.nodes, gltf.animations);
      _skinned_shader->attach_always(s->skin_descriptor(), 4);
    }
    else if (auto const node = gltf.skins[0].find_joint_index(node_index))
    {
      ptcl->emplace<bone_component>(*node);
    }

    if (data.mesh != -1)
    {
      for (int i = 0; i < gltf.geometries[data.mesh].size(); ++i)
      {
        auto const& mesh = gltf.geometries[data.mesh][i];
        auto mesh_child = _entity_manager.instantiate(ptcl);
        mesh_child->emplace<test_component>(std::format("{}", data.name), _sound);
        rnu::triangulated_object_t obj;
        obj.positions = mesh.positions;
        obj.normals = mesh.positions;
        obj.texcoords = mesh.texcoords;
        obj.indices = mesh.indices;
        auto r = mesh_child->emplace<renderer_component>();
        r->set_renderer(mesh_renderer);
        r->set_shader(_skinned_shader);

        auto const& mat = gltf.materials[mesh.material_id];
        auto m = std::make_shared<gev::game::material>();
        m->set_diffuse(mat.data.color);

        if (mat.data.has_texture)
        {
          m->load_diffuse(std::make_shared<gev::game::texture>(
            mat.diffuse_map.pixels, std::uint32_t(mat.diffuse_map.width), std::uint32_t(mat.diffuse_map.height)));
        }

        r->set_material(m);
        auto mop = std::make_shared<gev::game::mesh>(obj);
        r->set_mesh(mop);
        mop->make_skinned(mesh.joints);
      }
    }

    return ptcl;
  }

  void emplace_children(std::shared_ptr<gev::scenery::entity> e, gev::scenery::transform_node const& node,
    gev::scenery::gltf_data const& gltf)
  {
    for (std::size_t i = 0; i < node.num_children; ++i)
    {
      auto const index = i + node.children_offset;
      auto const& child = gltf.nodes.nodes()[index];
      auto const ptcl = child_from_node(e, index, child, gltf);
      emplace_children(ptcl, child, gltf);
    }
  }

  int start()
  {
    auto mesh_renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
    auto distance_field_generator = gev::engine::get().services().resolve<gev::game::distance_field_generator>();

    _environment_pipeline_layout = gev::create_pipeline_layout(gev::game::layouts::defaults().camera_set_layout());

    _sound = gev::engine::get().audio_host().load_file("res/sound.ogg");

    _default_shader = gev::game::shader::make_default();

    // ################################################
    auto ptcl_data = gev::scenery::load_gltf("res/ptcl/scene.gltf");

    auto const root = ptcl_data.nodes.nodes()[0];
    auto const data = std::any_cast<gev::scenery::node_data>(root.data);
    auto const ptcl = child_from_node(nullptr, 0, root, ptcl_data);
    emplace_children(ptcl, root, ptcl_data);
    // ################################################

    auto const texture_dirt = std::make_shared<gev::game::texture>("res/dirt.png");
    auto const texture_roughness = std::make_shared<gev::game::texture>("res/roughness.jpg");
    auto const texture_grass = std::make_shared<gev::game::texture>("res/grass.jpg");
    auto const texture_ground_roughness = std::make_shared<gev::game::texture>("res/ground_roughness.jpg");
    auto const texture_floor = std::make_shared<gev::game::texture>("res/floor.png");
    auto const texture_one = std::make_shared<gev::game::texture>("res/one.png");

    auto e = _entity_manager.instantiate();
    e->emplace<test_component>("Scene Root", _sound);

    auto ch03 = _entity_manager.instantiate(e);
    ch03->emplace<test_component>("Ground Map", _sound);
    auto rc03 = ch03->emplace<renderer_component>();
    rc03->set_renderer(mesh_renderer);
    rc03->set_shader(_default_shader);

    ch03->emplace<mg_component>();

    auto sm = _entity_manager.instantiate(e);
    sm->emplace<test_component>("ShadowMap Camera", _sound);
    sm->emplace<camera_component>();
    sm->emplace<shadow_map_component>();
    sm->local_transform.position = {0, 10, 10};
    sm->local_transform.rotation = rnu::quat(rnu::vec3(1, 0, 0), rnu::radians(-20));

    auto sm2 = _entity_manager.instantiate(e);
    sm2->emplace<test_component>("ShadowMap Camera 2", _sound);
    sm2->emplace<camera_component>();
    sm2->emplace<shadow_map_component>();
    sm2->local_transform.position = {0, 14, 10};
    sm2->local_transform.rotation = rnu::quat(rnu::vec3(1, 0, 0), rnu::radians(-30));

    auto unnamed = _entity_manager.instantiate(e);
    auto e2 = _entity_manager.instantiate();
    e2->emplace<test_component>("Camera", _sound);
    e2->emplace<camera_component>();
    e2->emplace<camera_controller>();
    e2->local_transform.position = rnu::vec3(0, 2, 5);

    auto och01 = _entity_manager.instantiate(e2);
    och01->emplace<test_component>("Other Child 01", _sound);

    auto och02 = _entity_manager.instantiate(e2);
    och02->emplace<test_component>("Other Child 02", _sound);
    _entity_manager.reparent(och02, och01);
    create_pipelines();

    _entity_manager.spawn();
    return gev::engine::get().run([this](auto&& f) { return loop(f); });
  }

  void create_pipelines()
  {
    gev::engine::get().device().waitIdle();
    auto renderer = gev::engine::get().services().resolve<gev::game::renderer>();
    auto mesh_renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
    renderer->set_samples(_render_samples);
    _default_shader->set_samples(_render_samples);

    _environment_pipeline.reset();
    auto env_vertex_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_vert));
    auto env_fragment_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_frag));
    _environment_pipeline =
      gev::simple_pipeline_builder::get(_environment_pipeline_layout)
        .stage(vk::ShaderStageFlagBits::eVertex, env_vertex_shader)
        .stage(vk::ShaderStageFlagBits::eFragment, env_fragment_shader)
        .dynamic_states(vk::DynamicState::eRasterizationSamplesEXT)
        .color_attachment(gev::engine::get().swapchain_format().surfaceFormat.format)
        .build();
  }

private:
  bool ui_in_loop(gev::frame const& frame)
  {
    auto renderer = gev::engine::get().services().resolve<gev::game::renderer>();
    auto mesh_renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();

    _timer += frame.delta_time;
    _count++;
    if (_timer >= 0.3)
    {
      _fps = _count / _timer;
      _count = 0;
      _timer = 0.0;
    }

    if (ImGui::Begin("Info"))
    {
      ImGui::Text("FPS: %.2f", _fps);
      if (ImGui::Button("Reload Pipelines"))
      {
        gev::engine::get().device().waitIdle();
        _default_shader->invalidate();
        create_pipelines();
      }

      ImGui::Checkbox("Draw DDFs", &_draw_ddfs);

      if (ImGui::Checkbox("Fullscreen", &_fullscreen))
      {
        auto const window = gev::engine::get().window();
        if (_fullscreen)
        {
          glfwGetWindowPos(window, &_position_before_fullscreen.x, &_position_before_fullscreen.y);
          glfwGetWindowSize(window, &_size_before_fullscreen.x, &_size_before_fullscreen.y);

          GLFWmonitor* monitor = glfwGetPrimaryMonitor();
          const GLFWvidmode* mode = glfwGetVideoMode(monitor);
          glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
          glfwSetWindowMonitor(window, nullptr, _position_before_fullscreen.x, _position_before_fullscreen.y,
            _size_before_fullscreen.x, _size_before_fullscreen.y, GLFW_DONT_CARE);
        }
      }

      if (ImGui::Button("1x AA"))
      {
        _render_samples = vk::SampleCountFlagBits::e1;
        renderer->set_samples(_render_samples);
        create_pipelines();
      }
      if (ImGui::Button("2x AA"))
      {
        _render_samples = vk::SampleCountFlagBits::e2;
        renderer->set_samples(_render_samples);
        create_pipelines();
      }
      if (ImGui::Button("4x AA"))
      {
        _render_samples = vk::SampleCountFlagBits::e4;
        renderer->set_samples(_render_samples);
        create_pipelines();
      }
      if (ImGui::Button("8x AA"))
      {
        _render_samples = vk::SampleCountFlagBits::e8;
        renderer->set_samples(_render_samples);
        create_pipelines();
      }

      if (ImGui::Button("Close"))
      {
        ImGui::End();
        return false;
      }

      ImGui::End();
    }

    if (ImGui::Begin("Entities"))
    {
      draw_component_tree(_entity_manager.root_entities());
      ImGui::End();
    }

    if (_selected_entity && ImGui::Begin("Entity"))
    {
      auto nc = _selected_entity->get<test_component>();
      if (nc)
        nc->ui();
      ImGui::End();
    }
    return true;
  }

  bool loop(gev::frame const& frame)
  {
    auto mesh_renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
    auto renderer = gev::engine::get().services().resolve<gev::game::renderer>();

    if (!ui_in_loop(frame))
      return false;

    // PREPARE AND UPDATE SCENE
    renderer->prepare_frame(frame);
    _entity_manager.apply_transform();
    _entity_manager.early_update();

    double const target = _fixed_update_time + frame.delta_time;
    for (double t = _fixed_update_time; t < target; t += _fixed_update_step)
      _entity_manager.fixed_update(t, _fixed_update_step);
    _fixed_update_time = target;

    _entity_manager.update();
    mesh_renderer->try_flush(frame.command_buffer);
    _entity_manager.late_update();

    // ENVIRONMENT
    renderer->begin_render(frame.command_buffer, false);
    renderer->bind_pipeline(frame.command_buffer, _environment_pipeline.get(), _environment_pipeline_layout.get());
    frame.command_buffer.setRasterizationSamplesEXT(_render_samples);
    mesh_renderer->get_camera()->bind(
      frame.command_buffer, frame.frame_index, _environment_pipeline_layout.get(), mesh_renderer->camera_set);
    frame.command_buffer.draw(3, 1, 0, 0);
    renderer->end_render(frame.command_buffer);

    // MESHES FROM render_component
    renderer->begin_render(frame.command_buffer, true);
    auto const size = gev::engine::get().swapchain_size();
    mesh_renderer->render(0, 0, size.width, size.height, gev::game::pass_id::forward, _render_samples, frame);
    renderer->end_render(frame.command_buffer);

    auto const col = renderer->color_image(frame.frame_index);
    auto const image_size = frame.output_image->extent();
    if (_render_samples == vk::SampleCountFlagBits::e1)
    {
      col->layout(frame.command_buffer, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eBlit,
        vk::AccessFlagBits2::eMemoryRead);
      frame.output_image->layout(frame.command_buffer, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eMemoryWrite);
      frame.command_buffer.blitImage(col->get_image(), vk::ImageLayout::eTransferSrcOptimal,
        frame.output_image->get_image(), vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit()
          .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
          .setSrcOffsets({vk::Offset3D(0, 0, 0), vk::Offset3D(image_size.width, image_size.height, 1)})
          .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
          .setDstOffsets({vk::Offset3D(0, 0, 0), vk::Offset3D(image_size.width, image_size.height, 1)}),
        vk::Filter::eLinear);
    }
    else
    {
      col->layout(frame.command_buffer, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eResolve,
        vk::AccessFlagBits2::eMemoryRead);
      frame.output_image->layout(frame.command_buffer, vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eResolve, vk::AccessFlagBits2::eMemoryWrite);
      frame.command_buffer.resolveImage(col->get_image(), vk::ImageLayout::eTransferSrcOptimal,
        frame.output_image->get_image(), vk::ImageLayout::eTransferDstOptimal,
        vk::ImageResolve()
          .setExtent(frame.output_image->extent())
          .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
          .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1)));
    }

    return true;
  }

  double _timer = 1.0;
  double _fps = 0.0;
  double _fixed_update_time = 0.0;
  double _fixed_update_step = 0.001;
  int _count = 0;

  vk::UniquePipelineLayout _environment_pipeline_layout;
  vk::UniquePipeline _environment_pipeline;

  std::shared_ptr<gev::game::shader> _default_shader;

  bool _draw_ddfs = false;
  bool _fullscreen = false;
  rnu::vec2i _position_before_fullscreen = {0, 0};
  rnu::vec2i _size_before_fullscreen = {0, 0};

  gev::scenery::entity_manager _entity_manager;
};

int main(int argc, char** argv)
{
  int retval = 0;
  {
    test01 test{};
    retval = test.start();
  }

  gev::engine::reset();
  return retval;
}
