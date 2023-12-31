#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

#include <any>
#include <functional>
#include <gev/audio/sound.hpp>
#include <gev/descriptors.hpp>
#include <gev/image.hpp>
#include <gev/imgui/imgui.h>
#include <gev/logger.hpp>
#include <gev/repo.hpp>
#include <gev/service_locator.hpp>
#include <gev/vma.hpp>
#include <gev/window.hpp>
#include <string>
#include <unordered_map>

namespace gev
{
  struct queues
  {
    std::uint32_t graphics_family = 0;
    vk::Queue graphics;
    vk::UniqueCommandPool graphics_command_pool;

    std::uint32_t transfer_family = 0;
    vk::Queue transfer;
    vk::UniqueCommandPool transfer_command_pool;

    std::uint32_t compute_family = 0;
    vk::Queue compute;
    vk::UniqueCommandPool compute_command_pool;

    std::uint32_t present_family = 0;
    vk::Queue present;
  };

  struct per_swapchain_image
  {
    vk::UniqueSemaphore available_semaphore;
    vk::UniqueSemaphore finished_semaphore;
    vk::UniqueFence render_fence;
    std::shared_ptr<image> output_image;
    vk::UniqueImageView output_view;
  };

  struct frame
  {
    double delta_time = 0.0;
    std::uint32_t frame_index = 0;
    std::shared_ptr<image> output_image;
    vk::ImageView output_view;
    vk::CommandBuffer command_buffer;

    void present_image(image& source) const;
  };

  using audio_repo = repo<audio::sound>;

  class engine
  {
  public:
    static constexpr std::uint32_t requested_num_swapchain_images = 3;

    static engine& get();
    static void reset();

    ~engine();
    void start(std::string const& title, int width, int height)
    {
      start_impl(title, width, height);
      ImGui::SetCurrentContext(_imgui_context);
    }

    int run(std::function<bool(frame const& f)> runnable);
    void on_resized(std::function<void(int w, int h)> callback);
    std::uint32_t num_images() const noexcept;
    std::span<vk::PresentModeKHR const> present_modes() const noexcept;
    vk::PresentModeKHR present_mode() const noexcept;
    void set_present_mode(vk::PresentModeKHR mode);

    descriptor_allocator& get_descriptor_allocator();
    descriptor_allocator const& get_descriptor_allocator() const;
    vk::Instance instance() const;
    vk::PhysicalDevice physical_device() const;
    vk::Device device() const;
    shared_allocator const& allocator() const;
    queues const& queues() const;
    GLFWwindow* window() const;
    vk::SurfaceKHR window_surface() const;
    vk::SurfaceFormat2KHR swapchain_format() const;
    vk::Format depth_format() const;
    vk::Format select_format(
      vk::ArrayProxy<vk::Format const> candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const;
    vk::SwapchainKHR swapchain() const;
    vk::Extent2D swapchain_size() const;

    service_locator& services();
    logger& logger();

    frame const& current_frame() const;

    void execute_once(std::function<void(vk::CommandBuffer c)> func, vk::CommandPool pool, bool synchronize = false);
    void execute_once(
      std::function<void(vk::CommandBuffer c)> func, vk::Queue queue, vk::CommandPool pool, bool synchronize = false);

  private:
    engine() = default;
    void start_impl(std::string const& title, int width, int height);

    VkBool32 debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
      void* pUserData);

    vk::UniqueInstance create_instance(std::string const& title);
    vk::PhysicalDevice pick_physical_device();
    vk::UniqueDevice create_device();
    unique_allocator create_allocator();
    void open_window(std::string const& title, int width, int height);
    void create_swapchain();

    static std::unique_ptr<engine> _engine;

    gev::logger _logger;

    vk::UniqueInstance _instance;
    vk::PhysicalDevice _physical_device;
    vk::UniqueDevice _device;
    vk::PhysicalDeviceProperties _physical_device_properties;
    VmaVulkanFunctions _vma_functions{};
    shared_allocator _allocator;
    gev::queues _queues;
    vk::Format _depth_format;
    vk::UniqueDebugUtilsMessengerEXT _debug_messenger;

    frame _current_frame;

    descriptor_allocator _descriptor_allocator;
    unique_window _window;
    vk::UniqueSurfaceKHR _window_surface;
    vk::SurfaceFormat2KHR _swapchain_format;
    vk::UniqueSwapchainKHR _swapchain;
    vk::Extent2D _swapchain_size;
    std::vector<per_swapchain_image> _per_swapchain_image;
    std::vector<std::function<void(int w, int h)>> _resize_callbacks;

    std::vector<vk::PresentModeKHR> _present_modes;
    vk::PresentModeKHR _present_mode;

    vk::UniqueDescriptorPool _imgui_descriptor_pool;
    service_locator _services;

    bool _swapchain_dirty = true;

    ImGuiContext* _imgui_context = nullptr;
  };

  static inline vk::Device device()
  {
    return engine::get().device();
  }

  template<typename Svc, typename... Args>
  std::shared_ptr<Svc> register_service(Args&&... args)
  {
    return gev::engine::get().services().register_service<Svc>(std::forward<Args>(args)...);
  }

  template<typename Svc>
  std::shared_ptr<Svc> service()
  {
    return gev::engine::get().services().resolve<Svc>();
  }
}    // namespace gev