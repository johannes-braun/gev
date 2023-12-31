#include <experimental/generator>
#include <gev/scenery/gltf.hpp>
#include <iostream>
#include <span>
#include <utility>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace gev::scenery
{
  struct gltf_load_state
  {
    std::vector<size_t> relocations;
  };

  template<std::integral I>
  auto const copy_int(char const* src, std::uint32_t& dst)
  {
    I i{};
    std::memcpy(&i, src, sizeof(I));
    dst = static_cast<std::uint32_t>(i);
  }

  std::pair<ptrdiff_t, size_t> insert_children(gltf_load_state& state, std::vector<transform_node>& nodes,
    std::vector<bool>& done, tinygltf::Model const& model, int node)
  {
    auto const& child = model.nodes[node];

    // apply transformation
    if (child.matrix.size() == 16)
    {
      rnu::mat4d mat;
      memcpy(&mat, child.matrix.data(), sizeof(rnu::mat4d));
      nodes[state.relocations[node]].transformation = rnu::mat4(mat);
    }
    else
    {
      rnu::quatd rot{1, 0, 0, 0};
      rnu::vec3d scale{1, 1, 1};
      rnu::vec3d trl{0, 0, 0};
      if (!child.rotation.empty())
      {
        rnu::vec4d rot_tmp;
        memcpy(rot_tmp.data(), child.rotation.data(), sizeof(rnu::vec4d));
        rot = normalize(rnu::quatd(rot_tmp.w, rot_tmp.x, rot_tmp.y, rot_tmp.z));
      }
      if (!child.translation.empty())
        memcpy(trl.data(), child.translation.data(), sizeof(rnu::vec3d));
      if (!child.scale.empty())
        memcpy(scale.data(), child.scale.data(), sizeof(rnu::vec3d));

      transform_node::loc_rot_scale lrs;
      lrs.location = trl;
      lrs.rotation = rot;
      lrs.scale = scale;
      nodes[state.relocations[node]].transformation = lrs;
    }

    node_data data;
    data.mesh = child.mesh;
    data.name = child.name;
    nodes[state.relocations[node]].data = std::move(data);

    // first, insert all child nodes, then recurse
    auto const base_offset = nodes.size();
    auto count = child.children.size();
    for (auto& c : child.children)
    {
      state.relocations[c] = nodes.size();
      nodes.emplace_back();
      nodes.back().parent = state.relocations[node];
    }

    for (int i = 0; i < child.children.size(); ++i)
    {
      auto& n = nodes[base_offset + i];
      auto [offset, count] = insert_children(state, nodes, done, model, child.children[i]);
      n.children_offset = offset;
      n.num_children = count;
    }

    done[node] = true;
    return {base_offset, count};
  }
  transform_tree extract_nodes(gltf_load_state& state, tinygltf::Model const& model)
  {
    std::vector<transform_node> nodes;
    nodes.reserve(model.nodes.size());
    state.relocations.resize(model.nodes.size());
    std::vector<bool> done(model.nodes.size());

    std::vector<int> root_indices;
    {
      std::vector<bool> is_root(model.nodes.size(), true);
      for (int i = 0; i < is_root.size(); ++i)
      {
        for (auto const& c : model.nodes[i].children)
          is_root[c] = false;
      }

      for (int i = 0; i < is_root.size(); ++i)
        if (is_root[i])
          root_indices.push_back(i);
    }
    std::vector<std::pair<ptrdiff_t, size_t>> roots;

    for (auto i : root_indices)
    {
      state.relocations[i] = nodes.size();
      nodes.emplace_back();
      nodes.back().parent = -1;
      auto const [offset, count] = insert_children(state, nodes, done, model, i);
      nodes[state.relocations[i]].children_offset = offset;
      nodes[state.relocations[i]].num_children = count;
    }
    return transform_tree{nodes};
  }
  std::vector<std::pair<size_t, std::span<char const>>> elements_of(
    tinygltf::Model const& model, tinygltf::Accessor const& acc)
  {
    auto const& bufv = model.bufferViews[acc.bufferView];
    auto const& buf = model.buffers[bufv.buffer];
    auto const stride = acc.ByteStride(bufv);
    auto const size = tinygltf::GetComponentSizeInBytes(acc.componentType) * tinygltf::GetNumComponentsInType(acc.type);
    auto const base_ptr = reinterpret_cast<char const*>(buf.data.data()) + bufv.byteOffset + acc.byteOffset;
    std::vector<std::pair<size_t, std::span<char const>>> v(acc.count);
    for (size_t i = 0; i < acc.count; ++i)
    {
      auto const ptr = base_ptr + i * stride;
      v[i] = (std::pair<size_t, std::span<char const>>(i, std::span<char const>(ptr, size)));
    }
    return v;
  }
  std::unordered_map<std::string, std::vector<animation>> extract_animations(
    gltf_load_state& state, tinygltf::Model const& model)
  {
    std::unordered_map<std::string, std::vector<animation>> anims;

    for (auto const& anim : model.animations)
    {
      anims[anim.name].reserve(anim.samplers.size());
      auto& dst = anims[anim.name];
      for (int i = 0; i < anim.samplers.size(); ++i)
      {
        tinygltf::AnimationChannel const& channel = anim.channels[i];
        tinygltf::AnimationSampler const& sampler = anim.samplers[channel.sampler];

        auto const node = state.relocations[channel.target_node];
        auto const type = channel.target_path == "translation" ?
          animation_target::location :
          (channel.target_path == "rotation" ?
              animation_target::rotation :
              (channel.target_path == "scale" ? animation_target::scale : animation_target::location));
        auto& inacc = model.accessors[sampler.input];

        std::vector<float> timestamps(inacc.count);

        for (auto [item, data] : elements_of(model, inacc))
          memcpy(&timestamps[item], data.data(), data.size());

        auto& outacc = model.accessors[sampler.output];

        animation::checkpoints checkpoints;

        switch (type)
        {
          case animation_target::location:
          case animation_target::scale: checkpoints.emplace<0>(outacc.count); break;
          case animation_target::rotation: checkpoints.emplace<1>(outacc.count); break;
        }

        for (auto [item, data] : elements_of(model, outacc))
        {
          switch (type)
          {
            case animation_target::location:
            case animation_target::scale:
              memcpy(std::get<0>(checkpoints)[item].data(), data.data(), data.size());
              break;
            case animation_target::rotation:
            {
              rnu::vec4 r;
              memcpy(r.data(), data.data(), data.size());
              std::get<1>(checkpoints)[item] = rnu::normalize(rnu::quat(r.w, r.x, r.y, r.z));
              break;
            }
          }
        }

        dst.emplace_back(type, node, std::move(checkpoints), std::move(timestamps));
      }
    }
    return anims;
  }
  std::vector<std::vector<geometry_data>> extract_geometries(tinygltf::Model const& model)
  {
    std::vector<std::vector<geometry_data>> geometries(model.meshes.size());
    for (std::size_t i = 0; i < model.meshes.size(); ++i)
    {
      auto const& mesh = model.meshes[i];
      for (auto const& prim : mesh.primitives)
      {
        auto const pos_acc = model.accessors[prim.attributes.at("POSITION")];

        auto& geo = geometries[i].emplace_back();
        geo.material_id = prim.material;
        geo.positions.clear();
        geo.positions.resize(pos_acc.count);

        for (auto [i, data] : elements_of(model, pos_acc))
        {
          memcpy(geo.positions[i].data(), data.data(), data.size());
        }

        if (prim.attributes.contains("NORMAL"))
        {
          auto const nor_acc = model.accessors[prim.attributes.at("NORMAL")];
          geo.normals.clear();
          geo.normals.resize(nor_acc.count);

          for (auto [i, data] : elements_of(model, nor_acc))
            memcpy(geo.normals[i].data(), data.data(), data.size());
        }

        if (prim.attributes.contains("TEXCOORD_0"))
        {
          auto const texc_acc = model.accessors[prim.attributes.at("TEXCOORD_0")];
          geo.texcoords.clear();
          geo.texcoords.resize(texc_acc.count);

          for (auto [i, data] : elements_of(model, texc_acc))
            memcpy(geo.texcoords[i].data(), data.data(), data.size());
        }

        if (prim.attributes.contains("JOINTS_0"))
        {
          auto const joints_acc = model.accessors[prim.attributes.at("JOINTS_0")];
          geo.joints.clear();
          geo.joints.resize(joints_acc.count);

          for (auto [i, data] : elements_of(model, joints_acc))
            memcpy(geo.joints[i].indices.data(), data.data(), data.size());
          for (auto [i, data] : elements_of(model, model.accessors[prim.attributes.at("WEIGHTS_0")]))
            memcpy(geo.joints[i].weights.data(), data.data(), data.size());
        }

        auto const idx_acc = model.accessors[prim.indices];
        geo.indices.clear();
        geo.indices.resize(idx_acc.count);

        for (auto [i, data] : elements_of(model, idx_acc))
        {
          switch (idx_acc.componentType)
          {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: copy_int<std::uint16_t>(data.data(), geo.indices[i]); break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: copy_int<std::uint32_t>(data.data(), geo.indices[i]); break;
          }
        }
      }
    }
    return geometries;
  }
  std::vector<material> extract_materials(tinygltf::Model const& model)
  {
    std::vector<material> materials(model.materials.size());

    for (size_t i = 0; i < model.materials.size(); ++i)
    {
      auto diffuse_texture_index = model.materials[i].pbrMetallicRoughness.baseColorTexture.index;

      auto const ext_specular_glossiness = model.materials[i].extensions.find("KHR_materials_pbrSpecularGlossiness");
      if (diffuse_texture_index == -1)
      {
        if (ext_specular_glossiness != model.materials[i].extensions.end())
        {
          if (ext_specular_glossiness->second.Has("diffuseTexture"))
            diffuse_texture_index = ext_specular_glossiness->second.Get("diffuseTexture").Get("index").GetNumberAsInt();
          // Todo: Whatever
          else if (ext_specular_glossiness->second.Has("specularGlossinessTexture"))
            diffuse_texture_index =
              ext_specular_glossiness->second.Get("specularGlossinessTexture").Get("index").GetNumberAsInt();
        }
      }

      rnu::vec4 diff_factor{0, 0, 0, 0};
      if (model.materials[i].pbrMetallicRoughness.baseColorFactor.size() == 4)
      {
        diff_factor[0] = model.materials[i].pbrMetallicRoughness.baseColorFactor[0];
        diff_factor[1] = model.materials[i].pbrMetallicRoughness.baseColorFactor[1];
        diff_factor[2] = model.materials[i].pbrMetallicRoughness.baseColorFactor[2];
        diff_factor[3] = model.materials[i].pbrMetallicRoughness.baseColorFactor[3];
      }

      if (ext_specular_glossiness != model.materials[i].extensions.end())
      {
        auto const diff_fac = ext_specular_glossiness->second.Get("diffuseFactor");
        diff_factor[0] = diff_fac.Get(0).GetNumberAsDouble();
        diff_factor[1] = diff_fac.Get(1).GetNumberAsDouble();
        diff_factor[2] = diff_fac.Get(2).GetNumberAsDouble();
        diff_factor[3] = diff_fac.Get(3).GetNumberAsDouble();
      }
      materials[i].data.color = diff_factor;

      if (diffuse_texture_index != -1)
      {
        auto& img = model.images[model.textures[diffuse_texture_index].source];

        materials[i].diffuse_map.width = img.width;
        materials[i].diffuse_map.height = img.height;
        materials[i].diffuse_map.pixels.resize(4 * img.width * img.height);
        std::copy(begin(img.image), end(img.image), materials[i].diffuse_map.pixels.begin());
        materials[i].data.has_texture = true;
      }
      else
      {
        materials[i].data.has_texture = false;
      }
    }
    return materials;
  }
  std::vector<skin> extract_skins(gltf_load_state& state, tinygltf::Model const& model)
  {
    std::vector<skin> result;
    result.reserve(model.skins.size());
    for (auto const& s : model.skins)
    {
      auto const inv_mat_acc = model.accessors[s.inverseBindMatrices];

      std::vector<rnu::mat4> joints_base(inv_mat_acc.count);

      for (auto [i, data] : elements_of(model, inv_mat_acc))
        memcpy(&joints_base[i], data.data(), data.size());

      auto const root = std::max(0, s.skeleton);

      std::vector<std::uint32_t> joint_nodes(s.joints.size());
      for (size_t i = 0; i < s.joints.size(); ++i)
        joint_nodes[i] = state.relocations[s.joints[i]];

      result.emplace_back(root, std::move(joint_nodes), std::move(joints_base));
    }
    return result;
  }
  gltf_data load_gltf(std::filesystem::path const& path)
  {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
    if (!err.empty())
      std::cerr << "GLTF [E]: " << err << '\n';
    if (!warn.empty())
      std::cerr << "GLTF [W]: " << warn << '\n';

    gltf_load_state state;

    gltf_data result{.nodes = extract_nodes(state, model),
      .materials = extract_materials(model),
      .skins = extract_skins(state, model),
      .geometries = extract_geometries(model)};

    const auto anims = extract_animations(state, model);
    for (auto& [k, v] : anims)
    {
      result.animations[k].set(v, false);
    }
    return result;
  }

}    // namespace gev::scenery