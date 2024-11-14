#pragma once

#include <emscripten/em_types.h>
#include <webgpu/webgpu_cpp.h>
#include "logstorm/logstorm_forward.h"
#include "vectorstorm/vector/vector2.h"

namespace render {

class webgpu_renderer {
  logstorm::manager &logger;

  struct {
    wgpu::Surface surface;                                                      // the canvas surface for rendering
    wgpu::Adapter adapter;                                                      // WebGPU adapter once it has been acquired
    wgpu::Device device;                                                        // WebGPU device once it has been acquired
    wgpu::Queue queue;                                                          // the queue for this device, once it has been acquired
    wgpu::BindGroupLayout bind_group_layout;                                    // layout for the uniform bind group
    wgpu::RenderPipeline pipeline;                                              // the render pipeline currently in use

    wgpu::SwapChain swapchain;                                                  // the swapchain providing a texture view to render to

    wgpu::Texture depth_texture;                                                // depth buffer
    wgpu::TextureView depth_texture_view;

    wgpu::TextureFormat surface_preferred_format{wgpu::TextureFormat::Undefined}; // preferred texture format for this surface
  } webgpu;

  struct {
    //GLFWwindow *glfw_window{nullptr};                                           // GLFW handle for the window

    vec2ui viewport_size;                                                       // our idea of the size of the viewport we render to, in real pixels
    vec2i canvas_size;                                                          // implementation-reported canvas size
    vec2ui document_body_size;                                                  // these sizes are before pixel ratio scaling, i.e. they change when the browser window is zoomed
    vec2ui window_inner_size;
    vec2ui window_outer_size;
    // TODO: get rid of whatever we don't need here
    float device_pixel_ratio{1.0f};
  } window;

  std::function<void()> main_loop_callback;

public:
  webgpu_renderer(logstorm::manager &logger);

  void init(std::function<void()> &&main_loop_callback);

private:
  void init_swapchain();
  void init_depth_texture();

  void wait_to_configure_loop();
  void configure();

public:
  void draw();
};

}
