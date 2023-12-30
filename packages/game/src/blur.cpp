#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/blur.hpp>
#include <gev/pipeline.hpp>
#include <gev_game_shaders_files.hpp>

namespace gev::game
{
  blur::blur(blur_dir dir)
  {
    _set_layout =
      gev::descriptor_layout_creator::get()
        .flags(vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR)
        .bind(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute)
        .bind(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
        .build();

    vk::PushConstantRange options_range;
    options_range.offset = 0;
    options_range.stageFlags = vk::ShaderStageFlagBits::eCompute;
    options_range.size = sizeof(float);
    _layout = gev::create_pipeline_layout(_set_layout.get(), options_range);

    struct spec_info_struct
    {
      blur_dir dir;
      std::uint32_t group_size_x;
      std::uint32_t group_size_y;
    } spec{
      dir,
      8, 8
    };

    vk::SpecializationMapEntry entries[] = {
      vk::SpecializationMapEntry(0u, offsetof(spec_info_struct, dir), sizeof(blur_dir)),
      vk::SpecializationMapEntry(1u, offsetof(spec_info_struct, group_size_x), sizeof(std::uint32_t)),
      vk::SpecializationMapEntry(2u, offsetof(spec_info_struct, group_size_y), sizeof(std::uint32_t)),
    };
    vk::SpecializationInfo spec_info;
    spec_info.setData<spec_info_struct>(spec);
    spec_info.setMapEntries(entries);

    auto const shader = gev::create_shader(gev::load_spv(gev_game_shaders::shaders::blur_comp));
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

  void blur::apply(vk::CommandBuffer c, float step_size, gev::image& input, vk::ImageView input_view,
    gev::image& output, vk::ImageView output_view)
  {
    auto const to1 = input.make_to_barrier();
    auto const to2 = output.make_to_barrier();

    input.layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderSampledRead);
    output.layout(c, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageWrite);

    c.bindPipeline(vk::PipelineBindPoint::eCompute, _pipeline.get());
    c.pushConstants<float>(_layout.get(), vk::ShaderStageFlagBits::eCompute, 0, step_size);

    vk::DescriptorImageInfo img0;
    img0.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(input_view).setSampler(_sampler.get());
    vk::DescriptorImageInfo img1;
    img1.setImageLayout(vk::ImageLayout::eGeneral).setImageView(output_view);

    vk::WriteDescriptorSet const set0 =
      vk::WriteDescriptorSet()
        .setDstSet(nullptr)
        .setDstBinding(0)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setImageInfo(img0);
    vk::WriteDescriptorSet const set1 =
      vk::WriteDescriptorSet()
        .setDstSet(nullptr)
        .setDstBinding(1)
        .setDescriptorType(vk::DescriptorType::eStorageImage)
        .setImageInfo(img1);
    c.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, _layout.get(), 0, {set0, set1});
    c.dispatch((input.extent().width + 7) / 8, (input.extent().height + 7) / 8, 1);

    if (to1.newLayout != vk::ImageLayout::eUndefined)
      input.layout(c, to1.newLayout, to1.dstStageMask, to1.dstAccessMask);
    if (to2.newLayout != vk::ImageLayout::eUndefined)
      output.layout(c, to2.newLayout, to2.dstStageMask, to2.dstAccessMask);
  }
}    // namespace gev::game