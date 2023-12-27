#include "blur.hpp"
#include <gev/descriptors.hpp>
#include <gev/pipeline.hpp>
#include <gev/engine.hpp>
#include <test_shaders_files.hpp>

blur::blur(blur_dir dir)
{
  _set_layout =
    gev::descriptor_layout_creator::get()
      .flags(vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR)
      .bind(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute)
      .bind(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
      .build();

  _layout = gev::create_pipeline_layout(_set_layout.get());

  vk::SpecializationMapEntry spec_entry;
  spec_entry.setConstantID(0).setOffset(0).setSize(sizeof(int));
  vk::SpecializationInfo spec_info;
  spec_info.setData<blur_dir>(dir);
  spec_info.setMapEntries(spec_entry);

  auto const shader = gev::create_shader(gev::load_spv(test_shaders::shaders::blur_comp));
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

void blur::apply(
  vk::CommandBuffer c, gev::image& input, vk::ImageView input_view, gev::image& output, vk::ImageView output_view)
{
  auto const to1 = input.make_to_barrier();
  auto const to2 = output.make_to_barrier();

  input.layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderSampledRead);
  output.layout(
    c, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite);

  c.bindPipeline(vk::PipelineBindPoint::eCompute, _pipeline.get());

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