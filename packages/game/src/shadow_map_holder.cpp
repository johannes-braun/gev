#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/shadow_map_holder.hpp>

namespace gev::game
{
  void shadow_map_instance::update_transform(rnu::mat4 const& transform)
  {
    if (!_holder.expired())
      _holder.lock()->update_matrix_internal(_byte_offset, transform);
  }

  void shadow_map_instance::destroy()
  {
    if (!_holder.expired())
      _holder.lock()->destroy(_map);
  }

  void shadow_map_holder::update_matrix_internal(std::size_t offset, rnu::mat4 matrix)
  {
    sm_info info;
    std::byte* begin = reinterpret_cast<std::byte*>(_map_instances.data()) + offset;
    std::memcpy(&info, begin, sizeof(sm_info));

    if ((matrix != info.matrix).any())
    {
      info.matrix = matrix;
      info.inverse_matrix = inverse(matrix);
      std::memcpy(begin, &info, 2 * sizeof(rnu::mat4));
      include_update_region(offset, offset + 2 * sizeof(rnu::mat4));
    }
  }

  shadow_map_holder::shadow_map_holder()
  {
    vk::DescriptorPoolSize sizes[] = {
      {vk::DescriptorType::eStorageBuffer, 1},
      {vk::DescriptorType::eCombinedImageSampler, max_num_shadow_maps},
    };
    _map_pool = gev::engine::get().device().createDescriptorPoolUnique(
      vk::DescriptorPoolCreateInfo().setMaxSets(1).setPoolSizes(sizes).setFlags(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet));
    auto const layout = layouts::defaults().shadow_map_layout();
    _map_descriptor = std::move(gev::engine::get().device().allocateDescriptorSetsUnique(
      vk::DescriptorSetAllocateInfo().setDescriptorPool(*_map_pool).setSetLayouts(layout))[0]);

    _map_instances.push_back({.map_id = -1});
    _update_region_start = 0;
    _update_region_end = sizeof(sm_info);

    _sampler = gev::engine::get().device().createSamplerUnique(
      vk::SamplerCreateInfo()
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
        .setAnisotropyEnable(false)
        .setCompareEnable(true)
        .setCompareOp(vk::CompareOp::eLess)
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setUnnormalizedCoordinates(false)
        .setMinLod(0)
        .setMaxLod(1000));
  }

  vk::DescriptorSet shadow_map_holder::descriptor() const
  {
    return _map_descriptor.get();
  }

  void shadow_map_holder::destroy(std::shared_ptr<gev::image> map)
  {
    auto const iter = _instance_refs.find(map);
    if (iter == _instance_refs.end())
      return;

    auto const index = iter->second.index;
    auto const& instance = _instances[index];
    auto const byte_offset = instance->_byte_offset;

    for (auto& i : _instances)
    {
      if (i->_byte_offset > byte_offset)
        i->_byte_offset -= sizeof(sm_info);
    }

    for (auto& [_, s] : _instance_refs)
      if (s.index > std::int64_t(index))
        s.index--;
    for (auto& i : _map_instances)
      if (i.map_id > std::int64_t(index))
        i.map_id--;

    _map_instances.erase(std::next(begin(_map_instances), index));
    _instances.erase(std::next(begin(_instances), index));
    _instance_refs.erase(iter);

    for (auto const& i : _instance_refs)
    {
      gev::update_descriptor(_map_descriptor.get(), binding_maps,
        vk::DescriptorImageInfo()
          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
          .setImageView(i.second.view.get())
          .setSampler(_sampler.get()),
        vk::DescriptorType::eCombinedImageSampler, std::uint32_t(i.second.index));
    }

    include_update_region(0, _map_instances.size() * sizeof(sm_info));
  }

  std::shared_ptr<shadow_map_instance> shadow_map_holder::instantiate(std::shared_ptr<gev::image> map, rnu::mat4 matrix)
  {
    auto& f = _instance_refs[map];

    if (!f.view)
    {
      f.index = _map_instances.size() - 1;
      f.view = map->create_view(vk::ImageViewType::e2D);
      gev::update_descriptor(_map_descriptor.get(), binding_maps,
        vk::DescriptorImageInfo()
          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
          .setImageView(f.view.get())
          .setSampler(_sampler.get()),
        vk::DescriptorType::eCombinedImageSampler, std::uint32_t(f.index));
    }

    auto& info = _map_instances[_map_instances.size() - 1];
    info.matrix = matrix;
    info.map_id = f.index;
    info.metadata0 = 0;
    info.metadata1 = 0;
    info.metadata2 = 0;
    _map_instances.push_back({.map_id = -1});

    auto const begin = (_map_instances.size() - 2) * sizeof(sm_info);
    include_update_region(begin, _map_instances.size() * sizeof(sm_info));

    auto instance = std::make_shared<shadow_map_instance>();
    instance->_byte_offset = begin;
    instance->_map = map;
    instance->_holder = shared_from_this();
    _instances.push_back(instance);
    return instance;
  }

  void shadow_map_holder::include_update_region(std::size_t begin, std::size_t end)
  {
    _update_region_start = std::min(_update_region_start, begin);
    _update_region_end = std::max(_update_region_end, end);
  }

  void shadow_map_holder::try_flush_buffer(vk::CommandBuffer c)
  {
    if (_instances_buffer && _update_region_end <= _update_region_start)
      return;

    if (!_instances_buffer || _update_region_end > _instances_buffer->size())
    {
      auto v = std::max(min_reserved_elements * sizeof(sm_info), _update_region_end);
      v--;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      v |= v >> 16;
      v |= v >> 32;
      v++;

      _instances_buffer.reset();
      _staging_buffer = gev::buffer::host_local(v, vk::BufferUsageFlagBits::eTransferSrc);
      _instances_buffer =
        gev::buffer::device_local(v, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst);
      gev::update_descriptor(
        _map_descriptor.get(), binding_instances, *_instances_buffer, vk::DescriptorType::eStorageBuffer);
    }

    if (_update_region_end > _update_region_start)
    {
      auto const size = _update_region_end - _update_region_start;
      std::byte const* begin = reinterpret_cast<std::byte const*>(_map_instances.data()) + _update_region_start;
      _staging_buffer->load_data(begin, size, _update_region_start);

      auto barr2 =
        vk::BufferMemoryBarrier()
          .setBuffer(_instances_buffer->get_buffer())
          .setOffset(_update_region_start)
          .setSize(size)
          .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
          .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
          .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
      c.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlagBits::eByRegion, nullptr, {barr2}, nullptr);

      _staging_buffer->copy_to(c, *_instances_buffer, size, _update_region_start, _update_region_start);

      auto barr =
        vk::BufferMemoryBarrier()
          .setBuffer(_instances_buffer->get_buffer())
          .setOffset(_update_region_start)
          .setSize(size)
          .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
          .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
          .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
      c.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlagBits::eByRegion, nullptr, {barr}, nullptr);
    }

    _update_region_start = std::numeric_limits<std::size_t>::max();
    _update_region_end = std::numeric_limits<std::size_t>::lowest();
  }
}    // namespace gev::game