#pragma once

#include "wgpu.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"

bool ImGui_init(WGPU::Context* ctx) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForOther(ctx->window);

  ImGui_ImplWGPU_InitInfo init_info;
  init_info.Device = ctx->device;
  init_info.RenderTargetFormat = ctx->surfaceFormat;
  return ImGui_ImplWGPU_Init(&init_info);
};

void ImGui_render(WGPU::Context* ctx, WGPUTextureView view) {
  WGPUCommandEncoder encoder = ctx->createCommandEncoder(new WGPUCommandEncoderDescriptor{});
  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, new WGPURenderPassDescriptor{
    .colorAttachmentCount = 1,
    .colorAttachments = new WGPURenderPassColorAttachment[1]{
      {
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .loadOp = WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
        .view = view,
      }
    },
    });
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
  wgpuCommandEncoderRelease(encoder);
  ctx->queueSubmit(1, &command);
  wgpuCommandBufferRelease(command);
};