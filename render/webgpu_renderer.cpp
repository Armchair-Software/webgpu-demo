#include "webgpu_renderer.h"
#include "logstorm/manager.h"
#include <array>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/val.h>
#include <magic_enum/magic_enum.hpp>
#include "vertex.h"
#include "triangle_index.h"
#include "uniforms.h"
#include "shaders/default.wgsl.h"

namespace render {

namespace {
mat4f make_projection_matrix(vec2f const &viewport_size) {
  /// Set up a projection matrix based on the FOV and the relevant planes
  enum class fov_mode_type {                                                    // how we set the field of view
    horizontal,                                                                 // field of view measures max horizontal angle of view
    vertical,                                                                   // field of view measures max vertical angle of view
    diagonal,                                                                   // field of view measures max diagonal angle of view
  } fov_mode{fov_mode_type::diagonal};

  float fov_angle{110.0f};                                                      // field of view, in degrees
  float fov_angle_rad{DEG2RAD(fov_angle)};                                      // field of view, in radians - automatically updated from degrees
  float clip_plane_near{1.0f};                                                  // near clip plane
  float clip_plane_far{100'000.0f};                                             // far clip plane

  #pragma GCC diagnostic push
  #pragma GCC diagnostic error "-Wswitch"                                       // enforce exhaustive switch here
  switch(fov_mode) {
  #pragma GCC diagnostic pop
  case fov_mode_type::horizontal:
    {
      float const aspect_ratio{viewport_size.y / viewport_size.x};
      float const right{std::tan(fov_angle_rad * 0.5f) * clip_plane_near};
      float const top{right * aspect_ratio};
      //logger << "DEBUG: horizontal aspect_ratio " << aspect_ratio;
      return mat4f::create_frustum(-right, right, -top, top, clip_plane_near, clip_plane_far);
    }
  case fov_mode_type::vertical:
    {
      float const aspect_ratio{viewport_size.x / viewport_size.y};
      float const top{std::tan(fov_angle_rad * 0.5f) * clip_plane_near};
      float const right{top * aspect_ratio};
      //logger << "DEBUG: vertical aspect_ratio " << aspect_ratio;
      return mat4f::create_frustum(-right, right, -top, top, clip_plane_near, clip_plane_far);
    }
  case fov_mode_type::diagonal:
    {
      float const diagonal{std::tan(fov_angle_rad * 0.5f) * clip_plane_near};
      float const viewport_diagonal{viewport_size.length()};
      float const right{diagonal * (viewport_size.x / viewport_diagonal)};
      float const top{  diagonal * (viewport_size.y / viewport_diagonal)};
      //logger << "DEBUG: diagonal aspect_ratio " << viewport_size.x / viewport_size.y;
      return mat4f::create_frustum(-right, right, -top, top, clip_plane_near, clip_plane_far);
    }
    // no default case, to enforce exhaustive switch
  }
  std::unreachable();
}

template<typename Tcpp, typename Tc>
  requires (!std::is_same_v<Tcpp, Tc>)
std::string enum_wgpu_name(Tc enum_in) {
  /// Attempt to interpret an enum into its most human-readable form, with fallbacks for unknown types
  /// Tc is the C API enum (WGPU...), Tcpp is the C++ API enum equivalent (wgpu::...)
  using value_type = std::underlying_type_t<Tc>;
  auto const enum_value{static_cast<value_type>(enum_in)};

  if(auto enum_out_opt{magic_enum::enum_cast<Tcpp>(enum_value)}; enum_out_opt.has_value()) { // first try to cast it to the C++ enum for clearest output
    return std::string{magic_enum::enum_name(*enum_out_opt)};
  }

  if(auto enum_out_opt{magic_enum::enum_cast<Tc>(enum_value)}; enum_out_opt.has_value()) { // fall back to trying the C enum interpretation
   return std::string{magic_enum::enum_name(*enum_out_opt)} + " (C binding only)";
  }

  std::ostringstream oss;
  oss << "unknown enum 0x" << std::hex << enum_value;                           // otherwise output the hex value and an explanatory note
  return oss.str();
}

template<typename Tenum>
std::string enum_wgpu_name(Tenum enum_in) {
  /// Interpret a C++ WebGPU enum directly
  if(auto const enum_name{magic_enum::enum_name(enum_in)}; !enum_name.empty()) {
    return std::string{enum_name};
  }

  std::ostringstream oss;
  oss << "unknown enum 0x" << std::hex << static_cast<std::underlying_type_t<Tenum>>(enum_in);
  return oss.str();
}

std::string_view string_wgpu(wgpu::StringView string_in) {
  /// Convert Dawn string views to standard string views for logging
  return static_cast<std::string_view>(string_in);
}

}

webgpu_renderer::webgpu_renderer(logstorm::manager &this_logger, std::function<void()> &&this_main_loop_callback)
  : logger{this_logger},
    main_loop_callback{this_main_loop_callback} {
  init();
}

void webgpu_renderer::init() {
  /// Initialise the WebGPU system
  // find out about the initial canvas size and the current window and doc sizes
  window.viewport_size.assign(emscripten::val::global("window")["innerWidth"].as<unsigned int>(),
                              emscripten::val::global("window")["innerHeight"].as<unsigned int>());
  window.device_pixel_ratio = emscripten::val::global("window")["devicePixelRatio"].as<float>(); // query device pixel ratio using JS
  logger << "render::window: Viewport size: " << window.viewport_size << " (device pixels: approx " << static_cast<vec2f>(window.viewport_size) * window.device_pixel_ratio << ")";
  logger << "render::window: Device pixel ratio: " << window.device_pixel_ratio << " canvas pixels to 1 device pixel (" << static_cast<unsigned int>(std::round(100.0f * window.device_pixel_ratio)) << "% zoom)";

  {
    // create a WebGPU instance
    wgpu::Instance instance{wgpu::CreateInstance()};
    if(!instance) throw std::runtime_error{"Could not initialize WebGPU"};

    // create a surface
    {
      wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector surface_descriptor_from_canvas;
      surface_descriptor_from_canvas.selector = "#canvas";

      wgpu::SurfaceDescriptor surface_descriptor{
        .nextInChain{&surface_descriptor_from_canvas},
        .label{"Canvas surface"},
      };
      webgpu.surface = instance.CreateSurface(&surface_descriptor);
    }
    if(!webgpu.surface) throw std::runtime_error{"Could not create WebGPU surface"};

    // request an adapter
    wgpu::RequestAdapterOptions adapter_request_options{
      .powerPreference{wgpu::PowerPreference::HighPerformance},
      .compatibleSurface{webgpu.surface},
    };

    instance.RequestAdapter(
      &adapter_request_options,
      wgpu::CallbackMode::AllowSpontaneous,
      [](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter_in, wgpu::StringView message, webgpu_renderer *renderer_ptr){
        /// Request adapter callback
        auto &renderer{*renderer_ptr};
        auto &logger{renderer.logger};
        auto &webgpu{renderer.webgpu};
        if(message.length) logger << "WebGPU: Request adapter callback message: " << string_wgpu(message);
        if(status != wgpu::RequestAdapterStatus::Success) {
          logger << "ERROR: WebGPU adapter request failure, status " << enum_wgpu_name<wgpu::RequestAdapterStatus>(status);
          throw std::runtime_error{"WebGPU: Could not get adapter"};
        }

        auto &adapter{webgpu.adapter};
        adapter = std::move(adapter_in);
        if(!adapter) throw std::runtime_error{"WebGPU: Could not acquire adapter"};

        wgpu::SurfaceCapabilities surface_capabilities;
        webgpu.surface.GetCapabilities(adapter, &surface_capabilities);

        // report surface and adapter capabilities
        #ifndef NDEBUG
          {
            for(size_t i{0}; i != surface_capabilities.formatCount; ++i) {
              logger << "DEBUG: WebGPU surface capabilities: texture formats: " << magic_enum::enum_name(surface_capabilities.formats[i]);
            }
            for(size_t i{0}; i != surface_capabilities.presentModeCount; ++i) {
              logger << "DEBUG: WebGPU surface capabilities: present modes: " << magic_enum::enum_name(surface_capabilities.presentModes[i]);
            }
            for(size_t i{0}; i != surface_capabilities.alphaModeCount; ++i) {
              logger << "DEBUG: WebGPU surface capabilities: alpha modes: " << magic_enum::enum_name(surface_capabilities.alphaModes[i]);
            }
          }
        #endif // NDEBUG

        if(surface_capabilities.formatCount != 0) {
          webgpu.surface_preferred_format = surface_capabilities.formats[0];
        }
        logger << "WebGPU surface preferred format for this adapter: " << magic_enum::enum_name(webgpu.surface_preferred_format);
        if(webgpu.surface_preferred_format == wgpu::TextureFormat::Undefined) {
          webgpu.surface_preferred_format = wgpu::TextureFormat::RGBA8Unorm;
          logger << "WebGPU manually specifying preferred format: " << magic_enum::enum_name(webgpu.surface_preferred_format);
        }

        {
          wgpu::AdapterInfo adapter_info;
          adapter.GetInfo(&adapter_info);
          #ifndef NDEBUG
            logger << "DEBUG: WebGPU adapter info: vendor: " << string_wgpu(adapter_info.vendor);
            logger << "DEBUG: WebGPU adapter info: architecture: " << string_wgpu(adapter_info.architecture);
            logger << "DEBUG: WebGPU adapter info: device: " << string_wgpu(adapter_info.device);
            logger << "DEBUG: WebGPU adapter info: description: " << string_wgpu(adapter_info.description);
            logger << "DEBUG: WebGPU adapter info: vendorID:deviceID: " << adapter_info.vendorID << ":" << adapter_info.deviceID;
            logger << "DEBUG: WebGPU adapter info: backendType: " << magic_enum::enum_name(adapter_info.backendType);
            logger << "DEBUG: WebGPU adapter info: adapterType: " << magic_enum::enum_name(adapter_info.adapterType);
          #endif // NDEBUG
          logger << "WebGPU adapter info: " << string_wgpu(adapter_info.description) << " (" << magic_enum::enum_name(adapter_info.backendType) << ", " << string_wgpu(adapter_info.vendor) << ", " << string_wgpu(adapter_info.architecture) << ")";
        }
        std::set<wgpu::FeatureName> adapter_features;
        {
          // see https://developer.mozilla.org/en-US/docs/Web/API/GPUSupportedFeatures and https://www.w3.org/TR/webgpu/#feature-index
          wgpu::SupportedFeatures adapter_supported_features;
          adapter.GetFeatures(&adapter_supported_features);
          logger << "DEBUG: WebGPU adapter features count: " << adapter_supported_features.featureCount;
          for(size_t i{0}; i != adapter_supported_features.featureCount; ++i) {
            adapter_features.emplace(adapter_supported_features.features[i]);
          }
        }
        for(auto const feature : adapter_features) {
          logger << "DEBUG: WebGPU adapter features: " << enum_wgpu_name(feature);
        }

        wgpu::Limits adapter_limits;
        bool const result{adapter.GetLimits(&adapter_limits)};
        if(!result) throw std::runtime_error{"WebGPU: Could not query adapter limits"};
        #ifndef NDEBUG
          logger << "DEBUG: WebGPU adapter limits result: " << std::boolalpha << result;
          logger << "DEBUG: WebGPU adapter limits nextInChain: " << adapter_limits.nextInChain;
          logger << "DEBUG: WebGPU adapter limits maxTextureDimension1D: " << adapter_limits.maxTextureDimension1D;
          logger << "DEBUG: WebGPU adapter limits maxTextureDimension2D: " << adapter_limits.maxTextureDimension2D;
          logger << "DEBUG: WebGPU adapter limits maxTextureDimension3D: " << adapter_limits.maxTextureDimension3D;
          logger << "DEBUG: WebGPU adapter limits maxTextureArrayLayers: " << adapter_limits.maxTextureArrayLayers;
          logger << "DEBUG: WebGPU adapter limits maxBindGroups: " << adapter_limits.maxBindGroups;
          logger << "DEBUG: WebGPU adapter limits maxBindGroupsPlusVertexBuffers: " << adapter_limits.maxBindGroupsPlusVertexBuffers;
          logger << "DEBUG: WebGPU adapter limits maxBindingsPerBindGroup: " << adapter_limits.maxBindingsPerBindGroup;
          logger << "DEBUG: WebGPU adapter limits maxDynamicUniformBuffersPerPipelineLayout: " << adapter_limits.maxDynamicUniformBuffersPerPipelineLayout;
          logger << "DEBUG: WebGPU adapter limits maxDynamicStorageBuffersPerPipelineLayout: " << adapter_limits.maxDynamicStorageBuffersPerPipelineLayout;
          logger << "DEBUG: WebGPU adapter limits maxSamplersPerShaderStage: " << adapter_limits.maxSamplersPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxStorageBuffersPerShaderStage: " << adapter_limits.maxStorageBuffersPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxStorageTexturesPerShaderStage: " << adapter_limits.maxStorageTexturesPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxUniformBuffersPerShaderStage: " << adapter_limits.maxUniformBuffersPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxUniformBufferBindingSize: " << adapter_limits.maxUniformBufferBindingSize;
          logger << "DEBUG: WebGPU adapter limits maxStorageBufferBindingSize: " << adapter_limits.maxStorageBufferBindingSize;
          logger << "DEBUG: WebGPU adapter limits minUniformBufferOffsetAlignment: " << adapter_limits.minUniformBufferOffsetAlignment;
          logger << "DEBUG: WebGPU adapter limits minStorageBufferOffsetAlignment: " << adapter_limits.minStorageBufferOffsetAlignment;
          logger << "DEBUG: WebGPU adapter limits maxVertexBuffers: " << adapter_limits.maxVertexBuffers;
          logger << "DEBUG: WebGPU adapter limits maxBufferSize: " << adapter_limits.maxBufferSize;
          logger << "DEBUG: WebGPU adapter limits maxVertexAttributes: " << adapter_limits.maxVertexAttributes;
          logger << "DEBUG: WebGPU adapter limits maxVertexBufferArrayStride: " << adapter_limits.maxVertexBufferArrayStride;
          logger << "DEBUG: WebGPU adapter limits maxInterStageShaderVariables: " << adapter_limits.maxInterStageShaderVariables;
          logger << "DEBUG: WebGPU adapter limits maxColorAttachments: " << adapter_limits.maxColorAttachments;
          logger << "DEBUG: WebGPU adapter limits maxColorAttachmentBytesPerSample: " << adapter_limits.maxColorAttachmentBytesPerSample;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupStorageSize: " << adapter_limits.maxComputeWorkgroupStorageSize;
          logger << "DEBUG: WebGPU adapter limits maxComputeInvocationsPerWorkgroup: " << adapter_limits.maxComputeInvocationsPerWorkgroup;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeX: " << adapter_limits.maxComputeWorkgroupSizeX;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeY: " << adapter_limits.maxComputeWorkgroupSizeY;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeZ: " << adapter_limits.maxComputeWorkgroupSizeZ;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupsPerDimension: " << adapter_limits.maxComputeWorkgroupsPerDimension;
        #endif // NDEBUG

        // specify required features for the device
        std::set<wgpu::FeatureName> required_features{
          wgpu::FeatureName::Depth32FloatStencil8,
          #ifndef NDEBUG
            wgpu::FeatureName::TimestampQuery,
          #endif // NDEBUG
          wgpu::FeatureName::TextureCompressionBC,
          wgpu::FeatureName::IndirectFirstInstance,
        };
        std::set<wgpu::FeatureName> desired_features{
          wgpu::FeatureName::ShaderF16,
          wgpu::FeatureName::Float32Filterable,
        };

        std::vector<wgpu::FeatureName> required_features_arr;
        for(auto const feature : required_features) {
          if(!adapter_features.contains(feature)) {
            logger << "WebGPU: Required adapter feature " << magic_enum::enum_name(feature) << " unavailable, cannot continue";
            throw std::runtime_error{"WebGPU: Required adapter feature " + std::string{magic_enum::enum_name(feature)} + " not available"};
          }
          logger << "WebGPU: Required adapter feature: " << magic_enum::enum_name(feature) << " requested";
          required_features_arr.emplace_back(feature);
        }
        for(auto const feature : desired_features) {
          if(!adapter_features.contains(feature)) {
            logger << "WebGPU: Desired adapter feature " << magic_enum::enum_name(feature) << " unavailable, continuing without it";
            continue;
          }
          logger << "WebGPU: Desired adapter feature " << magic_enum::enum_name(feature) << " requested";
          required_features_arr.emplace_back(feature);
        }

        // request a device
        wgpu::DeviceDescriptor device_descriptor;
        device_descriptor.requiredFeatureCount = required_features_arr.size();
        device_descriptor.requiredFeatures = required_features_arr.data();
        // The browser-facing requestDevice() path now validates requiredLimits directly.
        // This demo does not need limits above the WebGPU defaults.
        device_descriptor.requiredLimits = nullptr;
        device_descriptor.defaultQueue.label = "Default queue";
        device_descriptor.SetDeviceLostCallback(
          wgpu::CallbackMode::AllowSpontaneous,
          [](wgpu::Device const &, wgpu::DeviceLostReason reason, wgpu::StringView message, webgpu_renderer *renderer_ptr){
            /// Device lost callback
            auto &renderer{*renderer_ptr};
            auto &logger{renderer.logger};
            logger << "ERROR: WebGPU lost device, reason " << enum_wgpu_name<wgpu::DeviceLostReason>(reason) << ": " << string_wgpu(message);
          },
          &renderer
        );
        device_descriptor.SetUncapturedErrorCallback(
          [](wgpu::Device const &, wgpu::ErrorType type, wgpu::StringView message, webgpu_renderer *renderer_ptr){
            /// Uncaptured error callback
            auto &renderer{*renderer_ptr};
            auto &logger{renderer.logger};
            logger << "ERROR: WebGPU uncaptured error " << enum_wgpu_name<wgpu::ErrorType>(type) << ": " << string_wgpu(message);
          },
          &renderer
        );

        adapter.RequestDevice(
          &device_descriptor,
          wgpu::CallbackMode::AllowSpontaneous,
          [](wgpu::RequestDeviceStatus status, wgpu::Device device_in, wgpu::StringView message, webgpu_renderer *renderer_ptr){
            /// Request device callback
            auto &renderer{*renderer_ptr};
            auto &logger{renderer.logger};
            auto &webgpu{renderer.webgpu};
            if(message.length) logger << "WebGPU: Request device callback message: " << string_wgpu(message);
            if(status != wgpu::RequestDeviceStatus::Success) {
              logger << "ERROR: WebGPU device request failure, status " << enum_wgpu_name<wgpu::RequestDeviceStatus>(status);
              throw std::runtime_error{"WebGPU: Could not get adapter"};
            }
            auto &device{webgpu.device};
            device = std::move(device_in);

            // report device capabilities
            std::set<wgpu::FeatureName> device_features;
            {
              wgpu::SupportedFeatures device_supported_features;
              device.GetFeatures(&device_supported_features);
              #ifndef NDEBUG
                logger << "DEBUG: WebGPU device features count: " << device_supported_features.featureCount;
              #endif // NDEBUG
              for(size_t i{0}; i != device_supported_features.featureCount; ++i) {
                device_features.emplace(device_supported_features.features[i]);
              }
            }
            for(auto const feature : device_features) {
              logger << "DEBUG: WebGPU device features: " << magic_enum::enum_name(feature);
            }
            #ifndef NDEBUG
              {
                wgpu::Limits adapter_limits;
                bool result{device.GetLimits(&adapter_limits)};
                logger << "DEBUG: WebGPU device limits result: " << std::boolalpha << result;
                logger << "DEBUG: WebGPU device limits nextInChain: " << adapter_limits.nextInChain;
                logger << "DEBUG: WebGPU device limits maxTextureDimension1D: " << adapter_limits.maxTextureDimension1D;
                logger << "DEBUG: WebGPU device limits maxTextureDimension2D: " << adapter_limits.maxTextureDimension2D;
                logger << "DEBUG: WebGPU device limits maxTextureDimension3D: " << adapter_limits.maxTextureDimension3D;
                logger << "DEBUG: WebGPU device limits maxTextureArrayLayers: " << adapter_limits.maxTextureArrayLayers;
                logger << "DEBUG: WebGPU device limits maxBindGroups: " << adapter_limits.maxBindGroups;
                logger << "DEBUG: WebGPU device limits maxBindGroupsPlusVertexBuffers: " << adapter_limits.maxBindGroupsPlusVertexBuffers;
                logger << "DEBUG: WebGPU device limits maxBindingsPerBindGroup: " << adapter_limits.maxBindingsPerBindGroup;
                logger << "DEBUG: WebGPU device limits maxDynamicUniformBuffersPerPipelineLayout: " << adapter_limits.maxDynamicUniformBuffersPerPipelineLayout;
                logger << "DEBUG: WebGPU device limits maxDynamicStorageBuffersPerPipelineLayout: " << adapter_limits.maxDynamicStorageBuffersPerPipelineLayout;
                logger << "DEBUG: WebGPU device limits maxSamplersPerShaderStage: " << adapter_limits.maxSamplersPerShaderStage;
                logger << "DEBUG: WebGPU device limits maxStorageBuffersPerShaderStage: " << adapter_limits.maxStorageBuffersPerShaderStage;
                logger << "DEBUG: WebGPU device limits maxStorageTexturesPerShaderStage: " << adapter_limits.maxStorageTexturesPerShaderStage;
                logger << "DEBUG: WebGPU device limits maxUniformBuffersPerShaderStage: " << adapter_limits.maxUniformBuffersPerShaderStage;
                logger << "DEBUG: WebGPU device limits maxUniformBufferBindingSize: " << adapter_limits.maxUniformBufferBindingSize;
                logger << "DEBUG: WebGPU device limits maxStorageBufferBindingSize: " << adapter_limits.maxStorageBufferBindingSize;
                logger << "DEBUG: WebGPU device limits minUniformBufferOffsetAlignment: " << adapter_limits.minUniformBufferOffsetAlignment;
                logger << "DEBUG: WebGPU device limits minStorageBufferOffsetAlignment: " << adapter_limits.minStorageBufferOffsetAlignment;
                logger << "DEBUG: WebGPU device limits maxVertexBuffers: " << adapter_limits.maxVertexBuffers;
                logger << "DEBUG: WebGPU device limits maxBufferSize: " << adapter_limits.maxBufferSize;
                logger << "DEBUG: WebGPU device limits maxVertexAttributes: " << adapter_limits.maxVertexAttributes;
                logger << "DEBUG: WebGPU device limits maxVertexBufferArrayStride: " << adapter_limits.maxVertexBufferArrayStride;
                logger << "DEBUG: WebGPU device limits maxInterStageShaderVariables: " << adapter_limits.maxInterStageShaderVariables;
                logger << "DEBUG: WebGPU device limits maxColorAttachments: " << adapter_limits.maxColorAttachments;
                logger << "DEBUG: WebGPU device limits maxColorAttachmentBytesPerSample: " << adapter_limits.maxColorAttachmentBytesPerSample;
                logger << "DEBUG: WebGPU device limits maxComputeWorkgroupStorageSize: " << adapter_limits.maxComputeWorkgroupStorageSize;
                logger << "DEBUG: WebGPU device limits maxComputeInvocationsPerWorkgroup: " << adapter_limits.maxComputeInvocationsPerWorkgroup;
                logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeX: " << adapter_limits.maxComputeWorkgroupSizeX;
                logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeY: " << adapter_limits.maxComputeWorkgroupSizeY;
                logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeZ: " << adapter_limits.maxComputeWorkgroupSizeZ;
                logger << "DEBUG: WebGPU device limits maxComputeWorkgroupsPerDimension: " << adapter_limits.maxComputeWorkgroupsPerDimension;
              }
            #endif // NDEBUG

          },
          &renderer
        );
      },
      this
    );
  }

  emscripten_set_main_loop_arg([](void *data){
    /// Dispatch the loop waiting for WebGPU to become ready
    auto &renderer{*static_cast<webgpu_renderer*>(data)};
    renderer.wait_to_configure_loop();
  }, this, 0, true);                                                            // loop function, user data, FPS (0 to use browser requestAnimationFrame mechanism), simulate infinite loop
  std::unreachable();
}

void webgpu_renderer::configure_surface() {
  /// Create or recreate the configured surface for the current viewport size
  wgpu::SurfaceConfiguration surface_configuration{
    .device{webgpu.device},
    .format{webgpu.surface_preferred_format},
    .usage{wgpu::TextureUsage::RenderAttachment},
    .width{window.viewport_size.x},
    .height{window.viewport_size.y},
    .alphaMode{wgpu::CompositeAlphaMode::Auto},
    .presentMode{wgpu::PresentMode::Fifo},
  };
  webgpu.surface.Configure(&surface_configuration);
}

void webgpu_renderer::init_depth_texture() {
  /// Create or recreate the depth buffer and its texture view
  {
    constexpr auto depth_texture_format{wgpu::TextureFormat::Depth24Plus};
    wgpu::TextureDescriptor depth_texture_descriptor{
      .label{"Depth texture 1"},
      .usage{wgpu::TextureUsage::RenderAttachment},
      .dimension{wgpu::TextureDimension::e2D},
      .size{
        window.viewport_size.x,
        window.viewport_size.y,
        1
      },
      .format{wgpu::TextureFormat::Depth24Plus},
      .viewFormatCount{1},
      .viewFormats{&depth_texture_format},
    };
    webgpu.depth_texture = webgpu.device.CreateTexture(&depth_texture_descriptor);
  }
  {
    wgpu::TextureViewDescriptor depth_texture_view_descriptor{
      .label{"Depth texture view 1"},
      .format{wgpu::TextureFormat::Depth24Plus},
      .dimension{wgpu::TextureViewDimension::e2D},
      .mipLevelCount{1},
      .arrayLayerCount{1},
      .aspect{wgpu::TextureAspect::DepthOnly},
    };
    webgpu.depth_texture_view = webgpu.depth_texture.CreateView(&depth_texture_view_descriptor);
  }
}

void webgpu_renderer::wait_to_configure_loop() {
  /// Check if initialisation has completed and the WebGPU system is ready for configuration
  /// Since init occurs asynchronously, some emscripten ticks are needed before this becomes true
  if(!webgpu.device) {
    logger << "WebGPU: Waiting for device to become available";
    // TODO: sensible timeout
    return;
  }
  configure();

  logger << "WebGPU: Entering main loop";
  emscripten_cancel_main_loop();
  emscripten_set_main_loop_arg([](void *data){
    /// Main pseudo-loop waiting for initialisation to complete
    auto &renderer{*static_cast<webgpu_renderer*>(data)};
    renderer.main_loop_callback();
  }, this, 0, true);                                                            // loop function, user data, FPS (0 to use browser requestAnimationFrame mechanism), simulate infinite loop
  std::unreachable();
}

void webgpu_renderer::configure() {
  /// When the device is ready, configure the WebGPU system
  logger << "WebGPU device ready, configuring surface";
  configure_surface();

  logger << "WebGPU acquiring queue";
  webgpu.queue = webgpu.device.GetQueue();

  logger << "WebGPU assembling shaders";
  {
    wgpu::ShaderSourceWGSL shader_module_wgsl_decriptor;
    shader_module_wgsl_decriptor.code = render::shaders::default_wgsl;
    wgpu::ShaderModuleDescriptor shader_module_descriptor{
      .nextInChain{&shader_module_wgsl_decriptor},
      .label{"Shader module 1"},
    };
    wgpu::ShaderModule shader_module{webgpu.device.CreateShaderModule(&shader_module_descriptor)};

    logger << "WebGPU configuring pipeline";

    std::array vertex_attributes{
      wgpu::VertexAttribute{
        .format{wgpu::VertexFormat::Float32x3},
        .offset{offsetof(vertex, position)},
        .shaderLocation{0},
      },
      wgpu::VertexAttribute{
        .format{wgpu::VertexFormat::Float32x3},
        .offset{offsetof(vertex, normal)},
        .shaderLocation{1},
      },
      wgpu::VertexAttribute{
        .format{wgpu::VertexFormat::Float32x4},
        .offset{offsetof(vertex, colour)},
        .shaderLocation{2},
      },
    };
    wgpu::VertexBufferLayout vertex_buffer_layout{
      .arrayStride{sizeof(vertex)},
      .attributeCount{vertex_attributes.size()},
      .attributes{vertex_attributes.data()},
    };

    wgpu::BlendState blend_state{
      .color{                                                                   // BlendComponent
        .operation{wgpu::BlendOperation::Add},                                  // initial values from https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/hello-triangle.html
        .srcFactor{wgpu::BlendFactor::SrcAlpha},
        .dstFactor{wgpu::BlendFactor::OneMinusSrcAlpha},
      },
      .alpha{                                                                   // BlendComponent
        .operation{wgpu::BlendOperation::Add},                                  // these differ from defaults
        .srcFactor{wgpu::BlendFactor::Zero},
        .dstFactor{wgpu::BlendFactor::One},
        // TODO: compare with defaults
      },
    };
    wgpu::ColorTargetState colour_target_state{
      .format{webgpu.surface_preferred_format},
      .blend{&blend_state},
    };
    wgpu::FragmentState fragment_state{
      .module{shader_module},
      .entryPoint{"fs_main"},
      .constantCount{0},
      .constants{nullptr},
      .targetCount{1},
      .targets{&colour_target_state},
    };

    wgpu::DepthStencilState depth_stencil_state{
      .format{wgpu::TextureFormat::Depth24Plus},
      .depthWriteEnabled{true},
      .depthCompare{wgpu::CompareFunction::Less},
      .stencilFront{},                                                          // StencilFaceState
      .stencilBack{},                                                           // StencilFaceState
      .stencilReadMask{0},
      .stencilWriteMask{0},
      // TODO: tweak depth bias settings
    };

    wgpu::BindGroupLayoutEntry binding_layout{
      .binding{0},                                                              // binding index as used in the @binding attribute in the shader
      .visibility{wgpu::ShaderStage::Vertex},
      .buffer{                                                                  // BufferBindingLayout
        .type{wgpu::BufferBindingType::Uniform},
        .minBindingSize{sizeof(uniforms)},
      },
    };
    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor{
      .label{"Bind group layout 1"},
      .entryCount{1},
      .entries{&binding_layout},
    };
    webgpu.bind_group_layout = webgpu.device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{
      .label{"Pipeline layout 1"},
      .bindGroupLayoutCount{1},
      .bindGroupLayouts{&webgpu.bind_group_layout},
    };
    wgpu::PipelineLayout pipeline_layout{webgpu.device.CreatePipelineLayout(&pipeline_layout_descriptor)};

    wgpu::RenderPipelineDescriptor render_pipeline_descriptor{
      .label{"Render pipeline 1"},
      .layout{std::move(pipeline_layout)},
      .vertex{                                                                  // VertexState
        .module{shader_module},
        .entryPoint{"vs_main"},
        .constantCount{0},
        .constants{nullptr},
        .bufferCount{1},
        .buffers{&vertex_buffer_layout},
      },
      .primitive{                                                               // PrimitiveState
        .cullMode{wgpu::CullMode::Back},
      },
      .depthStencil{&depth_stencil_state},
      .multisample{},
      .fragment{&fragment_state},
    };
    webgpu.pipeline = webgpu.device.CreateRenderPipeline(&render_pipeline_descriptor);
  }

  init_depth_texture();

  emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, false,   // target, userdata, use_capture, callback
    ([](int /*event_type*/, EmscriptenUiEvent const *event, void *data) {       // event_type == EMSCRIPTEN_EVENT_RESIZE
      auto &renderer{*static_cast<webgpu_renderer*>(data)};
      renderer.window.viewport_size.x = static_cast<unsigned int>(event->windowInnerWidth);
      renderer.window.viewport_size.y = static_cast<unsigned int>(event->windowInnerHeight);

      renderer.configure_surface();
      renderer.init_depth_texture();
      return true;
    })
  );
}

void webgpu_renderer::draw() {
  /// Draw a frame
  wgpu::SurfaceTexture surface_texture;
  webgpu.surface.GetCurrentTexture(&surface_texture);
  if(surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal
     && surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
    throw std::runtime_error{"Could not get current texture from surface"};
  }

  wgpu::TextureView texture_view{surface_texture.texture.CreateView()};
  if(!texture_view) throw std::runtime_error{"Could not get current texture view from surface"};

  {
    wgpu::CommandEncoderDescriptor command_encoder_descriptor{
      .label = "Command encoder 1"
    };
    wgpu::CommandEncoder command_encoder{webgpu.device.CreateCommandEncoder(&command_encoder_descriptor)};

    {
      // set up render pass
      wgpu::RenderPassColorAttachment render_pass_colour_attachment{
        .view{texture_view},
        .loadOp{wgpu::LoadOp::Clear},
        .storeOp{wgpu::StoreOp::Store},
        .clearValue{wgpu::Color{0, 0.5, 0.5, 1.0}},
      };

      wgpu::RenderPassDepthStencilAttachment render_pass_depth_stencil_attachment{
        .view{webgpu.depth_texture_view},
        .depthLoadOp{wgpu::LoadOp::Clear},
        .depthStoreOp{wgpu::StoreOp::Store},
        .depthClearValue{1.0f},
      };
      wgpu::RenderPassDescriptor render_pass_descriptor{
        .label{"Render pass 1"},
        .colorAttachmentCount{1},
        .colorAttachments{&render_pass_colour_attachment},
        .depthStencilAttachment{&render_pass_depth_stencil_attachment},
      };
      wgpu::RenderPassEncoder render_pass_encoder{command_encoder.BeginRenderPass(&render_pass_descriptor)};

      render_pass_encoder.SetPipeline(webgpu.pipeline);                         // select which render pipeline to use

      // set up test buffers
      std::vector<vertex> vertex_data{
        {{-1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // bottom face normal & colour
        {{+1.0f, -1.0f, -1.0f}, {+1.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // right face normal & colour
        {{+1.0f, +1.0f, -1.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // front face normal & colour
        {{-1.0f, +1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // left face normal & colour
        {{-1.0f, -1.0f, +1.0f}, { 0.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // normal & colour not used
        {{+1.0f, -1.0f, +1.0f}, { 0.0f,  0.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // normal & colour not used
        {{+1.0f, +1.0f, +1.0f}, { 0.0f, +1.0f,  0.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // top face normal & colour
        {{-1.0f, +1.0f, +1.0f}, { 0.0f,  0.0f, +1.0f}, {1.0f, 0.75f, 0.0f, 1.0f}}, // back face normal & colour
      };
      std::vector<triangle_index> index_data{
        {0, 1, 5}, {0, 5, 4},                                                   // bottom face (y = -1)
        {1, 6, 5}, {1, 2, 6},                                                   // right face (x = +1)
        {2, 1, 0}, {2, 0, 3},                                                   // front face (z = -1)
        {3, 0, 4}, {3, 4, 7},                                                   // left face (x = -1)
        {6, 3, 7}, {6, 2, 3},                                                   // top face (y = +1)
        {7, 4, 5}, {7, 5, 6},                                                   // back face (z = +1)
      };

      // set up matrices
      static float angle{0.0f};
      angle += 0.01f;
      quatf model_rotation{quatf::from_euler_angles_rad(0.0, angle, 0.0)};

      vec3f camera_pos{0.0f, 2.0f, -5.0f};
      //camera_pos.rotate_rad_y(angle);

      mat4f projection{make_projection_matrix(static_cast<vec2f>(window.viewport_size))};
      mat4f look_at{mat4f::create_look_at(
        camera_pos,                                                             // eye pos
        {0.0f, 0.0f, 0.0f},                                                     // target pos
        {0.0f, 1.0f, 0.0f}                                                      // up dir
      )};

      uniforms uniform_data{
        projection * look_at * model_rotation.transform(),
        mat3fwgpu{model_rotation.rotmatrix()},
      };

      // vertex buffer
      wgpu::BufferDescriptor vertex_buffer_descriptor{
        .label{"Vertex buffer 1"},
        .usage{wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex},
        .size{vertex_data.size() * sizeof(vertex_data[0])},
      };
      wgpu::Buffer vertex_buffer{webgpu.device.CreateBuffer(&vertex_buffer_descriptor)};
      webgpu.queue.WriteBuffer(
        vertex_buffer,                                                          // buffer
        0,                                                                      // offset
        vertex_data.data(),                                                     // data
        vertex_data.size() * sizeof(vertex_data[0])                             // size
      );

      // index buffer
      wgpu::BufferDescriptor index_buffer_descriptor{
        .label{"Index buffer 1"},
        .usage{wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index},
        .size{index_data.size() * sizeof(index_data[0])},
      };
      wgpu::Buffer index_buffer{webgpu.device.CreateBuffer(&index_buffer_descriptor)};
      webgpu.queue.WriteBuffer(
        index_buffer,                                                           // buffer
        0,                                                                      // offset
        index_data.data(),                                                      // data
        index_data.size() * sizeof(index_data[0])                               // size
      );

      // uniform buffer
      wgpu::BufferDescriptor uniform_buffer_desecriptor{
        .label{"Uniform buffer 1"},
        .usage{wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform},
        .size{sizeof(uniform_data)},
      };
      wgpu::Buffer uniform_buffer{webgpu.device.CreateBuffer(&uniform_buffer_desecriptor)};
      webgpu.queue.WriteBuffer(
        uniform_buffer,                                                         // buffer
        0,                                                                      // offset
        &uniform_data,                                                          // data
        sizeof(uniform_data)                                                    // size
      );

      // uniform bind group
      wgpu::BindGroupEntry bind_group_entry{
        .binding{0},
        .buffer{uniform_buffer},
        .size{sizeof(uniforms)},
      };
      wgpu::BindGroupDescriptor bind_group_descriptor{
        .label{"Bind group 1"},
        .layout{webgpu.bind_group_layout},
        .entryCount{1},                                                         // must correspond to layout
        .entries{&bind_group_entry},
      };
      wgpu::BindGroup bind_group{webgpu.device.CreateBindGroup(&bind_group_descriptor)};

      render_pass_encoder.SetVertexBuffer(0, vertex_buffer, 0, vertex_buffer.GetSize()); // slot, buffer, offset, size
      render_pass_encoder.SetIndexBuffer(index_buffer, wgpu::IndexFormat::Uint16, 0, index_buffer.GetSize()); // buffer, format, offset, size
      render_pass_encoder.SetBindGroup(0, bind_group);                          // groupIndex, group, dynamicOffsetCount = 0, dynamicOffsets = nullptr
      render_pass_encoder.DrawIndexed(index_data.size() * decltype(index_data)::value_type::size()); // indexCount, instanceCount = 1, firstIndex = 0, baseVertex = 0, firstInstance = 0

      // TODO: add timestamp query: https://eliemichel.github.io/LearnWebGPU/advanced-techniques/benchmarking/time.html
      render_pass_encoder.End();
    }

    command_encoder.InsertDebugMarker("Debug marker 1");

    wgpu::CommandBufferDescriptor command_buffer_descriptor {
      .label = "Command buffer 1"
    };
    wgpu::CommandBuffer command_buffer{command_encoder.Finish(&command_buffer_descriptor)};

    //webgpu.queue.OnSubmittedWorkDone(
    //  [](WGPUQueueWorkDoneStatus status_c, void *data){
    //    /// Submitted work done callback - note, this only fires for the subsequent submit
    //    auto &renderer{*static_cast<webgpu_renderer*>(data)};
    //    auto &logger{renderer.logger};
    //    if(auto const status{static_cast<wgpu::QueueWorkDoneStatus>(status_c)}; status != wgpu::QueueWorkDoneStatus::Success) {
    //      logger << "ERROR: WebGPU queue submitted work failure, status: " << enum_wgpu_name<wgpu::QueueWorkDoneStatus>(status_c);
    //    }
    //    logger << "DEBUG: WebGPU queue submitted work done";
    //  },
    //  this
    //);

    webgpu.queue.Submit(1, &command_buffer);
  }

  // not needed for emscripten?
  //webgpu.surface.Present();
}

}
