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
#include "environment.hpp"
#include "environment_shader.hpp"
#include "gltf_loader.hpp"
#include "main_controls.hpp"
#include "post_process.hpp"
#include "registry.hpp"

#include <bullet/BulletWorldImporter/btBulletWorldImporter.h>
#include <bullet/btBulletCollisionCommon.h>
#include <bullet/btBulletDynamicsCommon.h>
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
#include <print>
#include <random>
#include <rnu/algorithm/hash.hpp>
#include <rnu/algorithm/smooth.hpp>
#include <rnu/camera.hpp>
#include <rnu/font/character_ranges.hpp>
#include <rnu/font/font.hpp>
#include <rnu/font/sdf_font.hpp>
#include <rnu/math/math.hpp>
#include <rnu/obj.hpp>
#include <sstream>
#include <stacktrace>
#include <test_shaders_files.hpp>

// MY UI
struct vertex2d
{
  rnu::vec2 position;
  rnu::vec2 uv;
  rnu::vec4ui8 tint;
};

rnu::vec4ui8 pack_vec4(rnu::vec4 v)
{
  return rnu::vec4ui8(clamp(v * 255.f, 0.f, 255.f));
}

class ui_shader : public gev::game::shader
{
public:
  struct options
  {
    rnu::vec2 size;
    int use_texture;
    int is_sdf;
  };

protected:
  vk::UniquePipelineLayout rebuild_layout()
  {
    vk::PushConstantRange constants[] = {
      {vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(options)},
    };

    _set_layout =
      gev::descriptor_layout_creator::get()
        .bind(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment,
          vk::DescriptorBindingFlagBits::ePartiallyBound)
        .flags(vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR)
        .build();
    return gev::create_pipeline_layout(_set_layout.get(), constants);
  }

  vk::UniquePipeline rebuild(gev::game::pass_id pass)
  {
    auto const vert = gev::create_shader(gev::load_spv(test_shaders::shaders::ui_vert));
    auto const frag = gev::create_shader(gev::load_spv(test_shaders::shaders::ui_frag));
    return gev::simple_pipeline_builder::get(layout())
      .color_attachment(gev::engine::get().swapchain_format().surfaceFormat.format,
        gev::blend_attachment(gev::default_blending::alpha_blending))
      .attribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(vertex2d, position))
      .attribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(vertex2d, uv))
      .attribute(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(vertex2d, tint))
      .binding(0, sizeof(vertex2d))
      .multisampling(vk::SampleCountFlagBits::e1)
      .cull(vk::CullModeFlagBits::eNone)
      .stage(vk::ShaderStageFlagBits::eVertex, vert)
      .stage(vk::ShaderStageFlagBits::eFragment, frag)
      .build();
  }

public:
  vk::UniqueDescriptorSetLayout _set_layout;
};

class mesh2d
{
public:
  mesh2d() = default;

  void load_vertices(vk::ArrayProxy<vertex2d const> vertices)
  {
    _num_vertices = vertices.size();
    if (!_vertex_buffer || vertices.size() * sizeof(vertex2d) > _vertex_buffer->size())
    {
      _vertex_buffer =
        gev::buffer::host_accessible(vertices.size() * sizeof(vertex2d), vk::BufferUsageFlagBits::eVertexBuffer);
    }
    _vertex_buffer->load_data<vertex2d>(vertices);
  }

  void load_indices(vk::ArrayProxy<std::uint16_t const> indices)
  {
    _num_indices = indices.size();
    if (!_index_buffer || indices.size() * sizeof(std::uint16_t) > _index_buffer->size())
    {
      _index_buffer =
        gev::buffer::host_accessible(indices.size() * sizeof(std::uint16_t), vk::BufferUsageFlagBits::eIndexBuffer);
    }
    _index_buffer->load_data<std::uint16_t>(indices);
  }

  void render(vk::CommandBuffer c, gev::game::shader& shader, vk::Extent2D size)
  {
    ui_shader::options options{{float(size.width), float(size.height)}};
    auto tex = texture();
    options.use_texture = tex ? 1 : 0;
    options.is_sdf = _is_sdf ? 1 : 0;
    c.pushConstants<decltype(options)>(shader.layout(), vk::ShaderStageFlagBits::eAllGraphics, 0, options);

    if (tex)
    {
      vk::DescriptorImageInfo image;
      image.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImageView(tex->view())
        .setSampler(tex->sampler());
      vk::WriteDescriptorSet write;
      write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDstSet(nullptr)
        .setDstBinding(0)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setImageInfo(image);
      c.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, shader.layout(), 0u, write);
    }

    c.bindVertexBuffers(0, _vertex_buffer->get_buffer(), 0ull);
    if (_index_buffer)
    {
      c.bindIndexBuffer(_index_buffer->get_buffer(), 0, vk::IndexType::eUint16);
      c.drawIndexed(_num_indices, 1, 0, 0, 0);
    }
    else
    {
      c.draw(_num_vertices, 1, 0, 0);
    }
  }

  void set_texture(std::shared_ptr<gev::game::texture> tex, bool is_sdf = false)
  {
    _texture = std::move(tex);
    _is_sdf = is_sdf;
  }

  std::shared_ptr<gev::game::texture> texture() const
  {
    return _texture;
  }

private:
  std::shared_ptr<gev::buffer> _vertex_buffer;
  std::shared_ptr<gev::buffer> _index_buffer;
  std::shared_ptr<gev::game::texture> _texture;
  std::size_t _num_indices = 0;
  std::size_t _num_vertices = 0;
  bool _is_sdf = false;
};

class panel
{
public:
  panel(float x, float y, float w, float h, rnu::vec4 color)
  {
    _vertices.insert(_vertices.end(),
      {
        vertex2d{{x, y}, {0, 0}, pack_vec4(color)},
        vertex2d{{x, y + h}, {0, 1}, pack_vec4(color)},
        vertex2d{{x + w, y}, {1, 0}, pack_vec4(color)},
        vertex2d{{x + w, y + h}, {1, 1}, pack_vec4(color)},
      });
    _indices.insert(_indices.end(), {0, 2, 1, 1, 2, 3});

    _mesh.load_indices(_indices);
    _mesh.load_vertices(_vertices);

    _box = rnu::rect2f();
    _box.position = {x, y};
    _box.size = {w, h};
  }

  void set_color(rnu::vec4 color)
  {
    for (auto& v : _vertices)
      v.tint = pack_vec4(color);

    _mesh.load_vertices(_vertices);
  }

  rnu::rect2f bounds() const
  {
    return _box;
  }

  void render(vk::CommandBuffer c, gev::game::shader& shader, vk::Extent2D size)
  {
    // vk::Rect2D scissor;
    // scissor.offset = vk::Offset2D(std::int32_t(_box.position.x), std::int32_t(_box.position.y));
    // scissor.extent = vk::Extent2D(std::uint32_t(_box.size.x), std::uint32_t(_box.size.y));
    // c.setScissor(0, vk::Rect2D(scissor));

    _mesh.render(c, shader, size);
  }

  int z_index() const
  {
    return _z_index;
  }

  void set_texture(std::shared_ptr<gev::game::texture> tex)
  {
    _mesh.set_texture(std::move(tex));
  }

  std::shared_ptr<gev::game::texture> texture() const
  {
    return _mesh.texture();
  }

private:
  int _z_index = 0;

  mesh2d _mesh;
  rnu::rect2f _box;

  std::vector<vertex2d> _vertices;
  std::vector<std::uint16_t> _indices;
};

class font
{
public:
  font(gev::serializer& ser, std::filesystem::path const& path, int atlas_width = 512, float base_size = 25.0f)
  {
    _font = std::make_unique<rnu::font>(path);

    std::vector ranges = {rnu::character_ranges::basic_latin, rnu::character_ranges::c1_controls_and_latin_1_supplement,
      rnu::character_ranges::currency_symbols};
    _sdf_font = std::make_unique<rnu::sdf_font>(atlas_width, base_size, 8.f, *_font, ranges);

    _sdf_font_atlas = as<gev::game::texture>(ser.initial_load("font_atlas.gevas",
      [&]
      {
        int atlas_w, atlas_h;
        std::vector<std::uint8_t> data;
        _sdf_font->dump(data, atlas_w, atlas_h);
        return std::make_shared<gev::game::texture>(gev::game::color_scheme::grayscale, data, atlas_w, atlas_h, 1);
      }));
  }

  rnu::sdf_font const& sdf_font() const
  {
    return *_sdf_font;
  }

  std::shared_ptr<gev::game::texture> const& atlas() const
  {
    return _sdf_font_atlas;
  }

  float scale(float desired_size) const
  {
    return desired_size / _sdf_font->base_size();
  }

  float base_size() const
  {
    return _sdf_font->base_size();
  }

private:
  std::unique_ptr<rnu::font> _font;
  std::unique_ptr<rnu::sdf_font> _sdf_font;
  std::shared_ptr<gev::game::texture> _sdf_font_atlas;
};

class ui_renderer
{
public:
  ui_renderer()
  {
    _shader = gev::service<gev::game::shader_repo>()->get("UI");
    _default_texture = std::make_shared<gev::game::texture>("res/one.png");
  }

  void sort_panels()
  {
    std::stable_sort(_panels.begin(), _panels.end(),
      [](auto const& a, auto const& b)
      {
        auto const la = a.lock();
        auto const lb = b.lock();

        auto const za = la ? la->z_index() : 100000;
        auto const zb = lb ? lb->z_index() : 100000;

        return za < zb;
      });
  }

  void begin_rendering(vk::CommandBuffer c, gev::game::render_target_2d& dst)
  {
    dst.image()->layout(c, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentWrite);
    vk::RenderingAttachmentInfo attachment;
    attachment.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .setImageView(dst.view())
      .setLoadOp(vk::AttachmentLoadOp::eLoad)
      .setStoreOp(vk::AttachmentStoreOp::eStore);
    vk::RenderingInfo ui_rendering;
    ui_rendering.setColorAttachments(attachment)
      .setLayerCount(1)
      .setRenderArea(vk::Rect2D({0, 0}, gev::engine::get().swapchain_size()));

    c.beginRendering(ui_rendering);

    _shader->bind(c, gev::game::pass_id::forward);

    vk::DescriptorImageInfo image;
    image.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
      .setImageView(_default_texture->view())
      .setSampler(_default_texture->sampler());
    vk::WriteDescriptorSet write;
    write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
      .setDstSet(nullptr)
      .setDstBinding(0)
      .setDstArrayElement(0)
      .setDescriptorCount(1)
      .setImageInfo(image);
    c.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, _shader->layout(), 0u, write);
  }

  std::shared_ptr<gev::game::shader> const& shader() const
  {
    return _shader;
  }

  void end_rendering(vk::CommandBuffer c)
  {
    c.endRendering();
  }

private:
  std::shared_ptr<gev::game::shader> _shader;
  std::vector<std::weak_ptr<panel>> _panels;
  std::shared_ptr<gev::game::texture> _default_texture;
};

void register_all_types(gev::serializer& s)
{
#define reg_one(Name) s.register_type<Name>(#Name);
  reg_one(gev::game::mesh);
  reg_one(gev::game::texture);
  reg_one(gev::game::material);
  reg_one(gev::scenery::entity);
  reg_one(gev::scenery::collision_shape);
  reg_one(gev::scenery::collider_component);

  reg_one(gev::scenery::joint_animation);
  reg_one(gev::scenery::animation);
  reg_one(gev::scenery::transform_tree);
  reg_one(gev::scenery::skin);

  reg_one(bone_component);
  reg_one(camera_component);
  reg_one(camera_controller_component);
  reg_one(debug_ui_component);
  reg_one(ground_component);
  reg_one(remote_controller_component);
  reg_one(shadow_map_component);
  reg_one(skin_component);
  reg_one(renderer_component);
  reg_one(sound_component);
#undef reg_one
}

class test01
{
public:
  test01()
  {
    gev::engine::get().start("Test 01", 1280, 720);
    gev::register_service<gev::game::renderer>(gev::engine::get().swapchain_size(), _render_samples);
    auto const renderer = gev::register_service<gev::game::mesh_renderer>();
    auto const shadow_map_holder = gev::register_service<gev::game::shadow_map_holder>();
    renderer->set_shadow_maps(shadow_map_holder->descriptor());
    gev::register_service<main_controls>();
    auto shader_repo = gev::register_service<gev::game::shader_repo>();
    auto const serializer = gev::register_service<gev::serializer>();
    register_all_types(*serializer);

    _environment = std::make_shared<environment>();
    _post_process = std::make_shared<post_process>(vk::Format::eR16G16B16A16Sfloat);

    shader_repo->emplace("UI", std::make_shared<ui_shader>());
    _font = std::make_unique<font>(*serializer, "res/Poppins-Regular.ttf", 512, 16.0f);

    float xm = 0.0f;
    float ym = 0.0f;
    auto const glyphs = _font->sdf_font().text_set(L"Hello, World! (&Änderungen§)", nullptr, &xm, &ym);

    rnu::vec4 color(rnu::vec3(1.f), 1.0f);
    std::vector<vertex2d> vertices;
    std::vector<std::uint16_t> indices;
    float scale = _font->scale(15.0f);

    float x_max = xm * scale;
    float y_max = ym * scale;
    for (auto const& g : glyphs)
    {
      float x = g.bounds.position.x * scale + 50;
      float y = 50 - g.bounds.position.y * scale - g.bounds.size.y * scale;
      //-g.bounds.size.y;
      float w = g.bounds.size.x * scale;
      float h = g.bounds.size.y * scale;

      float uvx = g.uvs.position.x;
      float uvy = g.uvs.position.y;
      float uvw = g.uvs.size.x;
      float uvh = g.uvs.size.y;

      auto const v0 = std::uint16_t(vertices.size());
      vertices.insert(vertices.end(),
        {
          vertex2d{{x, y}, {uvx, uvy + uvh}, pack_vec4(color)},
          vertex2d{{x, y + h}, {uvx, uvy}, pack_vec4(color)},
          vertex2d{{x + w, y}, {uvx + uvw, uvy + uvh}, pack_vec4(color)},
          vertex2d{{x + w, y + h}, {uvx + uvw, uvy}, pack_vec4(color)},
        });

      auto const v1 = std::uint16_t(v0 + 1);
      auto const v2 = std::uint16_t(v0 + 2);
      auto const v3 = std::uint16_t(v0 + 3);
      indices.insert(indices.end(), {v0, v2, v1, v1, v2, v3});
    }
    _text_mesh.load_indices(indices);
    _text_mesh.load_vertices(vertices);
    _text_mesh.set_texture(_font->atlas(), true);

    _ui = std::make_shared<ui_renderer>();

    _panels.push_back(std::make_shared<panel>(13.f, 14.f, 600.f, 50.f, rnu::vec4(0, 0, 0, 0.3)));
    _panels.push_back(std::make_shared<panel>(50.f, 50.f, 600.f, 50.f, rnu::vec4(0.95, 0.95, 0.95, 1)));
    _panels.push_back(std::make_shared<panel>(50.f, 50.f, x_max, y_max, rnu::vec4(0.2, 0.2, 0.2, 0.8)));

    gev::engine::get().on_resized([this](int w, int h)
      { gev::service<gev::game::renderer>()->set_render_size(vk::Extent2D(std::uint32_t(w), std::uint32_t(h))); });
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
    auto const serializer = gev::service<gev::serializer>();
    auto mesh_renderer = gev::service<gev::game::mesh_renderer>();
    auto entity_manager = gev::service<gev::scenery::entity_manager>();
    auto collision_system = gev::service<gev::scenery::collision_system>();
    auto shader_repo = gev::service<gev::game::shader_repo>();
    auto audio_host = gev::service<gev::audio::audio_host>();
    auto audio_repo = gev::service<gev::audio_repo>();

    auto const torus =
      serializer->initial_load("torus.gevas", [] { return std::make_shared<gev::game::mesh>("res/torus.obj"); });
    auto const grass =
      serializer->initial_load("grass.gevas", [] { return std::make_shared<gev::game::texture>("res/grass.jpg"); });
    auto const sphere =
      serializer->initial_load("sphere.gevas", [] { return std::make_shared<gev::game::mesh>("res/sphere.obj"); });
    auto const torus_material = serializer->initial_load("torus_material.gevas",
      [&]
      {
        auto torus_mat = std::make_shared<gev::game::material>();
        torus_mat->load_diffuse(as<gev::game::texture>(grass));
        return torus_mat;
      });

    audio_repo->emplace("sound.ogg", audio_host->load_file("res/sound.ogg"));

    serializer->init(); 
    
    // ################################################
    auto player = as<gev::scenery::entity>(serializer->initial_load("player_object.gevas",
      [&]
      {
        auto const player = entity_manager->instantiate(entities::player_entity);
        auto shape = std::make_shared<gev::scenery::collision_shape>();
        shape->set(gev::scenery::capsule_shape{0.25f, 0.5f});
        player->emplace<gev::scenery::collider_component>(
          std::move(shape), 10.0f, false, collisions::player, collisions::all ^ collisions::player);
        auto const ptcl = load_gltf_entity("res/ptcl/scene.gltf");
        entity_manager->reparent(ptcl, player);
        ptcl->local_transform.position.y = -0.5;
        player->local_transform.position.y = 0.5;
        player->emplace<debug_ui_component>("Player");
        player->emplace<remote_controller_component>()->own();
        return player;
      }));
    entity_manager->add(player);
    entity_manager->reparent(player);
    // ################################################

    auto e = entity_manager->instantiate();
    e->emplace<debug_ui_component>("Scene Root");

    auto e2 = as<gev::scenery::entity>(serializer->initial_load("main_camera.gevas",
      [&]
      {
        auto c = entity_manager->instantiate(entities::player_camera);
        c->emplace<debug_ui_component>("Camera");
        c->emplace<camera_component>();
        c->emplace<camera_controller_component>();
        c->emplace<sound_component>();
        c->local_transform.position = rnu::vec3(4, 2, 5);
        c->local_transform.rotation = rnu::look_at(c->local_transform.position, rnu::vec3(0, 0, 0), rnu::vec3(0, 1, 0));
        return c;
      }));
    entity_manager->add(e2);

    auto ch03 = as<gev::scenery::entity>(serializer->initial_load("ground.gevas",
      [&]
      {
        auto ch03 = entity_manager->instantiate();
        ch03->emplace<debug_ui_component>("Ground Map");
        ch03->emplace<renderer_component>();
        ch03->emplace<ground_component>();
        auto ground = std::make_shared<gev::scenery::collision_shape>();
        ch03->emplace<gev::scenery::collider_component>(
          std::move(ground), 0.0f, true, collisions::scene, collisions::all ^ collisions::scene);
        return ch03;
      }));
    entity_manager->add(ch03);

    auto ch04 = as<gev::scenery::entity>(serializer->initial_load("torus_object.gevas",
      [&]
      {
        auto c = entity_manager->instantiate();
        c->emplace<debug_ui_component>("Object");
        auto rend = c->emplace<renderer_component>();
        rend->set_mesh(as<gev::game::mesh>(torus));
        rend->set_material(as<gev::game::material>(torus_material));
        return c;
      }));
    entity_manager->add(ch04, e);

    auto const sphere_prefab = as<gev::scenery::entity>(serializer->initial_load("sphere_prefab.gevas", [&]{
      auto collider = entity_manager->instantiate();
      collider->local_transform.position = {5, 0, 3};
      collider->emplace<debug_ui_component>("Collider Sphere");
      auto sphere_rnd = collider->emplace<renderer_component>();
      sphere_rnd->set_mesh(as<gev::game::mesh>(sphere));
      auto const sphere_mat = std::make_shared<gev::game::material>();
      sphere_mat->set_diffuse({0.2f, 0.4f, 0.7f, 1.0f});
      sphere_rnd->set_material(sphere_mat);
      auto collision = std::make_shared<gev::scenery::collision_shape>();
      collision->set(gev::scenery::sphere_shape{1.0f});
      collider->emplace<gev::scenery::collider_component>(
        std::move(collision), 0.0f, true, collisions::scene, collisions::all ^ collisions::scene);
      return collider;
      }));
    entity_manager->add(sphere_prefab, e);

    auto const sm = as<gev::scenery::entity>(serializer->initial_load("shadow_light_object.gevas",
      [&]
      {
        auto sm = entity_manager->instantiate();
        sm->emplace<debug_ui_component>("ShadowMap Camera");
        sm->emplace<shadow_map_component>();
        sm->local_transform.position = {5, 10, 10};
        sm->local_transform.rotation = rnu::look_at(sm->local_transform.position, rnu::vec3(0), rnu::vec3(0, 1, 0));
        return sm;
      }));
    entity_manager->add(sm, e);
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

      if (ImGui::Button("Crash"))
      {
        std::println("{}", std::stacktrace::current());
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
    auto const mesh_renderer = gev::service<gev::game::mesh_renderer>();
    auto const renderer = gev::service<gev::game::renderer>();
    auto const shadow_map_holder = gev::service<gev::game::shadow_map_holder>();
    auto const controls = gev::service<main_controls>();
    auto const size = gev::engine::get().swapchain_size();

    bool skip_frame = false;
    if (!ui_in_loop(frame, skip_frame))
      return false;
    if (skip_frame)
      return true;

    // PREPARE AND UPDATE SCENE
    renderer->prepare_frame(frame.command_buffer);
    shadow_map_holder->sync(frame.command_buffer);
    mesh_renderer->sync(frame.command_buffer);
    controls->main_camera->sync(frame.command_buffer);

    // ENVIRONMENT
    _environment->render(frame.command_buffer, 0, 0, size.width, size.height);

    // MESHES FROM render_component
    renderer->begin_render(frame.command_buffer, true);
    mesh_renderer->render(frame.command_buffer, *controls->main_camera, 0, 0, size.width, size.height,
      gev::game::pass_id::forward, _render_samples);
    renderer->end_render(frame.command_buffer);

    renderer->resolve(frame.command_buffer);

    // POST PROCESSING
    auto const& pp = _post_process->process(frame.command_buffer, renderer->resolve_target());
    frame.present_image(*pp.image());

    // SOME UI
    double cx, cy;
    glfwGetCursorPos(gev::engine::get().window(), &cx, &cy);
    if (_panels[1]->bounds().contains(rnu::vec2(cx, cy)))
    {
      _panels[1]->set_color({1, 0.6, 0.4, 1});
    }
    else
    {
      _panels[1]->set_color({1, 1, 1, 1});
    }

    gev::game::render_target_2d tmp_target(frame.output_image, frame.output_view);
    _ui->begin_rendering(frame.command_buffer, tmp_target);

    for (auto const& p : _panels)
      p->render(frame.command_buffer, *_ui->shader(), size);

    _text_mesh.render(frame.command_buffer, *_ui->shader(), size);

    _ui->end_rendering(frame.command_buffer);
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

  std::shared_ptr<environment> _environment;
  std::shared_ptr<post_process> _post_process;

  std::shared_ptr<ui_renderer> _ui;
  std::vector<std::shared_ptr<panel>> _panels;
  std::unique_ptr<font> _font;
  mesh2d _text_mesh;
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
