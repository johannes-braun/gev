#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "rethink_sans.hpp"

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <gev/engine.hpp>
#include <gev/imgui/imgui.h>
#include <gev/imgui/imgui_impl_glfw.h>
#include <gev/imgui/imgui_impl_vulkan.h>
#include <print>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace gev
{
  class queue_family
  {
  public:
    queue_family() = default;
    void set(int index)
    {
      _index = index;
      _done = true;
    }
    bool done() const
    {
      return _done;
    }
    int family() const
    {
      return _index;
    }

  private:
    int _index = 0;
    bool _done = false;
  };

  std::unique_ptr<engine> engine::_engine = std::unique_ptr<engine>(new engine);

  engine& engine::get()
  {
    return *_engine;
  }

  void engine::reset()
  {
    _engine.reset();
  }

  logger& engine::logger()
  {
    return _logger;
  }

  vk::Format find_supported_format(vk::PhysicalDevice device, vk::ArrayProxy<vk::Format const> candidates,
    vk::ImageTiling tiling, vk::FormatFeatureFlags features)
  {
    for (auto const format : candidates)
    {
      auto const props = device.getFormatProperties(format);

      if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
      {
        return format;
      }
      else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
      {
        return format;
      }
    }

    throw std::runtime_error("failed to find supported format!");
  }

  service_locator& engine::services()
  {
    return _services;
  }

  void engine::start_impl(std::string const& title, int width, int height)
  {
    static class glfw_initializer
    {
    public:
      glfw_initializer() : _started(glfwInit()) {}
      ~glfw_initializer()
      {
        glfwTerminate();
      }

      bool has_started() const noexcept
      {
        return _started;
      }

    private:
      bool _started = false;
    } glfw_init;

    if (!glfw_init.has_started())
      throw std::runtime_error("GLFW failed to initialize.");

    if (!glfwVulkanSupported())
      throw std::runtime_error("Vulkan is not supported on this device.");

    VULKAN_HPP_DEFAULT_DISPATCHER.init(&glfwGetInstanceProcAddress);
    _instance = create_instance(title);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*_instance);

    _debug_messenger = _instance->createDebugUtilsMessengerEXTUnique(
      vk::DebugUtilsMessengerCreateInfoEXT()
        .setMessageSeverity(
          // vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
          // vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
          vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
          vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding |
          vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(
          [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32
          {
            auto* e = static_cast<engine*>(pUserData);
            return e->debug_message_callback(messageSeverity, messageTypes, pCallbackData, pUserData);
          })
        .setPUserData(this));

    _physical_device = pick_physical_device();
    _physical_device_properties = _physical_device.getProperties();
    _device = create_device();
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*_device);
    _allocator = create_allocator();

    vk::CommandPoolCreateInfo cpc;
    cpc.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cpc.queueFamilyIndex = _queues.graphics_family;
    _queues.graphics_command_pool = _device->createCommandPoolUnique(cpc);
    cpc.queueFamilyIndex = _queues.compute_family;
    _queues.compute_command_pool = _device->createCommandPoolUnique(cpc);
    cpc.queueFamilyIndex = _queues.transfer_family;
    _queues.transfer_command_pool = _device->createCommandPoolUnique(cpc);

    open_window(title, width, height);
    create_swapchain();

    auto const imgui_pool_size = vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1);
    _imgui_descriptor_pool = _device->createDescriptorPoolUnique(
      vk::DescriptorPoolCreateInfo()
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(1)
        .setPoolSizes(imgui_pool_size));
    _imgui_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(_imgui_context);
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(_window.get(), true);
    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance)
      { return gev::engine::get().instance().getProcAddr(function_name); });
    ImGui_ImplVulkan_InitInfo init{};
    init.UseDynamicRendering = true;
    init.ColorAttachmentFormat = VkFormat(gev::engine::get().swapchain_format().surfaceFormat.format);
    init.Instance = _instance.get();
    init.Device = _device.get();
    init.QueueFamily = _queues.graphics_family;
    init.Queue = _queues.graphics;
    init.ImageCount = _per_swapchain_image.size();
    init.MinImageCount = _per_swapchain_image.size();
    init.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init.PhysicalDevice = _physical_device;
    init.DescriptorPool = _imgui_descriptor_pool.get();
    ImGui_ImplVulkan_Init(&init, nullptr);

    ImFontConfig cfg{};
    cfg.FontDataOwnedByAtlas = false;
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(rethink_sans), rethink_sans_length, 16, &cfg);

    _depth_format =
      select_format({vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eD16UnormS8Uint},
        vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);

    _audio_host = audio::audio_host::create();
  }

  VkBool32 engine::debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
  {
    switch (messageSeverity)
    {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: _logger.debug("{}", pCallbackData->pMessage); break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: _logger.log("{}", pCallbackData->pMessage); break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: _logger.warn("{}", pCallbackData->pMessage); break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: _logger.error("{}", pCallbackData->pMessage); break;
      default: break;
    }
    return VK_FALSE;
  }

  frame const& engine::current_frame() const
  {
    return _current_frame;
  }

  int engine::run(std::function<bool(frame const& f)> runnable)
  {
    vk::CommandBufferAllocateInfo cballoc;
    cballoc.commandBufferCount = std::uint32_t(_per_swapchain_image.size());
    cballoc.commandPool = _queues.graphics_command_pool.get();
    cballoc.level = vk::CommandBufferLevel::ePrimary;
    auto const cbufs = _device->allocateCommandBuffersUnique(cballoc);

    std::uint32_t current_frame = 0;
    double last_time = glfwGetTime();
    double last_frame_time = glfwGetTime();
    while (!glfwWindowShouldClose(_window.get()))
    {
      double const delta = glfwGetTime() - last_frame_time;
      last_frame_time = glfwGetTime();

      if (glfwGetWindowAttrib(_window.get(), GLFW_ICONIFIED))
      {
        glfwWaitEvents();
        continue;
      }
      auto& frame = _per_swapchain_image[current_frame];

      [[maybe_unused]] auto const wait_result =
        _device->waitForFences(frame.render_fence.get(), true, std::numeric_limits<std::uint64_t>::max());
      _device->resetFences(frame.render_fence.get());

      vk::AcquireNextImageInfoKHR acquire;
      acquire.semaphore = frame.available_semaphore.get();
      acquire.swapchain = _swapchain.get();
      acquire.timeout = std::numeric_limits<std::uint64_t>::max();
      acquire.deviceMask = 1;
      auto const [acquire_result, image_index] = _device->acquireNextImage2KHR(acquire);

      auto const& c = cbufs[current_frame].get();

      c.reset();
      c.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse));

      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      _current_frame.delta_time = delta;
      _current_frame.frame_index = current_frame;
      _current_frame.output_image = frame.output_image;
      _current_frame.output_view = frame.output_view.get();
      _current_frame.command_buffer = c;
      if (!runnable(_current_frame))
      {
        glfwSetWindowShouldClose(_window.get(), true);
      }

      ImGui::Render();

      frame.output_image->layout(c, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
        _queues.graphics_family);
      auto out_att =
        vk::RenderingAttachmentInfo()
          .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
          .setImageView(frame.output_view.get())
          .setLoadOp(vk::AttachmentLoadOp::eLoad)
          .setStoreOp(vk::AttachmentStoreOp::eStore);
      c.beginRendering(vk::RenderingInfo().setColorAttachments(out_att).setLayerCount(1).setRenderArea(
        vk::Rect2D({0, 0}, {_swapchain_size.width, _swapchain_size.height})));

      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), c);
      c.endRendering();

      frame.output_image->layout(c, vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead, _queues.present_family);
      c.end();

      vk::SubmitInfo submit;
      submit.setWaitSemaphores(frame.available_semaphore.get());
      submit.setCommandBuffers(c);
      submit.setSignalSemaphores(frame.finished_semaphore.get());
      vk::PipelineStageFlags wait_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
      submit.setWaitDstStageMask(wait_stage_mask);
      _queues.graphics.submit(submit, frame.render_fence.get());

      vk::PresentInfoKHR present;
      present.setImageIndices(image_index);
      present.setSwapchains(_swapchain.get());
      present.setWaitSemaphores(frame.finished_semaphore.get());
      auto const present_result = _queues.present.presentKHR(&present);

      glfwPollEvents();
      current_frame = (current_frame + 1) % _per_swapchain_image.size();

      if (present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR)
      {
        create_swapchain();
        continue;
      }
    }

    _device->waitIdle();
    return 0;
  }

  engine::~engine()
  {
    _device->waitIdle();
    _services.clear();
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext(_imgui_context);
  }

  vk::Format engine::select_format(
    vk::ArrayProxy<vk::Format const> candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const
  {
    return find_supported_format(_physical_device, candidates, tiling, features);
  }

  vk::Format engine::depth_format() const
  {
    return _depth_format;
  }

  std::uint32_t engine::num_images() const noexcept
  {
    return static_cast<std::uint32_t>(_per_swapchain_image.size());
  }

  void engine::on_resized(std::function<void(int w, int h)> callback)
  {
    _resize_callbacks.push_back(std::move(callback));
  }

  void engine::execute_once(
    std::function<void(vk::CommandBuffer c)> func, vk::Queue queue, vk::CommandPool pool, bool synchronize)
  {
    vk::CommandBufferAllocateInfo cballoc;
    cballoc.commandBufferCount = 1;
    cballoc.commandPool = pool;
    cballoc.level = vk::CommandBufferLevel::ePrimary;

    auto const cbufs = _device->allocateCommandBuffersUnique(cballoc);
    auto const fence = synchronize ? _device->createFenceUnique({}) : vk::UniqueFence();

    vk::CommandBufferBeginInfo begin;
    begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cbufs[0]->begin(begin);

    func(cbufs[0].get());

    cbufs[0]->end();

    vk::SubmitInfo submit;
    submit.setCommandBuffers(cbufs[0].get());
    queue.submit(submit, fence.get());

    if (fence)
    {
      [[maybe_unused]] auto const r =
        gev::engine::get().device().waitForFences(fence.get(), true, std::numeric_limits<std::uint64_t>::max());
    }
  }

  void engine::execute_once(std::function<void(vk::CommandBuffer c)> func, vk::CommandPool pool, bool synchronize)
  {
    execute_once(std::move(func), _queues.graphics, pool, synchronize);
  }

  vk::UniqueInstance engine::create_instance(std::string const& title)
  {
    auto const instance_version = vk::enumerateInstanceVersion();
    if (instance_version < VK_API_VERSION_1_3)
      throw std::runtime_error("Instance version is not high enough.");

    vk::ApplicationInfo app_info(
      title.c_str(), VK_MAKE_API_VERSION(1, 0, 0, 0), "GEV", VK_MAKE_API_VERSION(1, 0, 0, 0), VK_API_VERSION_1_3);

    vk::InstanceCreateInfo instance_info;
    instance_info.pApplicationInfo = &app_info;

    constexpr static std::array required_instance_layers{
      "VK_LAYER_KHRONOS_validation",
    };

    std::vector required_instance_extensions{
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME};
    std::uint32_t num_glfw = 0;
    auto const** arr_glfw = glfwGetRequiredInstanceExtensions(&num_glfw);
    for (std::uint32_t i = 0; i < num_glfw; ++i)
      required_instance_extensions.push_back(arr_glfw[i]);

    auto const instance_layers = vk::enumerateInstanceLayerProperties();
    auto const instance_extensions = vk::enumerateInstanceExtensionProperties();

    std::vector<char const*> found_instance_layers;
    std::vector<char const*> found_instance_extensions;

    for (auto const& req : required_instance_layers)
    {
      bool found = false;
      for (auto const& cur : instance_layers)
      {
        if (std::string_view(cur.layerName.data()) == std::string_view(req))
        {
          found_instance_layers.push_back(req);
          _logger.debug("Layer {}: FOUND", req);
          found = true;
          break;
        }
      }
      if (found)
        continue;

      _logger.warn("Layer {}: NOT FOUND", req);
    }

    for (auto const& req : required_instance_extensions)
    {
      bool found = false;
      for (auto const& cur : instance_extensions)
      {
        if (std::string_view(cur.extensionName.data()) == std::string_view(req))
        {
          found_instance_extensions.push_back(req);
          _logger.debug("Extension {}: FOUND", req);
          found = true;
          break;
        }
      }
      if (found)
        continue;

      _logger.warn("Extension {}: NOT FOUND", req);
    }

    instance_info.setPEnabledLayerNames(found_instance_layers);
    instance_info.setPEnabledExtensionNames(found_instance_extensions);

    return vk::createInstanceUnique(instance_info);
  }

  vk::PhysicalDevice engine::pick_physical_device()
  {
    auto const devices = _instance->enumeratePhysicalDevices();

    // Todo: better query

    return devices[0];
  }

  vk::UniqueDevice engine::create_device()
  {
    auto const device_extensions = _physical_device.enumerateDeviceExtensionProperties(nullptr);
    auto const device_layers = _physical_device.enumerateDeviceLayerProperties();
    auto feat = _physical_device.getFeatures2();
    auto queue_infos = _physical_device.getQueueFamilyProperties2();

    vk::PhysicalDeviceSynchronization2Features sync2;
    sync2.synchronization2 = true;
    feat.pNext = &sync2;
    vk::PhysicalDeviceDynamicRenderingFeatures rend;
    rend.dynamicRendering = true;
    sync2.pNext = &rend;
    vk::PhysicalDeviceShaderClockFeaturesKHR clock;
    clock.shaderDeviceClock = true;
    rend.pNext = &clock;
    vk::PhysicalDeviceVulkan11Features vk11f;
    vk11f.setShaderDrawParameters(true);
    clock.pNext = &vk11f;
    vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT dynamic_vertex_input;
    dynamic_vertex_input.setVertexInputDynamicState(true);
    vk11f.pNext = &dynamic_vertex_input;

    vk::PhysicalDeviceVulkan12Features ft12;
    ft12.setRuntimeDescriptorArray(true);
    ft12.setDescriptorBindingPartiallyBound(true);
    ft12.setDescriptorBindingVariableDescriptorCount(true);
    dynamic_vertex_input.pNext = &ft12;

    vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT extd3;
    extd3.setExtendedDynamicState3RasterizationSamples(true);
    ft12.pNext = &extd3;

    const std::array required_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_CLOCK_EXTENSION_NAME,
      VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME};

    std::vector<const char*> found_device_extensions;
    for (auto const& req : required_device_extensions)
    {
      bool found = false;
      for (auto const& cur : device_extensions)
      {
        if (std::string_view(cur.extensionName.data()) == std::string_view(req))
        {
          found_device_extensions.push_back(req);
          _logger.debug("Device Extension {}: FOUND", req);
          found = true;
          break;
        }
      }
      if (found)
        continue;

      _logger.warn("Device Extension {}: NOT FOUND", req);
    }

    // 1present, 2 gfx, 2 transfer, 2 compute
    queue_family present_family;
    queue_family graphics_family;
    queue_family transfer_family;
    queue_family compute_family;

    for (std::uint32_t i = 0; i < queue_infos.size(); ++i)
    {
      if (glfwGetPhysicalDevicePresentationSupport(*_instance, _physical_device, i))
      {
        present_family.set(i);
        break;
      }
    }

    for (std::uint32_t i = 0; i < queue_infos.size(); ++i)
    {
      if (!graphics_family.done() && queue_infos[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics)
      {
        graphics_family.set(i);

        if (!compute_family.done() && queue_infos[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute)
        {
          compute_family.set(i);
        }
      }

      if (!compute_family.done() && queue_infos[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute)
      {
        compute_family.set(i);
      }

      if (!transfer_family.done() && queue_infos[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eTransfer)
      {
        transfer_family.set(i);
      }
    }

    std::unordered_set<std::uint32_t> family_filter;
    family_filter.emplace(present_family.family());
    family_filter.emplace(graphics_family.family());
    family_filter.emplace(compute_family.family());
    family_filter.emplace(transfer_family.family());

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::unordered_map<std::uint32_t, std::uint32_t> indices;
    std::array priorities{1.0f};

    for (auto const& family : family_filter)
    {
      queue_create_infos.push_back(vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags{}, family, priorities));
    }

    vk::DeviceCreateInfo info;
    info.setQueueCreateInfos(queue_create_infos);
    info.pNext = &feat;
    info.setPEnabledExtensionNames(found_device_extensions);

    auto device = _physical_device.createDeviceUnique(info, nullptr);
    _queues.present_family = present_family.family();
    _queues.present = device->getQueue(present_family.family(), 0);

    _queues.graphics_family = graphics_family.family();
    _queues.graphics = device->getQueue(graphics_family.family(), 0);

    _queues.transfer_family = transfer_family.family();
    _queues.transfer = device->getQueue(transfer_family.family(), 0);

    _queues.compute_family = compute_family.family();
    _queues.compute = device->getQueue(compute_family.family(), 0);

    return device;
  }

  unique_allocator engine::create_allocator()
  {
    _vma_functions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    _vma_functions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = _physical_device;
    allocatorCreateInfo.device = *_device;
    allocatorCreateInfo.instance = *_instance;
    allocatorCreateInfo.pVulkanFunctions = &_vma_functions;

    VmaAllocator allocator;
    vmaCreateAllocator(&allocatorCreateInfo, &allocator);
    return unique_allocator(allocator);
  }

  void engine::open_window(std::string const& title, int w, int h)
  {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = unique_window{glfwCreateWindow(w, h, title.data(), nullptr, nullptr)};

    VkSurfaceKHR surface = nullptr;
    glfwCreateWindowSurface(*_instance, _window.get(), nullptr, &surface);
    _window_surface = vk::UniqueSurfaceKHR(
      surface, vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(_instance.get()));

    vk::PhysicalDeviceSurfaceInfo2KHR query(*_window_surface);
    auto const formats = _physical_device.getSurfaceFormats2KHR(query);
    vk::SurfaceFormat2KHR format = formats[0];
    for (auto const& f : formats)
    {
      if (f.surfaceFormat.format == vk::Format::eB8G8R8A8Srgb &&
        f.surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
      {
        format = f;
        break;
      }
    }
    _swapchain_format = format;
  }

  void engine::create_swapchain()
  {
    _device->waitIdle();

    _per_swapchain_image.clear();

    vk::PhysicalDeviceSurfaceInfo2KHR query(*_window_surface);
    auto const caps = _physical_device.getSurfaceCapabilities2KHR(query);
    auto const present_modes = _physical_device.getSurfacePresentModesKHR(query.surface);

    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    if (std::ranges::find(present_modes, vk::PresentModeKHR::eMailbox) != end(present_modes))
      present_mode = vk::PresentModeKHR::eMailbox;

    vk::SwapchainCreateInfoKHR sc_info;
    sc_info.surface = *_window_surface;
    sc_info.minImageCount = std::clamp(
      requested_num_swapchain_images, caps.surfaceCapabilities.minImageCount, caps.surfaceCapabilities.maxImageCount);
    sc_info.imageArrayLayers = 1;
    sc_info.imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;
    sc_info.imageSharingMode = vk::SharingMode::eExclusive;
    sc_info.imageExtent = caps.surfaceCapabilities.currentExtent;
    sc_info.oldSwapchain = *_swapchain;
    sc_info.presentMode = present_mode;
    sc_info.imageColorSpace = _swapchain_format.surfaceFormat.colorSpace;
    sc_info.imageFormat = _swapchain_format.surfaceFormat.format;

    std::unordered_set<std::uint32_t> unique_families{_queues.graphics_family, _queues.present_family};
    std::vector<std::uint32_t> families(std::begin(unique_families), end(unique_families));
    sc_info.setQueueFamilyIndices(families);
    sc_info.preTransform = caps.surfaceCapabilities.currentTransform;
    sc_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    sc_info.clipped = true;
    sc_info.imageFormat = _swapchain_format.surfaceFormat.format;

    _swapchain = _device->createSwapchainKHRUnique(sc_info);
    _swapchain_format = _swapchain_format;
    _swapchain_size = sc_info.imageExtent;

    auto const images = _device->getSwapchainImagesKHR(*_swapchain);
    _per_swapchain_image.resize(images.size());

    vk::CommandBufferAllocateInfo cballoc;
    cballoc.commandBufferCount = 1;
    cballoc.commandPool = _queues.graphics_command_pool.get();
    cballoc.level = vk::CommandBufferLevel::ePrimary;

    auto const cbufs = _device->allocateCommandBuffersUnique(cballoc);
    auto const fence = _device->createFenceUnique({});

    vk::CommandBufferBeginInfo begin;
    begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cbufs[0]->begin(begin);

    std::unordered_set<std::uint32_t> const queue_families_set{
      _queues.graphics_family, _queues.present_family, _queues.transfer_family};
    std::vector<std::uint32_t> const queue_families(std::begin(queue_families_set), end(queue_families_set));

    for (std::size_t i = 0; i < _per_swapchain_image.size(); ++i)
    {
      auto& pi = _per_swapchain_image[i];
      pi.output_image =
        std::make_shared<image>(images[i], _swapchain_format.surfaceFormat.format, vk::Extent3D(_swapchain_size, 1));
      pi.output_view = _device->createImageViewUnique(
        vk::ImageViewCreateInfo()
          .setComponents(vk::ComponentMapping())

          .setFormat(sc_info.imageFormat)
          .setImage(pi.output_image->get_image())
          .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
          .setViewType(vk::ImageViewType::e2D));
      pi.available_semaphore = _device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
      pi.finished_semaphore = _device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
      pi.render_fence = _device->createFenceUnique(vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled));

      pi.output_image->layout(cbufs[0].get(), vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead,
        _queues.present_family);
    }
    cbufs[0]->end();

    vk::SubmitInfo2 submit;
    vk::CommandBufferSubmitInfo cbs;
    cbs.commandBuffer = cbufs[0].get();
    submit.setCommandBufferInfos(cbs);
    _queues.graphics.submit2(submit, fence.get());
    [[maybe_unused]] auto const wait_result =
      _device->waitForFences(fence.get(), true, std::numeric_limits<uint32_t>::max());

    for (auto& callback : _resize_callbacks)
      callback(_swapchain_size.width, _swapchain_size.height);
  }

  audio::audio_host& engine::audio_host() const
  {
    return *_audio_host;
  }

  descriptor_allocator& engine::get_descriptor_allocator()
  {
    return _descriptor_allocator;
  }

  descriptor_allocator const& engine::get_descriptor_allocator() const
  {
    return _descriptor_allocator;
  }

  vk::Instance engine::instance() const
  {
    return *_instance;
  }

  vk::PhysicalDevice engine::physical_device() const
  {
    return _physical_device;
  }

  vk::Device engine::device() const
  {
    return *_device;
  }

  shared_allocator const& engine::allocator() const
  {
    return _allocator;
  }

  queues const& engine::queues() const
  {
    return _queues;
  }

  GLFWwindow* engine::window() const
  {
    return _window.get();
  }

  vk::SurfaceKHR engine::window_surface() const
  {
    return _window_surface.get();
  }

  vk::SurfaceFormat2KHR engine::swapchain_format() const
  {
    return _swapchain_format;
  }

  vk::SwapchainKHR engine::swapchain() const
  {
    return _swapchain.get();
  }

  vk::Extent2D engine::swapchain_size() const
  {
    return _swapchain_size;
  }

}    // namespace gev