#include "collision_masks.hpp"
#include "components/bone_component.hpp"
#include "components/camera_component.hpp"
#include "components/camera_controller_component.hpp"
#include "components/debug_ui_component.hpp"
#include "components/ground_component.hpp"
#include "components/remote_controller_component.hpp"
#include "components/renderer_component.hpp"
#include "components/shadow_map_component.hpp"
#include "components/skin_component.hpp"
#include "components/sound_component.hpp"
#include "entity_ids.hpp"
#include "environment_shader.hpp"
#include "gltf_loader.hpp"

#include <gev/audio/audio.hpp>
#include <gev/audio/playback.hpp>
#include <gev/audio/sound.hpp>
#include <gev/buffer.hpp>
#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/addition.hpp>
#include <gev/game/blur.hpp>
#include <gev/game/camera.hpp>
#include <gev/game/cutoff.hpp>
#include <gev/game/formats.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/game/render_target_2d.hpp>
#include <gev/game/renderer.hpp>
#include <gev/imgui/imgui.h>
#include <gev/imgui/imgui_extra.hpp>
#include <gev/per_frame.hpp>
#include <gev/pipeline.hpp>
#include <gev/scenery/collider.hpp>
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

class test01
{
public:
  test01()
  {
    gev::engine::get().start("Test 01", 1280, 720);
    gev::register_service<gev::game::renderer>(gev::engine::get().swapchain_size(), _render_samples);
    gev::register_service<gev::game::mesh_renderer>();
    gev::register_service<gev::game::shader_repo>();

    fill_img(_img[0], gev::engine::get().swapchain_size());
    fill_img(_img[1], gev::engine::get().swapchain_size());
    early_init();

    gev::engine::get().on_resized(
      [this](int w, int h)
      {
        gev::service<gev::game::renderer>()->set_render_size(vk::Extent2D(std::uint32_t(w), std::uint32_t(h)));
        fill_img(_img[0], {std::uint32_t(w), std::uint32_t(h)});
        fill_img(_img[1], {std::uint32_t(w), std::uint32_t(h)});
      });
  }

  void draw_component_tree(std::span<std::shared_ptr<gev::scenery::entity> const> entities)
  {
    for (auto const& e : entities)
    {
      auto nc = e->get<debug_ui_component>();

      ImGui::PushID(nc.get());
      auto const name = nc ? nc->name() : "<unnamed object>";

      int flags = _selected_entity.lock() == e ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;

      if (e->children().empty())
        flags = int(flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet);

      auto const elem = ImGui::TreeNodeEx(name.c_str(), flags | ImGuiTreeNodeFlags_OpenOnDoubleClick);
      if (ImGui::IsItemClicked())
      {
        if (_selected_entity.lock() == e)
          _selected_entity.reset();
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

  void init_start()
  {
    auto mesh_renderer = gev::service<gev::game::mesh_renderer>();
    auto entity_manager = gev::service<gev::scenery::entity_manager>();
    auto collision_system = gev::service<gev::scenery::collision_system>();
    auto shader_repo = gev::service<gev::game::shader_repo>();
    auto audio_host = gev::service<gev::audio::audio_host>();
    auto audio_repo = gev::service<gev::audio_repo>();
    shader_repo->emplace("ENVIRONMENT", std::make_shared<environment_shader>());

    audio_repo->emplace("sound.ogg", audio_host->load_file("res/sound.ogg"));

    // ################################################
    auto const player = entity_manager->instantiate(entities::player_entity);
    auto shape = std::make_shared<btCapsuleShape>(0.25f, 0.5f);
    player->emplace<gev::scenery::collider_component>(
      collision_system, std::move(shape), 10.0f, false, collisions::player, collisions::all ^ collisions::player);
    auto const ptcl = load_gltf_entity("res/ptcl/scene.gltf");
    entity_manager->reparent(ptcl, player);
    ptcl->local_transform.position.y = -0.5;
    player->local_transform.position.y = 0.5;
    player->emplace<debug_ui_component>("Player");
    player->emplace<remote_controller_component>()->own();
    // ################################################

    auto e = entity_manager->instantiate();
    e->emplace<debug_ui_component>("Scene Root");

    auto unnamed = entity_manager->instantiate(e);
    auto e2 = entity_manager->instantiate(entities::player_camera);
    e2->emplace<debug_ui_component>("Camera");
    e2->emplace<camera_component>();
    e2->emplace<camera_controller_component>();
    e2->emplace<sound_component>();
    e2->local_transform.position = rnu::vec3(4, 2, 5);
    e2->local_transform.rotation = rnu::look_at(e2->local_transform.position, rnu::vec3(0, 0, 0), rnu::vec3(0, 1, 0));

    auto ch03 = entity_manager->instantiate(e);
    ch03->emplace<debug_ui_component>("Ground Map");
    ch03->emplace<renderer_component>();
    ch03->emplace<ground_component>();
    auto ground = std::make_shared<btStaticPlaneShape>(btVector3{0, 1, 0}, 0.f);
    ch03->emplace<gev::scenery::collider_component>(
      collision_system, std::move(ground), 0.0f, true, collisions::scene, collisions::all ^ collisions::scene);

    auto ch04 = entity_manager->instantiate(e);
    ch04->emplace<debug_ui_component>("Object");
    auto rc04 = ch04->emplace<renderer_component>();
    rc04->set_mesh(std::make_shared<gev::game::mesh>("res/torus.obj"));
    rc04->set_material(std::make_shared<gev::game::material>());

    {
      auto collider = entity_manager->instantiate(e);
      collider->local_transform.position = {5, 0, 3};
      collider->emplace<debug_ui_component>("Collider Sphere");
      auto sphere_rnd = collider->emplace<renderer_component>();
      sphere_rnd->set_mesh(std::make_shared<gev::game::mesh>("res/sphere.obj"));
      auto const sphere_mat = std::make_shared<gev::game::material>();
      sphere_mat->set_diffuse({0.2f, 0.4f, 0.7f, 1.0f});
      sphere_rnd->set_material(sphere_mat);
      auto collision = std::make_shared<btSphereShape>(1.0f);
      collider->emplace<gev::scenery::collider_component>(
        collision_system, std::move(collision), 0.0f, true, collisions::scene, collisions::all ^ collisions::scene);
    }

    auto sm = entity_manager->instantiate(e);
    sm->emplace<debug_ui_component>("ShadowMap Camera");
    sm->emplace<shadow_map_component>();
    sm->local_transform.position = {0, 10, 10};
    sm->local_transform.rotation = rnu::quat(rnu::vec3(1, 0, 0), rnu::radians(-20));
  }

  int start()
  {
    init_start();
    return gev::engine::get().run([this](auto&& f) { return loop(f); });
  }

private:
  bool ui_in_loop(gev::frame const& frame, bool& skip_frame)
  {
    auto renderer = gev::service<gev::game::renderer>();
    auto mesh_renderer = gev::service<gev::game::mesh_renderer>();

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
      if (ImGui::Button("Reload Shaders"))
      {
        gev::engine::get().device().waitIdle();
        gev::service<gev::game::shader_repo>()->invalidate_all();
        skip_frame = true;
      }

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

      int aa = int(std::log2(int(_render_samples)));
      char const* arr[] = {"1x", "2x", "4x", "8x"};
      if (ImGui::Combo("AA", &aa, arr, std::size(arr)))
      {
        _render_samples = vk::SampleCountFlagBits(int(std::pow(2, aa)));
        gev::service<gev::game::renderer>()->set_samples(_render_samples);
      }

      auto const pmode = gev::engine::get().present_mode();
      auto const pmodes = gev::engine::get().present_modes();
      auto const iter = std::find(pmodes.begin(), pmodes.end(), pmode);
      int index = int(std::distance(pmodes.begin(), iter));
      if (ImGui::Combo(
            "Present Mode", &index,
            [](void* d, int i) -> char const*
            {
              switch (gev::engine::get().present_modes()[i])
              {
                case vk::PresentModeKHR::eFifo: return "FiFo";
                case vk::PresentModeKHR::eFifoRelaxed: return "Relaxed FiFo";
                case vk::PresentModeKHR::eImmediate: return "Immediate";
                case vk::PresentModeKHR::eMailbox: return "Mailbox";
                case vk::PresentModeKHR::eSharedContinuousRefresh: return "Shared Continuous Refresh";
                case vk::PresentModeKHR::eSharedDemandRefresh: return "Shared Demand Refresh";
              }
              return "<unknown>";
            },
            nullptr, int(pmodes.size())))
      {
        gev::engine::get().set_present_mode(pmodes[index]);
      }

      if (ImGui::Button("Close"))
      {
        ImGui::End();
        return false;
      }

      ImGui::BeginGroupPanel("Entities", ImVec2(ImGui::GetContentRegionAvail().x, 0.0));
      draw_component_tree(gev::service<gev::scenery::entity_manager>()->root_entities());
      ImGui::EndGroupPanel();
    }
    ImGui::End();

    if (_selected_entity.lock() && ImGui::Begin("Entity"))
    {
      auto nc = _selected_entity.lock()->get<debug_ui_component>();
      if (nc)
        nc->ui();
      ImGui::End();
    }
    return true;
  }

  bool loop(gev::frame const& frame)
  {
    auto mesh_renderer = gev::service<gev::game::mesh_renderer>();
    auto renderer = gev::service<gev::game::renderer>();
    auto shader_repo = gev::service<gev::game::shader_repo>();

    bool skip_frame = false;
    if (!ui_in_loop(frame, skip_frame))
      return false;
    if (skip_frame)
      return true;

    // PREPARE AND UPDATE SCENE
    renderer->prepare_frame(frame.command_buffer);
    mesh_renderer->try_flush(frame.command_buffer);

    // ENVIRONMENT
    renderer->begin_render(frame.command_buffer, false);
    auto const env = shader_repo->get("ENVIRONMENT");
    env->bind(frame.command_buffer, gev::game::pass_id::forward);
    auto const x = 0;
    auto const y = 0;
    auto const w = gev::engine::get().swapchain_size().width;
    auto const h = gev::engine::get().swapchain_size().height;
    frame.command_buffer.setViewport(0, vk::Viewport(float(x), float(y), float(w), float(h), 0.f, 1.f));
    frame.command_buffer.setScissor(0, vk::Rect2D({x, y}, {w, h}));
    frame.command_buffer.setRasterizationSamplesEXT(_render_samples);
    env->attach(frame.command_buffer, mesh_renderer->get_camera()->descriptor(), mesh_renderer->camera_set);
    frame.command_buffer.draw(3, 1, 0, 0);
    renderer->end_render(frame.command_buffer);

    // MESHES FROM render_component
    renderer->begin_render(frame.command_buffer, true);
    auto const size = gev::engine::get().swapchain_size();
    mesh_renderer->render(
      frame.command_buffer, 0, 0, size.width, size.height, gev::game::pass_id::forward, _render_samples);
    renderer->end_render(frame.command_buffer);

    renderer->resolve(frame.command_buffer);

    // BLOOM because why not
    auto& s = renderer->resolve_target();
    auto& a = *_img[0];
    auto& b = *_img[1];

    _cutoff->apply(frame.command_buffer, 1.0f, s, b);
    _blur->apply(frame.command_buffer, gev::game::blur_dir::horizontal, 3.2f, b, a);
    _blur->apply(frame.command_buffer, gev::game::blur_dir::vertical, 3.2f, a, b);
    _addition->apply(frame.command_buffer, 0.53f, s, b, a);

    frame.present_image(*a.image());
    return true;
  }

  double _timer = 1.0;
  double _fps = 0.0;
  int _count = 0;

  vk::SampleCountFlagBits _render_samples = vk::SampleCountFlagBits::e4;
  std::weak_ptr<gev::scenery::entity> _selected_entity;

  bool _fullscreen = false;
  rnu::vec2i _position_before_fullscreen = {0, 0};
  rnu::vec2i _size_before_fullscreen = {0, 0};

  std::unique_ptr<gev::game::render_target_2d> _img[2];

  std::unique_ptr<gev::game::cutoff> _cutoff;
  std::unique_ptr<gev::game::blur> _blur;
  std::unique_ptr<gev::game::addition> _addition;

  void early_init()
  {
    _cutoff = std::make_unique<gev::game::cutoff>();
    _blur = std::make_unique<gev::game::blur>();
    _addition = std::make_unique<gev::game::addition>();
  }

  void fill_img(std::unique_ptr<gev::game::render_target_2d>& i, vk::Extent2D size)
  {
    i = std::make_unique<gev::game::render_target_2d>(size, gev::game::formats::forward_pass,
      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
  }
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
