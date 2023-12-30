#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/addition.hpp>
#include <gev/pipeline.hpp>
#include <gev_game_shaders_files.hpp>

namespace gev::game
{
  addition::addition()
  {
    _set_layout =
      gev::descriptor_layout_creator::get()
        .flags(vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR)
        .bind(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute)
        .bind(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute)
        .bind(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
        .build();

    vk::PushConstantRange options_range;
    options_range.offset = 0;
    options_range.stageFlags = vk::ShaderStageFlagBits::eCompute;
    options_range.size = sizeof(float);
    _layout = gev::create_pipeline_layout(_set_layout.get(), options_range);

    struct spec_info_struct
    {
      std::uint32_t group_size_x;
      std::uint32_t group_size_y;
    } spec{8, 8};

    vk::SpecializationMapEntry entries[] = {
      vk::SpecializationMapEntry(1u, offsetof(spec_info_struct, group_size_x), sizeof(std::uint32_t)),
      vk::SpecializationMapEntry(2u, offsetof(spec_info_struct, group_size_y), sizeof(std::uint32_t)),
    };
    vk::SpecializationInfo spec_info;
    spec_info.setData<spec_info_struct>(spec);
    spec_info.setMapEntries(entries);

    auto const shader = gev::create_shader(gev::load_spv(gev_game_shaders::shaders::addition_comp));
    _pipeline = gev::build_compute_pipeline(_layout.get(), shader.get(), "main", &spec_info);
    _sampler = gev::engine::get().device().createSamplerUnique(
      vk::SamplerCreateInfo()
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
        .setAnisotropyEnable(false)
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setUnnormalizedCoordinates(false)
        .setMinLod(0)
        .setMaxLod(1000));
  }

  void addition::apply(vk::CommandBuffer c, float factor, render_target_2d const& src1, render_target_2d const& src2,
    render_target_2d const& dst)
  {
    src1.image()->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderSampledRead);
    src2.image()->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderSampledRead);
    dst.image()->layout(c, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageWrite);

    c.bindPipeline(vk::PipelineBindPoint::eCompute, _pipeline.get());
    c.pushConstants<float>(_layout.get(), vk::ShaderStageFlagBits::eCompute, 0, factor);

    vk::DescriptorImageInfo img01;
    img01.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(src1.view()).setSampler(_sampler.get());
    vk::DescriptorImageInfo img02;
    img02.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(src2.view()).setSampler(_sampler.get());
    vk::DescriptorImageInfo img1;
    img1.setImageLayout(vk::ImageLayout::eGeneral).setImageView(dst.view());

    vk::WriteDescriptorSet const set01 =
      vk::WriteDescriptorSet()
        .setDstSet(nullptr)
        .setDstBinding(0)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setImageInfo(img01);
    vk::WriteDescriptorSet const set02 =
      vk::WriteDescriptorSet()
        .setDstSet(nullptr)
        .setDstBinding(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setImageInfo(img02);
    vk::WriteDescriptorSet const set1 =
      vk::WriteDescriptorSet()
        .setDstSet(nullptr)
        .setDstBinding(2)
        .setDescriptorType(vk::DescriptorType::eStorageImage)
        .setImageInfo(img1);
    c.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, _layout.get(), 0, {set01, set02, set1});
    c.dispatch((src1.image()->extent().width + 7) / 8, (src1.image()->extent().height + 7) / 8, 1);
  }
}    // namespace gev::game