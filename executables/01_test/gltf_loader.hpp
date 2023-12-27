#pragma once
#include <gev/scenery/gltf.hpp>
#include <gev/scenery/entity.hpp>

std::shared_ptr<gev::scenery::entity> load_gltf_entity(std::filesystem::path const& path);