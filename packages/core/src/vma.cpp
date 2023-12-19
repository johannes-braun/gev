#include <gev/vma.hpp>
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vulkan/vulkan.hpp>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace gev
{
  void vma_allocator_deleter::operator()(VmaAllocator alloc) const
  {
    vmaDestroyAllocator(alloc);
  }

  void vma_allocation_deleter::operator()(VmaAllocation alloc) const
  {
    vmaFreeMemory(allocator.get(), alloc);
  }
}    // namespace gev