#include <gev/descriptors.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/mesh_batch.hpp>

namespace gev::game
{
  void mesh_instance::destroy()
  {
    if (!_holder.expired())
    {
      _holder.lock()->destroy(*this);
      _mesh.reset();
      _holder.reset();
      _byte_offset = 0;
    }
  }

  void mesh_instance::update_transform(rnu::mat4 const& transform)
  {
    if (!_holder.expired())
      _holder.lock()->update_transform_internal(_byte_offset, transform);
  }

  mesh_batch::mesh_batch()
  {
    vk::DescriptorPoolSize sizes[] = {
      {vk::DescriptorType::eStorageBuffer, 1},
    };
    _mesh_pool = gev::engine::get().device().createDescriptorPoolUnique(
      vk::DescriptorPoolCreateInfo().setMaxSets(1).setPoolSizes(sizes).setFlags(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet));
    auto const layout = layouts::defaults().object_set_layout();
    _mesh_descriptor = std::move(gev::engine::get().device().allocateDescriptorSetsUnique(
      vk::DescriptorSetAllocateInfo().setDescriptorPool(*_mesh_pool).setSetLayouts(layout))[0]);
  }

  void mesh_batch::update_transform_internal(std::size_t offset, rnu::mat4 transform)
  {
    auto const inv = inverse(transform);

    auto const& index = offset / sizeof(mesh_info);

    if ((_mesh_infos[index].transform != transform).any())
    {
      _mesh_infos[index].transform = transform;
      _mesh_infos[index].inverse_transform = inv;
      include_update_region(offset, offset + sizeof(mesh_info));
    }
  }

  std::shared_ptr<mesh_instance> mesh_batch::instantiate(std::shared_ptr<mesh> const& id, rnu::mat4 transform)
  {
    auto iter = _instance_refs.find(id);
    if (iter == _instance_refs.end())
    {
      iter = _instance_refs.emplace_hint(
        iter, id, mesh_ref{.first_instance = _instances.size() * sizeof(mesh_info), .instance_count = 0});
    }

    auto& fi = iter->second;
    for (auto& i : _instance_refs)
    {
      if (i.first != id && i.second.first_instance >= fi.first_instance)
        i.second.first_instance += sizeof(mesh_info);
    }

    auto const increase_from = fi.first_instance + fi.instance_count * sizeof(mesh_info);

    for (auto iter = _instances.cbegin(); iter != _instances.cend(); ++iter)
    {
      if (iter->get()->_byte_offset < increase_from)
        break;

      iter->get()->_byte_offset += sizeof(mesh_info);
    }

    auto const insert_byte = fi.first_instance + fi.instance_count * sizeof(mesh_info);
    fi.instance_count += 1;

    auto const instance = std::make_shared<mesh_instance>();
    instance->_byte_offset = insert_byte;
    instance->_holder = shared_from_this();
    instance->_mesh = id;
    auto const insert_index = insert_byte / sizeof(mesh_info);
    _instances.insert(std::next(begin(_instances), insert_index), instance);
    _mesh_infos.insert(std::next(begin(_mesh_infos), insert_index),
      mesh_info{.transform = transform, .inverse_transform = inverse(transform)});

    include_update_region(insert_byte, _instances.size() * sizeof(mesh_info));

    return instance;
  }

  void mesh_batch::destroy(std::shared_ptr<mesh_instance> instance)
  {
    destroy(*instance);
  }

  void mesh_batch::destroy(mesh_instance const& instance)
  {
    auto iter = _instance_refs.find(instance._mesh);
    if (iter == _instance_refs.end())
      return;

    auto const& m = instance._mesh;
    auto const byte_offset = instance._byte_offset;

    for (auto& i : _instance_refs)
    {
      if (i.first != m && i.second.first_instance >= byte_offset)
        i.second.first_instance -= sizeof(mesh_info);
    }

    for (auto iter = _instances.cbegin(); iter != _instances.cend(); ++iter)
    {
      if (iter->get()->_byte_offset < byte_offset)
        break;

      iter->get()->_byte_offset -= sizeof(mesh_info);
    }

    iter->second.instance_count -= 1;
    if (iter->second.instance_count == 0)
      _instance_refs.erase(iter);

    auto const remove_index = byte_offset / sizeof(mesh_info);
    _instances.erase(next(begin(_instances), remove_index));
    _mesh_infos.erase(next(begin(_mesh_infos), remove_index));
  }

  vk::DescriptorSet mesh_batch::descriptor() const
  {
    return _mesh_descriptor.get();
  }

  void mesh_batch::render(vk::CommandBuffer c)
  {
    for (auto const& ref : _instance_refs)
    {
      auto const first_index = ref.second.first_instance / sizeof(mesh_info);
      ref.first->draw(c, ref.second.instance_count, first_index);
    }
  }

  void mesh_batch::try_flush_buffer()
  {
    if (_instances_buffer && _update_region_end <= _update_region_start)
      return;

    if (!_instances_buffer || _update_region_end > _instances_buffer->size())
    {
      auto v = std::max(min_reserved_elements * sizeof(mesh_info), _update_region_end);
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
      gev::update_descriptor(
        _mesh_descriptor.get(), binding_instances, *_instances_buffer, vk::DescriptorType::eStorageBuffer);
    }

    if (_update_region_end > _update_region_start)
    {
      std::byte const* begin = reinterpret_cast<std::byte const*>(_mesh_infos.data()) + _update_region_start;
      _instances_buffer->load_data(begin, _update_region_end - _update_region_start, _update_region_start);
    }

    _update_region_start = std::numeric_limits<std::size_t>::max();
    _update_region_end = std::numeric_limits<std::size_t>::lowest();
  }

  void mesh_batch::include_update_region(std::size_t begin, std::size_t end)
  {
    _update_region_start = std::min(_update_region_start, begin);
    _update_region_end = std::max(_update_region_end, end);
  }
}    // namespace gev::game