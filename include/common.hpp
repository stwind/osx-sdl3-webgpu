#pragma once

#include "wgpu.hpp"
#include "imgui.hpp"

class WGPUApplication {
public:
  WGPU::Context ctx;

  WGPUApplication(int w, int h) : ctx(w, h) {
    if (!ImGui_init(&ctx)) throw std::runtime_error("ImGui_init failed");
  }

  ~WGPUApplication() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }

  void processEvent(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
  }
};