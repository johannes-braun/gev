#include "gltf_loader.hpp"

#include "components/bone_component.hpp"
#include "components/debug_ui_component.hpp"
#include "components/renderer_component.hpp"
#include "components/skin_component.hpp"

#include <gev/engine.hpp>
#include <gev/game/mesh_renderer.hpp>
#include <gev/game/shader.hpp>
#include <gev/scenery/entity_manager.hpp>

std::shared_ptr<gev::scenery::entity> child_from_node(std::shared_ptr<gev::scenery::entity> e, std::uint32_t node_index,
  gev::scenery::transform_node const& node, gev::scenery::gltf_data const& gltf)
{
  auto mesh_renderer = gev::service<gev::game::mesh_renderer>();
  auto entity_manager = gev::service<gev::scenery::entity_manager>();
  auto shader_repo = gev::service<gev::game::shader_repo>();

  auto const skinned_shader = shader_repo->get(gev::game::shaders::skinned);
  auto const default_shader = shader_repo->get(gev::game::shaders::standard);

  auto ptcl = entity_manager->instantiate(e);
  ptcl->local_transform = node.matrix();
  auto const data = std::any_cast<gev::scenery::node_data>(node.data);
  ptcl->emplace<debug_ui_component>(data.name);

  if (gltf.skins[0].root_node() == node_index)
  {
    auto const s = ptcl->emplace<skin_component>(gltf.skins[0], gltf.nodes, gltf.animations);
    skinned_shader->attach_always(s->skin_descriptor(), 4);
  }
  else if (auto const node = gltf.skins[0].find_joint_index(node_index))
  {
    ptcl->emplace<bone_component>(*node);
#ifdef GLTF_LOADER_VISUALIZE_BONES
    auto rc04 = ptcl->emplace<renderer_component>();
    rc04->set_renderer(mesh_renderer);
    rc04->set_shader(default_shader);
    rc04->set_mesh(std::make_shared<gev::game::mesh>("res/bone.obj"));
    rc04->set_material(std::make_shared<gev::game::material>());
#endif
  }

  if (data.mesh != -1)
  {
    for (int i = 0; i < gltf.geometries[data.mesh].size(); ++i)
    {
      auto const& mesh = gltf.geometries[data.mesh][i];
      auto mesh_child = entity_manager->instantiate(ptcl);
      mesh_child->emplace<debug_ui_component>(data.name);
      rnu::triangulated_object_t obj;
      obj.positions = mesh.positions;
      obj.normals = mesh.normals;
      obj.texcoords = mesh.texcoords;
      obj.indices = mesh.indices;
      auto r = mesh_child->emplace<renderer_component>();
      r->set_renderer(mesh_renderer);
      r->set_shader(skinned_shader);

      auto const& gltf_material = gltf.materials[mesh.material_id];
      auto material = std::make_shared<gev::game::material>();
      material->set_diffuse(gltf_material.data.color);

      if (gltf_material.data.has_texture)
      {
        material->load_diffuse(std::make_shared<gev::game::texture>(gltf_material.diffuse_map.pixels,
          std::uint32_t(gltf_material.diffuse_map.width), std::uint32_t(gltf_material.diffuse_map.height)));
      }

      r->set_material(material);
      r->set_mesh(std::make_shared<gev::game::mesh>(obj));
      r->get_mesh()->make_skinned(mesh.joints);
    }
  }

  return ptcl;
}

void emplace_children(std::shared_ptr<gev::scenery::entity> e, gev::scenery::transform_node const& node,
  gev::scenery::gltf_data const& gltf)
{
  for (std::size_t i = 0; i < node.num_children; ++i)
  {
    auto const index = i + node.children_offset;
    auto const& child = gltf.nodes.nodes()[index];
    auto const ptcl = child_from_node(e, index, child, gltf);
    emplace_children(ptcl, child, gltf);
  }
}

std::shared_ptr<gev::scenery::entity> load_gltf_entity(std::filesystem::path const& path)
{
  auto gltf = gev::scenery::load_gltf(path);
  gev::scenery::transform_node const root = gltf.nodes.nodes()[0];
  auto const data = std::any_cast<gev::scenery::node_data>(root.data);
  auto const root_entity = child_from_node(nullptr, 0, root, gltf);
  emplace_children(root_entity, root, gltf);
  return root_entity;
}