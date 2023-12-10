#include <gev/descriptors.hpp>
#include <gev/engine.hpp>

namespace gev
{
  descriptor_layout_creator descriptor_layout_creator::get()
  {
    return descriptor_layout_creator{};
  }

  descriptor_layout_creator& descriptor_layout_creator::bind(std::uint32_t binding, vk::DescriptorType type, std::uint32_t count, vk::ShaderStageFlags stage_flags, vk::DescriptorBindingFlags flags, vk::ArrayProxy<vk::Sampler> immutable_sampler)
  {
    std::uintptr_t offset = static_cast<std::uintptr_t>(_samplers.size());
    vk::Sampler const* offset_ptr = std::bit_cast<vk::Sampler const*>(offset);
    if (!immutable_sampler.empty())
    {
      _samplers.insert(_samplers.end(), immutable_sampler.begin(), immutable_sampler.end());
    }

    _bindings.push_back(vk::DescriptorSetLayoutBinding(binding, type, count, stage_flags, offset_ptr));
    _flags.push_back(flags);
    return *this;
  }

  vk::UniqueDescriptorSetLayout descriptor_layout_creator::build()
  {
    vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info;
    flags_info.setBindingFlags(_flags);
    return engine::get().device().createDescriptorSetLayoutUnique(
      vk::DescriptorSetLayoutCreateInfo().setBindings(_bindings).setPNext(&flags_info));
  }

  void update_descriptor(vk::DescriptorSet set, std::uint32_t binding, vk::DescriptorBufferInfo info, vk::DescriptorType type, std::uint32_t array_element)
  {
    vk::WriteDescriptorSet write;
    write.setDescriptorCount(1)
      .setDstSet(set)
      .setDstBinding(binding)
      .setBufferInfo(info)
      .setDescriptorType(type)
      .setDstArrayElement(array_element);
    engine::get().device().updateDescriptorSets(write, {});
  }

  void update_descriptor(vk::DescriptorSet set, std::uint32_t binding, vk::DescriptorImageInfo info, vk::DescriptorType type, std::uint32_t array_element)
  {
    vk::WriteDescriptorSet write;
    write.setDescriptorCount(1)
      .setDstSet(set)
      .setDstBinding(binding)
      .setImageInfo(info)
      .setDescriptorType(type)
      .setDstArrayElement(array_element);
    engine::get().device().updateDescriptorSets(write, {});
  }

  descriptor_allocator::descriptor_allocator(int size)
    : _pool_size(size)
  {
    for (size_t i = 0; i < _scales.size(); ++i)
    {
      _sizes.push_back(vk::DescriptorPoolSize()
        .setType(_scales[i].type)
        .setDescriptorCount(_scales[i].scale * _pool_size));
    }
  }

  vk::DescriptorSet descriptor_allocator::allocate(vk::DescriptorSetLayout layout)
  {
    return allocate(layout, 1)[0];
  }

  std::vector<vk::DescriptorSet> descriptor_allocator::allocate(vk::DescriptorSetLayout layout, std::uint32_t count)
  {
    if (!_current_pool)
      _current_pool = get_free_pool();

    std::vector<vk::DescriptorSet> result;
    bool needs_allocation = false;
    vk::DescriptorSetAllocateInfo info = vk::DescriptorSetAllocateInfo()
      .setDescriptorPool(_current_pool)
      .setDescriptorSetCount(count)
      .setSetLayouts(layout);

    try {
      result = engine::get().device().allocateDescriptorSets(info);
    }
    catch (vk::FragmentedPoolError const& fragmented)
    {
      needs_allocation = true;
    }
    catch (vk::OutOfPoolMemoryError const& out_of_pool_memory)
    {
      needs_allocation = true;
    }

    if (needs_allocation)
    {
      _current_pool = get_free_pool();
      result = engine::get().device().allocateDescriptorSets(info);
    }

    return result;
  }

  vk::DescriptorPool descriptor_allocator::get_free_pool()
  {
    auto pool = activate_unused_pool();
    if (!pool)
    {
      allocate_next_pool();
      pool = _pools[_pools.size() - 1].get();
    }
    return pool;
  }

  void descriptor_allocator::allocate_next_pool()
  {
    vk::DescriptorPoolCreateInfo info;
    info.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
      .setMaxSets(_pool_size)
      .setPoolSizes(_sizes);
    _pools.push_back(engine::get().device().createDescriptorPoolUnique(info));
  }

  vk::DescriptorPool descriptor_allocator::activate_unused_pool()
  {
    if (!_unused.empty())
    {
      auto pool = std::move(_unused[_unused.size() - 1]);
      _unused.pop_back();
      _pools.push_back(std::move(pool));
      return _pools[_pools.size() - 1].get();
    }

    return nullptr;
  }

  void descriptor_allocator::reset()
  {
    for (auto& p : _pools)
    {
      engine::get().device().resetDescriptorPool(p.get());
      _unused.push_back(std::move(p));
    }
    _pools.clear();
    _current_pool = nullptr;
  }
}