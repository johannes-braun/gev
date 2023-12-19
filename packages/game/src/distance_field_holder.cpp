#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/distance_field_holder.hpp>

namespace gev::game
{
  void distance_field_instance::update_transform(rnu::mat4 const& transform)
  {
    if (!_holder.expired())
      _holder.lock()->update_transform_internal(_byte_offset, transform);
  }

  void distance_field_holder::update_transform_internal(std::size_t offset, rnu::mat4 transform)
  {
    transform = inverse(transform);

    df_info info;
    std::byte* begin = reinterpret_cast<std::byte*>(_field_instances.data()) + offset;
    std::memcpy(&info, begin, sizeof(df_info));

    if ((transform != info.inverse_transform).any())
    {
      std::memcpy(begin, transform.data(), sizeof(rnu::mat4));
      include_update_region(offset, offset + sizeof(rnu::mat4));
    }
  }

  distance_field_holder::distance_field_holder()
  {
    vk::DescriptorPoolSize sizes[] = {
      {vk::DescriptorType::eStorageBuffer, 1},
      {vk::DescriptorType::eCombinedImageSampler, max_num_distance_fields},
    };
    _field_pool = gev::engine::get().device().createDescriptorPoolUnique(
      vk::DescriptorPoolCreateInfo().setMaxSets(1).setPoolSizes(sizes).setFlags(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet));
    _field_layout =
      gev::descriptor_layout_creator::get()
        .bind(binding_instances, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
        .bind(binding_fields, vk::DescriptorType::eCombinedImageSampler, max_num_distance_fields,
          vk::ShaderStageFlagBits::eAllGraphics, vk::DescriptorBindingFlagBits::ePartiallyBound)
        .build();
    _field_descriptor = std::move(gev::engine::get().device().allocateDescriptorSetsUnique(
      vk::DescriptorSetAllocateInfo().setDescriptorPool(*_field_pool).setSetLayouts(*_field_layout))[0]);

    _field_instances.push_back({.field_id = -1});
    _update_region_start = 0;
    _update_region_end = sizeof(df_info);
  }

  field_id distance_field_holder::register_field(std::shared_ptr<distance_field> field)
  {
    auto& f = _fields.emplace_back(std::move(field));
    gev::update_descriptor(_field_descriptor.get(), binding_fields,
      vk::DescriptorImageInfo()
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImageView(f->image_view())
        .setSampler(f->sampler()),
      vk::DescriptorType::eCombinedImageSampler, std::uint32_t(_fields.size() - 1));
    return field_id(_fields.size() - 1);
  }

  vk::DescriptorSet distance_field_holder::descriptor() const
  {
    return _field_descriptor.get();
  }

  vk::DescriptorSetLayout distance_field_holder::layout() const
  {
    return _field_layout.get();
  }

  std::shared_ptr<distance_field_instance> distance_field_holder::instantiate(field_id field, rnu::mat4 transform)
  {
    auto const id = std::size_t(field);
    auto const& f = _fields[id];

    auto& info = _field_instances[_field_instances.size() - 1];
    info.inverse_transform = inverse(transform);
    info.bmin = f->bounds().lower();
    info.field_id = id;
    info.bmax = f->bounds().upper();
    info.metadata = 0;
    _field_instances.push_back({.field_id = -1});

    auto const begin = (_field_instances.size() - 2) * sizeof(df_info);
    include_update_region(begin, _field_instances.size() * sizeof(df_info));

    auto instance = std::make_shared<distance_field_instance>();
    instance->_byte_offset = begin;
    instance->_holder = shared_from_this();
    return instance;
  }

  void distance_field_holder::include_update_region(std::size_t begin, std::size_t end)
  {
    _update_region_start = std::min(_update_region_start, begin);
    _update_region_end = std::max(_update_region_end, end);
  }

  void distance_field_holder::try_flush_buffer()
  {
    if (_instances_buffer && _update_region_end <= _update_region_start)
      return;

    if (!_instances_buffer || _update_region_end > _instances_buffer->size())
    {
      auto v = std::max(min_reserved_elements * sizeof(df_info), _update_region_end);
      v--;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      v |= v >> 16;
      v |= v >> 32;
      v++;

      _instances_buffer.reset();

      _instances_buffer = gev::buffer::host_accessible(v, vk::BufferUsageFlagBits::eStorageBuffer);
      gev::update_descriptor(_field_descriptor.get(), binding_instances,
        vk::DescriptorBufferInfo().setBuffer(_instances_buffer->get_buffer()).setOffset(0).setRange(v),
        vk::DescriptorType::eStorageBuffer);
    }

    if (_update_region_end > _update_region_start)
    {
      std::byte const* begin = reinterpret_cast<std::byte const*>(_field_instances.data()) + _update_region_start;
      _instances_buffer->load_data(begin, _update_region_end - _update_region_start, _update_region_start);
    }

    _update_region_start = std::numeric_limits<std::size_t>::max();
    _update_region_end = std::numeric_limits<std::size_t>::lowest();
  }
}    // namespace gev::game