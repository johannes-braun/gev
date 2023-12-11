#include "camera.hpp"
#include "object.hpp"
#include "texture.hpp"
#include "ddf_generator.hpp"

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

#include <rnu/math/math.hpp>
#include <rnu/obj.hpp>
#include <rnu/camera.hpp>
#include <rnu/algorithm/smooth.hpp>

#include <test_shaders_files.hpp>

struct render_attachments
{
  std::shared_ptr<gev::image> msaa_image;
  vk::UniqueImageView msaa_view;
  std::shared_ptr<gev::image> depth_image;
  vk::UniqueImageView depth_view;
};

class ddf_binder
{
public:
  ddf_binder()
  {
    _device_fields = std::make_unique<gev::buffer>(
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive)
      .setSize(256 * sizeof(distance_field))
      .setUsage(vk::BufferUsageFlagBits::eStorageBuffer));

    _host_fields.resize(256);
    _device_fields->load_data<distance_field>(_host_fields);

    _ddf_layout = gev::descriptor_layout_creator::get()
      .bind(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment)
      .bind(1, vk::DescriptorType::eCombinedImageSampler, 256, vk::ShaderStageFlagBits::eFragment, vk::DescriptorBindingFlagBits::ePartiallyBound)
      .build();

    _descriptor = gev::engine::get().get_descriptor_allocator().allocate(_ddf_layout.get());
    gev::update_descriptor(_descriptor, 0, vk::DescriptorBufferInfo()
      .setBuffer(_device_fields->get_buffer())
      .setOffset(0)
      .setRange(_device_fields->size()), vk::DescriptorType::eStorageBuffer);
  }

  void add(ddf const& d)
  {
    auto const index = _current_field++;
    auto& field = _host_fields[index];
    field.bounds_min = rnu::vec4(d.bounds().lower(), 1);
    field.bounds_max = rnu::vec4(d.bounds().upper(), 1);
    field.field_index = index;

    _host_fields[index + 1].field_index = -1;

    _device_fields->load_data<distance_field>(_host_fields);

    gev::update_descriptor(_descriptor, 1, vk::DescriptorImageInfo()
      .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
      .setImageView(d.image_view())
      .setSampler(d.sampler()), vk::DescriptorType::eCombinedImageSampler,
      index);
  }

  void bind(vk::CommandBuffer c, vk::PipelineLayout layout, int set_index)
  {
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, set_index, _descriptor, nullptr);
  }

  vk::DescriptorSetLayout layout() const
  {
    return *_ddf_layout;
  }

  vk::DescriptorSet descriptor() const
  {
    return _descriptor;
  }

private:
  struct distance_field
  {
    rnu::vec4 bounds_min;
    rnu::vec4 bounds_max;
    int field_index = -1;

    int _p0;
    int _p1;
    int _p2;
  };

  vk::UniqueDescriptorSetLayout _ddf_layout;
  vk::DescriptorSet _descriptor;
  std::unique_ptr<gev::buffer> _device_fields;
  std::vector<distance_field> _host_fields;
  int _current_field = 0;
};

class simple_material
{
public:
  simple_material(vk::DescriptorSetLayout layout)
  {
    _texture_descriptor = gev::engine::get().get_descriptor_allocator().allocate(layout);
  }

  void load_diffuse(std::shared_ptr<texture> dt)
  {
    _diffuse = std::move(dt);
    _diffuse->bind(_texture_descriptor, 0);
  }

  void load_roughness(std::shared_ptr<texture> rt)
  {
    _roughness = std::move(rt);
    _roughness->bind(_texture_descriptor, 1);
  }

  void bind(vk::CommandBuffer c, vk::PipelineLayout layout, std::uint32_t index)
  {
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, index, _texture_descriptor, nullptr);
  }

private:
  vk::DescriptorSet _texture_descriptor;
  std::shared_ptr<texture> _diffuse;
  std::shared_ptr<texture> _roughness;
};

struct entity
{
  std::shared_ptr<object> obj;
  std::shared_ptr<simple_material> mtl;
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
    {
      if (!_playback->is_playing())
      {
        _playback.reset();
      }
    }
  }

  void play_sound()
  {
    _playback = _sound->open_playback();
    _playback->play();
  }

  void ui()
  {
    ImGui::Text("Name: %s", _name.c_str());

    ImGui::InputFloat3("Position", owner()->local_transform.position.data());
    ImGui::InputFloat4("Orientation", owner()->local_transform.rotation.data());
    ImGui::InputFloat3("Scale", owner()->local_transform.scale.data());

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
  struct stopper {
    ~stopper() {
      gev::engine::reset();
    }
  } stop;

  std::shared_ptr<gev::audio::sound> _sound;

  test01() {
    gev::engine::get().on_resized([this](auto w, auto h) {
      _per_index.reset(); });
    gev::engine::get().start("Test 01", 1280, 720);

    _sound = gev::engine::get().audio_host().load_file("res/sound.ogg");

    auto e = _entity_manager.instantiate();
    e->emplace<test_component>("Scene Root", _sound);
    auto ch01 = _entity_manager.instantiate(e);
    ch01->emplace<test_component>("Child 01", _sound);
    auto ch02 = _entity_manager.instantiate(e);
    ch02->emplace<test_component>("Child 02", _sound);
    auto unnamed = _entity_manager.instantiate(e);
    auto e2 = _entity_manager.instantiate();
    e2->emplace<test_component>("Other Root", _sound);
    auto och01 = _entity_manager.instantiate(e2);
    och01->emplace<test_component>("Other Child 01", _sound);
    auto och02 = _entity_manager.instantiate(e2);
    och02->emplace<test_component>("Other Child 02", _sound);
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

      if(elem)
      {
        draw_component_tree(e->children());
        ImGui::TreePop();
      }
    }
  }

  int start()
  {
    _per_index.set_generator([&](int i) {
      auto const size = gev::engine::get().swapchain_size();
      render_attachments idx;
      idx.depth_image = std::make_shared<gev::image>(vk::ImageCreateInfo()
        .setFormat(gev::engine::get().depth_format())
        .setArrayLayers(1)
        .setExtent({ size.width, size.height, 1 })
        .setImageType(vk::ImageType::e2D)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setMipLevels(1)
        .setSamples(vk::SampleCountFlagBits::e4)
        .setTiling(vk::ImageTiling::eOptimal)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment));
      idx.depth_view = idx.depth_image->create_view(vk::ImageViewType::e2D);

      idx.msaa_image = std::make_shared<gev::image>(vk::ImageCreateInfo()
        .setFormat(gev::engine::get().swapchain_format().surfaceFormat.format)
        .setArrayLayers(1)
        .setExtent({ size.width, size.height, 1 })
        .setImageType(vk::ImageType::e2D)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setMipLevels(1)
        .setSamples(vk::SampleCountFlagBits::e4)
        .setTiling(vk::ImageTiling::eOptimal)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setUsage(vk::ImageUsageFlagBits::eColorAttachment));
      idx.msaa_view = idx.msaa_image->create_view(vk::ImageViewType::e2D);
      return idx;
      });

    _camera = std::make_unique<camera>();

    _ddf_binder = std::make_unique<ddf_binder>();

    _material_set_layout = gev::descriptor_layout_creator::get()
      .bind(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAllGraphics)
      .bind(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAllGraphics)
      .build();
    _main_pipeline_layout = gev::create_pipeline_layout({
      _camera->get_descriptor_set_layout(),
      _material_set_layout.get(),
      _ddf_binder->layout(),
      });
    _environment_pipeline_layout = gev::create_pipeline_layout(_camera->get_descriptor_set_layout());

    {
      auto& e = _entities.emplace_back();
      e.obj = std::make_shared<object>("res/bunny.obj");
      e.mtl = std::make_shared<simple_material>(*_material_set_layout);
      e.mtl->load_diffuse(std::make_unique<texture>("res/dirt.png"));
      e.mtl->load_roughness(std::make_unique<texture>("res/roughness.jpg"));
    }

    {
      auto& e = _entities.emplace_back();
      e.obj = std::make_shared<object>("res/torus.obj");
      e.mtl = std::make_shared<simple_material>(*_material_set_layout);
      e.mtl->load_diffuse(std::make_unique<texture>("res/dirt.png"));
      e.mtl->load_roughness(std::make_unique<texture>("res/ground_roughness.jpg"));
    }

    {
      auto& e = _entities.emplace_back();
      e.obj = std::make_shared<object>("res/plane.obj");
      e.mtl = std::make_shared<simple_material>(*_material_set_layout);
      e.mtl->load_diffuse(std::make_unique<texture>("res/grass.jpg"));
      e.mtl->load_roughness(std::make_unique<texture>("res/ground_roughness.jpg"));
    }

    _ddf_gen = std::make_unique<ddf_generator>(_camera->get_descriptor_set_layout());
    for (auto const& e : _entities)
    {
      _ddf_binder->add(*_ddfs.emplace_back(_ddf_gen->generate(*e.obj, 32)));
    }

    create_pipelines();

    _entity_manager.spawn();

    return gev::engine::get().run([this](auto&& f) { return loop(f); });
  }

  void create_pipelines()
  {
    _main_pipeline.reset();
    _environment_pipeline.reset();

    _ddf_gen->recreate_pipelines();

    auto vertex_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::shader_vert));
    auto fragment_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::shader_frag));
    _main_pipeline = gev::simple_pipeline_builder::get(_main_pipeline_layout)
      .stage(vk::ShaderStageFlagBits::eVertex, vertex_shader)
      .stage(vk::ShaderStageFlagBits::eFragment, fragment_shader)
      .attribute(0, 0, vk::Format::eR32G32B32Sfloat)
      .attribute(1, 1, vk::Format::eR32G32B32Sfloat)
      .attribute(2, 2, vk::Format::eR32G32Sfloat)
      .binding(0, sizeof(rnu::vec4))
      .binding(1, sizeof(rnu::vec3))
      .binding(2, sizeof(rnu::vec2))
      .multisampling(vk::SampleCountFlagBits::e4)
      .color_attachment(gev::engine::get().swapchain_format().surfaceFormat.format)
      .depth_attachment(gev::engine::get().depth_format())
      .stencil_attachment(gev::engine::get().depth_format())
      .build();

    auto env_vertex_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_vert));
    auto env_fragment_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_frag));
    _environment_pipeline = gev::simple_pipeline_builder::get(_environment_pipeline_layout)
      .stage(vk::ShaderStageFlagBits::eVertex, env_vertex_shader)
      .stage(vk::ShaderStageFlagBits::eFragment, env_fragment_shader)
      .multisampling(vk::SampleCountFlagBits::e4)
      .color_attachment(gev::engine::get().swapchain_format().surfaceFormat.format)
      .build();
  }

private:
  bool loop(gev::frame const& frame)
  {
    _timer += frame.delta_time;
    _count++;
    if (_timer >= 0.3)
    {
      _fps = _count / _timer;
      _count = 0;
      _timer = 0.0;
    }
    _entity_manager.apply_transform();
    _entity_manager.early_update();

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
      if(nc) nc->ui();
      ImGui::End();
    }

    auto const& index = _per_index[frame.frame_index];
    auto const& size = gev::engine::get().swapchain_size();
    auto const& c = frame.command_buffer;
    _entity_manager.update();

    frame.output_image->layout(c,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
      gev::engine::get().queues().graphics_family);
    index.msaa_image->layout(c,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
      gev::engine::get().queues().graphics_family);
    index.depth_image->layout(c,
      vk::ImageLayout::eDepthStencilAttachmentOptimal,
      vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      gev::engine::get().queues().graphics_family);

    auto out_att = vk::RenderingAttachmentInfo()
      .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .setClearValue(vk::ClearValue(std::array{ 0.24f, 0.21f, 0.35f, 1.0f }))
      .setImageView(index.msaa_view.get())
      .setResolveImageView(frame.output_view)
      .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .setResolveMode(vk::ResolveModeFlagBits::eAverage)
      .setLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStoreOp(vk::AttachmentStoreOp::eStore);
    auto depth_attachment = vk::RenderingAttachmentInfo()
      .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
      .setClearValue(vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)))
      .setImageView(index.depth_view.get())
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore);

    c.beginRendering(vk::RenderingInfo()
      .setColorAttachments(out_att)
      .setLayerCount(1)
      .setRenderArea(vk::Rect2D({ 0, 0 }, { size.width, size.height })));
    c.bindPipeline(vk::PipelineBindPoint::eGraphics, _environment_pipeline.get());
    c.setViewport(0, vk::Viewport(0, 0, size.width, size.height, 0.f, 1.f));
    c.setScissor(0, vk::Rect2D({ 0, 0 }, size));
    _camera->bind(c, vk::PipelineBindPoint::eGraphics, _environment_pipeline_layout.get(), frame.frame_index);
    c.draw(3, 1, 0, 0);
    c.endRendering();

    out_att.setLoadOp(vk::AttachmentLoadOp::eLoad);
    c.beginRendering(vk::RenderingInfo()
      .setColorAttachments(out_att)
      .setPDepthAttachment(&depth_attachment)
      .setLayerCount(1)
      .setRenderArea(vk::Rect2D({ 0, 0 }, { size.width, size.height })));

    bool const allow_input = !(ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard);
    _camera->update(frame.delta_time, frame.frame_index, allow_input);

    c.bindPipeline(vk::PipelineBindPoint::eGraphics, _main_pipeline.get());
    c.setViewport(0, vk::Viewport(0, 0, size.width, size.height, 0.f, 1.f));
    c.setScissor(0, vk::Rect2D({ 0, 0 }, size));

    _camera->bind(c, vk::PipelineBindPoint::eGraphics, _main_pipeline_layout.get(), frame.frame_index);
    _ddf_binder->bind(c, _main_pipeline_layout.get(), 2);

    for (auto const& e : _entities)
    {
      e.mtl->bind(c, _main_pipeline_layout.get(), 1);
      e.obj->draw(c);
    }

    c.endRendering();

    if (_draw_ddfs)
    {
      depth_attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
      c.beginRendering(vk::RenderingInfo()
        .setColorAttachments(out_att)
        .setPDepthAttachment(&depth_attachment)
        .setLayerCount(1)
        .setRenderArea(vk::Rect2D({ 0, 0 }, { size.width, size.height })));

      for (auto const& d : _ddfs)
      {
        _ddf_gen->draw(c, *_camera, *d, frame.frame_index);
      }
      c.endRendering();
    }

    _entity_manager.late_update();
    return true;
  }

  double _timer = 1.0;
  double _fps = 0.0;
  int _count = 0;
  gev::per_frame<render_attachments> _per_index;

  vk::UniqueDescriptorSetLayout _material_set_layout;

  vk::UniquePipelineLayout _main_pipeline_layout;
  vk::UniquePipeline _main_pipeline;

  vk::UniquePipelineLayout _environment_pipeline_layout;
  vk::UniquePipeline _environment_pipeline;

  std::unique_ptr<camera> _camera;

  std::vector<entity> _entities;
  std::unique_ptr<ddf_generator> _ddf_gen;
  std::unique_ptr<ddf_binder> _ddf_binder;
  std::vector<std::unique_ptr<ddf>> _ddfs;

  bool _draw_ddfs = false;
  bool _fullscreen = false;
  rnu::vec2i _position_before_fullscreen = { 0, 0 };
  rnu::vec2i _size_before_fullscreen = { 0, 0 };

  gev::scenery::entity_manager _entity_manager;
};

int main(int argc, char** argv)
{
  return test01{}.start();
}