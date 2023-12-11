#pragma once

#include <gev/vma.hpp>
#include <gev/window.hpp>
#include <gev/image.hpp>
#include <gev/logger.hpp>
#include <gev/descriptors.hpp>
#include <gev/imgui/imgui.h>
#include <gev/audio/audio.hpp>

#include <string>
#include <functional>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <any>
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
  };

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
    vk::SwapchainKHR swapchain() const;
    vk::Extent2D swapchain_size() const;

    logger& logger();
    audio::audio_host& audio_host() const;

    frame const& current_frame() const;

    void execute_once(std::function<void(vk::CommandBuffer c)> func, vk::CommandPool pool, bool synchronize = false);
    void execute_once(std::function<void(vk::CommandBuffer c)> func, vk::Queue queue, vk::CommandPool pool, bool synchronize = false);

  private:
    engine() = default;
    void start_impl(std::string const& title, int width, int height);

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

    frame _current_frame;

    descriptor_allocator _descriptor_allocator;
    unique_window _window;
    vk::UniqueSurfaceKHR _window_surface;
    vk::SurfaceFormat2KHR _swapchain_format;
    vk::UniqueSwapchainKHR _swapchain;
    vk::Extent2D _swapchain_size;
    std::vector<per_swapchain_image> _per_swapchain_image;
    std::function<void(int w, int h)> _callback;

    vk::UniqueDescriptorPool _imgui_descriptor_pool;
    std::unique_ptr<audio::audio_host> _audio_host;

    ImGuiContext* _imgui_context = nullptr;
  };
}