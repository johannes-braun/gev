#include "components/camera_component.hpp"
#include "components/camera_controller.hpp"
#include "components/renderer_component.hpp"

#include <gev/engine.hpp>
#include <gev/buffer.hpp>
#include <gev/descriptors.hpp>
#include <gev/pipeline.hpp>
#include <gev/per_frame.hpp>
#include <gev/imgui/imgui.h>
#include <gev/scenery/entity_manager.hpp>
#include <gev/scenery/component.hpp>
#include <gev/audio/audio.hpp>
#include <gev/audio/sound.hpp>
#include <gev/audio/playback.hpp>
#include <gev/game/renderer.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/game/camera.hpp>
#include <gev/game/distance_field_generator.hpp>

#include <rnu/math/math.hpp>
#include <rnu/obj.hpp>
#include <rnu/camera.hpp>
#include <rnu/algorithm/smooth.hpp>

#include <test_shaders_files.hpp>

#include <random>
#include <mdspan>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

class mg_component : public gev::scenery::component
{
public:
  mg_component() {
    _diffuse = std::make_shared<gev::game::texture>("res/grass.jpg");
    _roughness = std::make_shared<gev::game::texture>("res/ground_roughness.jpg");
  }

  void spawn() {
    _renderer = owner()->get<renderer_component>();

    _renderer->set_mesh(std::make_shared<gev::game::mesh>(_tri));
    _renderer->set_material(std::make_shared<gev::game::material>());
    _renderer->get_material()->load_diffuse(_diffuse);
    _renderer->get_material()->load_roughness(_roughness);
    _renderer->get_material()->set_two_sided(true);
  }

  void update()
  {
    ImGui::Begin("mg_c");
    if (ImGui::Button("eh"))
    {
      _tri.indices.clear();
      _tri.positions.clear();
      _tri.normals.clear();
      _tri.texcoords.clear();

      auto w = 0;
      auto h = 0;
      auto c = 0;
      float* arr = stbi_loadf("res/heightmap.png", &w, &h, &c, 1);
      std::vector<float> hs(arr, arr + w * h);
      STBI_FREE(arr);

      generate_mesh(hs, w, h, 10.0f, 0.3f);
      _renderer->get_mesh()->load(_tri);
    }
    ImGui::End();
  }

private:

  void generate_mesh(std::span<float> heights, int rx, int ry, float hscale, float scale)
  {
    for (int i = 0; i < rx; ++i)
    {
      for (int j = 0; j < ry; ++j)
      {
        auto const h = heights[i * ry + j] * hscale;
        rnu::vec3 const p(i * scale, h, j * scale);
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
        auto const idx10 = std::uint32_t((i + 1) * ry + j);
        auto const idx01 = std::uint32_t(i * ry + (j + 1));
        auto const idx11 = std::uint32_t((i + 1) * ry + (j + 1));

        _tri.indices.insert(_tri.indices.end(), {
          idx00, idx10, idx01,
          idx01, idx10, idx11
          });

        rnu::vec3 const p00 = _tri.positions[idx00];
        rnu::vec3 const p01 = _tri.positions[idx01];
        rnu::vec3 const p10 = _tri.positions[idx10];
        rnu::vec3 const p11 = _tri.positions[idx11];

        _tri.normals[idx00] = rnu::cross(p10 - p00, p01 - p00);
        _tri.normals[idx11] = rnu::cross(p01 - p11, p10 - p11);
      }
    }

    auto const prx_a = _tri.positions[(rx - 1) * ry];
    auto const prx_b = _tri.positions[(rx - 1) * ry + 1];
    auto const prx_c = _tri.positions[(rx - 2) * ry];
    _tri.normals[(rx - 1) * ry] = rnu::cross(prx_b - prx_a, prx_c - prx_a);

    auto const pry_a = _tri.positions[ry - 1];
    auto const pry_b = _tri.positions[ry - 2];
    auto const pry_c = _tri.positions[2 * ry - 1];
    _tri.normals[ry - 1] = rnu::cross(pry_b - pry_a, pry_c - pry_a);
  }

  int _xpos = 0;
  rnu::triangulated_object_t _tri;
  std::shared_ptr<renderer_component> _renderer;
  std::shared_ptr<gev::game::texture> _diffuse;
  std::shared_ptr<gev::game::texture> _roughness;
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

    ImGui::DragFloat3("Position", owner()->local_transform.position.data(), 0.01);

    auto euler = gev::scenery::to_euler(owner()->local_transform.rotation);
    if (ImGui::DragFloat3("Orientation", euler.data(), 0.01)) {
      owner()->local_transform.rotation = gev::scenery::from_euler(euler);
    }

    ImGui::DragFloat3("Scale", owner()->local_transform.scale.data(), 0.01);

    auto const global = owner()->global_transform();
    ImGui::Text("Global:");
    ImGui::Text("Position: (%.3f, %.3f, %.3f)", global.position.x, global.position.y, global.position.z);
    ImGui::Text("Orientation: (%.3f, %.3f, %.3f, %.3f)", global.rotation.w, global.rotation.x, global.rotation.y, global.rotation.z);
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

class test01
{
public:
  vk::SampleCountFlagBits _render_samples = vk::SampleCountFlagBits::e4;

  test01() {
    gev::engine::get().start("Test 01", 1280, 720);

    gev::engine::get().services().register_service<gev::game::renderer>(_render_samples);
    gev::engine::get().services().register_service<gev::game::mesh_renderer>(_render_samples);
    gev::engine::get().services().register_service<gev::game::distance_field_generator>();
  }

  std::shared_ptr<gev::scenery::entity> _selected_entity;

  void draw_component_tree(std::span<std::shared_ptr<gev::scenery::entity> const> entities)
  {
    for (auto const& e : entities)
    {
      auto nc = e->get<test_component>();
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
      if (nc && (elem || e->children().empty()))
      {
        if (ImGui::Button("Play sound here"))
        {
          nc->play_sound();
        }
      }

      if (elem)
      {
        draw_component_tree(e->children());
        ImGui::TreePop();
      }
    }
  }

  int start()
  {
    auto mesh_renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
    auto distance_field_generator = gev::engine::get().services().resolve<gev::game::distance_field_generator>();

    _environment_pipeline_layout = gev::create_pipeline_layout(mesh_renderer->camera_set_layout());

    rnu::triangulated_object_t tri_mesh;
    tri_mesh.indices = {
      0, 1, 2,
      2, 1, 3
    };
    tri_mesh.positions = {
      { -5, 0, -5 },
      { -5, 0, 5 },
      { 5, 0, -5 },
      { 5, 0, 5 },
    };
    tri_mesh.normals = {
      {0, 1, 0},
      {0, 1, 0},
      {0, 1, 0},
      {0, 1, 0},
    };
    tri_mesh.texcoords = {
      {0, 0},
      {0, 1},
      {1, 0},
      {1, 1},
    };

    auto const sound = gev::engine::get().audio_host().load_file("res/sound.ogg");

    auto object0 = std::make_shared<gev::game::mesh>("res/bunny.obj");
    mesh_renderer->generate_distance_field(object0, *distance_field_generator, 48);
    auto object1 = std::make_shared<gev::game::mesh>(tri_mesh);
    mesh_renderer->generate_distance_field(object1, *distance_field_generator, 48);

    auto const texture_dirt = std::make_shared<gev::game::texture>("res/dirt.png");
    auto const texture_roughness = std::make_shared<gev::game::texture>("res/roughness.jpg");
    auto const texture_grass = std::make_shared<gev::game::texture>("res/grass.jpg");
    auto const texture_ground_roughness = std::make_shared<gev::game::texture>("res/ground_roughness.jpg");

    auto e = _entity_manager.instantiate();
    e->emplace<test_component>("Scene Root", sound);

    auto ch01 = _entity_manager.instantiate(e);
    ch01->emplace<test_component>("Child 01", sound);
    auto r = ch01->emplace<renderer_component>();
    r->set_mesh(object0);
    auto const mtl = std::make_shared<gev::game::material>();
    mtl->load_diffuse(texture_dirt);
    mtl->load_roughness(texture_roughness);
    r->set_material(mtl);

    auto ch02 = _entity_manager.instantiate(e);
    ch02->emplace<test_component>("Child 02", sound);
    auto r2 = ch02->emplace<renderer_component>();
    r2->set_mesh(object1);
    auto const mtl2 = std::make_shared<gev::game::material>();
    mtl2->load_diffuse(texture_grass);
    mtl2->load_roughness(texture_ground_roughness);
    r2->set_material(mtl2);

    auto ch03 = _entity_manager.instantiate(e);
    ch03->emplace<test_component>("Child 03", sound);
    ch03->emplace<renderer_component>();
    ch03->emplace<mg_component>();

    auto unnamed = _entity_manager.instantiate(e);
    auto e2 = _entity_manager.instantiate();
    e2->emplace<test_component>("Camera", sound);
    e2->emplace<camera_component>();
    e2->emplace<camera_controller>();
    e2->local_transform.position = rnu::vec3(0, 2, 5);

    auto och01 = _entity_manager.instantiate(e2);
    och01->emplace<test_component>("Other Child 01", sound);

    auto och02 = _entity_manager.instantiate(e2);
    och02->emplace<test_component>("Other Child 02", sound);
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
    mesh_renderer->set_samples(_render_samples);

    _environment_pipeline.reset();
    auto env_vertex_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_vert));
    auto env_fragment_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_frag));
    _environment_pipeline = gev::simple_pipeline_builder::get(_environment_pipeline_layout)
      .stage(vk::ShaderStageFlagBits::eVertex, env_vertex_shader)
      .stage(vk::ShaderStageFlagBits::eFragment, env_fragment_shader)
      .multisampling(_render_samples)
      .color_attachment(gev::engine::get().swapchain_format().surfaceFormat.format)
      .build();
  }

private:
  bool ui_in_loop(gev::frame const& frame)
  {
    auto renderer = gev::engine::get().services().resolve<gev::game::renderer>();

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
          glfwSetWindowMonitor(window, nullptr,
            _position_before_fullscreen.x, _position_before_fullscreen.y,
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
      if (nc) nc->ui();
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
    mesh_renderer->prepare_frame(frame);
    _entity_manager.apply_transform();
    _entity_manager.early_update();
    _entity_manager.update();
    _entity_manager.late_update();
    mesh_renderer->finalize_enqueues(frame);

    // ENVIRONMENT
    renderer->begin_render(frame, false);
    renderer->bind_pipeline(frame, _environment_pipeline.get(), _environment_pipeline_layout.get());
    mesh_renderer->get_camera()->bind(frame, _environment_pipeline_layout.get());
    frame.command_buffer.draw(3, 1, 0, 0);
    renderer->end_render(frame);

    // MESHES FROM render_component
    mesh_renderer->render(*renderer, frame);

    mesh_renderer->cleanup_frame();
    return true;
  }

  double _timer = 1.0;
  double _fps = 0.0;
  int _count = 0;

  vk::UniquePipelineLayout _environment_pipeline_layout;
  vk::UniquePipeline _environment_pipeline;

  bool _draw_ddfs = false;
  bool _fullscreen = false;
  rnu::vec2i _position_before_fullscreen = { 0, 0 };
  rnu::vec2i _size_before_fullscreen = { 0, 0 };

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
