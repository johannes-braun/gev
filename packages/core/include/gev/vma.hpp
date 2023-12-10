#pragma once

#include <memory>
#include <vk_mem_alloc.h>
#include <span>

namespace gev
{
  struct vma_allocator_deleter
  {
    void operator()(VmaAllocator alloc) const;
  };
  using unique_allocator = std::unique_ptr<VmaAllocator_T, vma_allocator_deleter>;
  using shared_allocator = std::shared_ptr<VmaAllocator_T>;

  struct vma_allocation_deleter
  {
    shared_allocator allocator;

    void operator()(VmaAllocation alloc) const;
  };
  using unique_allocation = std::unique_ptr<VmaAllocation_T, vma_allocation_deleter>;

  inline unique_allocation wrap_allocation(shared_allocator allocator, VmaAllocation allocation)
  {
    return unique_allocation(allocation, vma_allocation_deleter{ std::move(allocator) });
  }

  template<typename T>
  std::span<T> map_memory(shared_allocator const& allocator, unique_allocation& alloc, size_t count)
  {
    void* memory = nullptr;
    vmaMapMemory(allocator.get(), alloc.get(), &memory);
    return std::span(static_cast<T*>(memory), count);
  }
}