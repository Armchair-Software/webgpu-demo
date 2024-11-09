#include <iostream>
#include <set>
#include <thread>
#include <vector>
#include <boost/throw_exception.hpp>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/val.h>
#include <magic_enum/magic_enum.hpp>
//#define WEBGPU_CPP_IMPLEMENTATION
//#include <webgpu.hpp>
#include <webgpu/webgpu_cpp.h>
#include "logstorm/logstorm.h"

#ifdef BOOST_NO_EXCEPTIONS
void boost::throw_exception(std::exception const & e) {
  /// Custom exception replacement function when boost exceptions are disabled
  std::cerr << "ERROR: Boost would have thrown exception: " << e.what() << std::endl;
  abort();
  std::unreachable();
}
#endif // BOOST_NO_EXCEPTIONS

class game_manager {
  logstorm::manager logger{logstorm::manager::build_with_sink<logstorm::sink::console>()}; // logging system

  struct {
    bool ready{false};
    wgpu::Device device;
  } webgpu;

  //render::window window{logger, "Loading: Armchair WebGPU Demo"};
  //gui::world_gui gui{logger, window};

public:
  void run();

private:
  static void loop_main_dispatcher(void *data);
  void loop_wait_init();
  void loop_main();
};

template<typename Tcpp, typename Tc>
std::string enum_wgpu_name(Tc enum_in) {
  /// Attempt to interpret an enum into its most human-readable form, with fallbacks for unknown types
  /// Tc is the C API enum (WGPU...), Tcpp is the C++ API enum equivalent (wgpu::...)
  if(auto enum_out_opt{magic_enum::enum_cast<Tcpp>(enum_in)}; enum_out_opt.has_value()) { // first try to cast it to the C++ enum for clearest output
    return std::string{magic_enum::enum_name(*enum_out_opt)};
  }

  if(auto enum_out_opt{magic_enum::enum_cast<Tc>(enum_in)}; enum_out_opt.has_value()) { // fall back to trying the C enum interpretation
   return std::string{magic_enum::enum_name(*enum_out_opt)} + " (C binding only)";
  }

  std::ostringstream oss;
  oss << "unknown enum 0x" << std::hex << enum_in;                              // otherwise output the hex value and an explanatory note
  return oss.str();
}

template<typename Tcpp, typename Tc>
std::string enum_wgpu_name(Tcpp enum_in) {
  /// When passing the C++ version, cast it to the C version
  return enum_wgpu_name<Tcpp, Tc>(static_cast<Tc>(enum_in));
}

void game_manager::run() {
  /// Launch the game pseudo-loop
  logger << "Starting Armchair WebGPU Demo";

  /**
  // Using webgpu.hpp:

  wgpu::Instance instance{wgpu::createInstance()};
  if(!instance) throw std::runtime_error{"Could not initialize WebGPU"};

  logger << "DEBUG: WebGPU instance " << instance;

  //wgpu::RequestAdapterOptions options{wgpu::Default};
  wgpu::RequestAdapterOptions options;
  options.powerPreference = wgpu::PowerPreference::HighPerformance;

  wgpu::RequestAdapterCallback callback{[&](WGPURequestAdapterStatus status, wgpu::Adapter adapter, char const *message) {
    logger << "DEBUG: WebGPU callback called, status " << magic_enum::enum_name(status) << ", adapter " << adapter << ", message " << message;
  }};

  auto adapter{instance.requestAdapter(options, std::move(callback))};

  logger << "DEBUG: WebGPU adapter " << adapter;
  **/



  // Using Dawn's webgpu/webgpu_cpp.h
  {
    wgpu::Instance instance{wgpu::CreateInstance()};
    if(!instance) throw std::runtime_error{"Could not initialize WebGPU"};

    wgpu::RequestAdapterOptions adapter_request_options{
      .powerPreference = wgpu::PowerPreference::HighPerformance,
    };

    instance.RequestAdapter(
      &adapter_request_options,
      [](WGPURequestAdapterStatus status_c, WGPUAdapterImpl *adapter_ptr, const char *message, void *data){
        /// Request adapter callback
        auto &game{*static_cast<game_manager*>(data)};
        auto &logger{game.logger};
        if(message) logger << "WebGPU: Request adapter callback message: " << message;
        if(auto status{static_cast<wgpu::RequestAdapterStatus>(status_c)}; status != wgpu::RequestAdapterStatus::Success) {
          logger << "ERROR: WebGPU adapter request failure, status " << enum_wgpu_name<wgpu::RequestAdapterStatus>(status_c);
          throw std::runtime_error{"WebGPU: Could not get adapter"};
        }
        //game.webgpu.adapter = adapter;

        wgpu::Adapter adapter{wgpu::Adapter::Acquire(adapter_ptr)};

        {
          wgpu::AdapterInfo adapter_info;
          adapter.GetInfo(&adapter_info);
          #ifndef NDEBUG
            logger << "DEBUG: WebGPU adapter info: vendor: " << adapter_info.vendor;
            logger << "DEBUG: WebGPU adapter info: architecture: " << adapter_info.architecture;
            logger << "DEBUG: WebGPU adapter info: device: " << adapter_info.device;
            logger << "DEBUG: WebGPU adapter info: description: " << adapter_info.description;
            logger << "DEBUG: WebGPU adapter info: vendorID:deviceID: " << adapter_info.vendorID << ":" << adapter_info.deviceID;
            logger << "DEBUG: WebGPU adapter info: backendType: " << magic_enum::enum_name(adapter_info.backendType);
            logger << "DEBUG: WebGPU adapter info: adapterType: " << magic_enum::enum_name(adapter_info.adapterType);
          #endif // NDEBUG
          logger << "WebGPU adapter info: " << adapter_info.description << " (" << magic_enum::enum_name(adapter_info.backendType) << ", " << adapter_info.vendor << ", " << adapter_info.architecture << ")";
        }
        {
          wgpu::AdapterProperties adapter_properties;
          adapter.GetProperties(&adapter_properties);
          #ifndef NDEBUG
            logger << "DEBUG: WebGPU adapter properties: vendorID: " << adapter_properties.vendorID;
            logger << "DEBUG: WebGPU adapter properties: vendorName: " << adapter_properties.vendorName;
            logger << "DEBUG: WebGPU adapter properties: architecture: " << adapter_properties.architecture;
            logger << "DEBUG: WebGPU adapter properties: deviceID: " << adapter_properties.deviceID;
            logger << "DEBUG: WebGPU adapter properties: name: " << adapter_properties.name;
            logger << "DEBUG: WebGPU adapter properties: driverDescription: " << adapter_properties.driverDescription;
            logger << "DEBUG: WebGPU adapter properties: backendType: " << magic_enum::enum_name(adapter_properties.backendType);
            logger << "DEBUG: WebGPU adapter properties: adapterType: " << magic_enum::enum_name(adapter_properties.adapterType);
            logger << "DEBUG: WebGPU adapter properties: compatibilityMode: " << std::boolalpha << adapter_properties.compatibilityMode;
            logger << "DEBUG: WebGPU adapter properties: nextInChain: " << adapter_properties.nextInChain;
          #endif // NDEBUG
        }
        std::set<wgpu::FeatureName> adapter_features;
        {
          // see https://developer.mozilla.org/en-US/docs/Web/API/GPUSupportedFeatures and https://www.w3.org/TR/webgpu/#feature-index
          auto const count{adapter.EnumerateFeatures(nullptr)};
          logger << "DEBUG: WebGPU adapter features count: " << count;
          std::vector<wgpu::FeatureName> adapter_features_arr(count);
          adapter.EnumerateFeatures(adapter_features_arr.data());
          for(unsigned int i{0}; i != adapter_features_arr.size(); ++i) {
            adapter_features.emplace(adapter_features_arr[i]);
          }
        }
        for(auto const feature : adapter_features) {
          logger << "DEBUG: WebGPU adapter features: " << enum_wgpu_name<wgpu::FeatureName, WGPUFeatureName>(feature);
        }
        {
          wgpu::SupportedLimits adapter_limits;
          bool result{adapter.GetLimits(&adapter_limits)};
          logger << "DEBUG: WebGPU adapter limits result: " << std::boolalpha << result;
          logger << "DEBUG: WebGPU adapter limits nextInChain: " << adapter_limits.nextInChain;
          logger << "DEBUG: WebGPU adapter limits maxTextureDimension1D: " << adapter_limits.limits.maxTextureDimension1D;
          logger << "DEBUG: WebGPU adapter limits maxTextureDimension2D: " << adapter_limits.limits.maxTextureDimension2D;
          logger << "DEBUG: WebGPU adapter limits maxTextureDimension3D: " << adapter_limits.limits.maxTextureDimension3D;
          logger << "DEBUG: WebGPU adapter limits maxTextureArrayLayers: " << adapter_limits.limits.maxTextureArrayLayers;
          logger << "DEBUG: WebGPU adapter limits maxBindGroups: " << adapter_limits.limits.maxBindGroups;
          logger << "DEBUG: WebGPU adapter limits maxBindGroupsPlusVertexBuffers: " << adapter_limits.limits.maxBindGroupsPlusVertexBuffers;
          logger << "DEBUG: WebGPU adapter limits maxBindingsPerBindGroup: " << adapter_limits.limits.maxBindingsPerBindGroup;
          logger << "DEBUG: WebGPU adapter limits maxDynamicUniformBuffersPerPipelineLayout: " << adapter_limits.limits.maxDynamicUniformBuffersPerPipelineLayout;
          logger << "DEBUG: WebGPU adapter limits maxDynamicStorageBuffersPerPipelineLayout: " << adapter_limits.limits.maxDynamicStorageBuffersPerPipelineLayout;
          logger << "DEBUG: WebGPU adapter limits maxSamplersPerShaderStage: " << adapter_limits.limits.maxSamplersPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxStorageBuffersPerShaderStage: " << adapter_limits.limits.maxStorageBuffersPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxStorageTexturesPerShaderStage: " << adapter_limits.limits.maxStorageTexturesPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxUniformBuffersPerShaderStage: " << adapter_limits.limits.maxUniformBuffersPerShaderStage;
          logger << "DEBUG: WebGPU adapter limits maxUniformBufferBindingSize: " << adapter_limits.limits.maxUniformBufferBindingSize;
          logger << "DEBUG: WebGPU adapter limits maxStorageBufferBindingSize: " << adapter_limits.limits.maxStorageBufferBindingSize;
          logger << "DEBUG: WebGPU adapter limits minUniformBufferOffsetAlignment: " << adapter_limits.limits.minUniformBufferOffsetAlignment;
          logger << "DEBUG: WebGPU adapter limits minStorageBufferOffsetAlignment: " << adapter_limits.limits.minStorageBufferOffsetAlignment;
          logger << "DEBUG: WebGPU adapter limits maxVertexBuffers: " << adapter_limits.limits.maxVertexBuffers;
          logger << "DEBUG: WebGPU adapter limits maxBufferSize: " << adapter_limits.limits.maxBufferSize;
          logger << "DEBUG: WebGPU adapter limits maxVertexAttributes: " << adapter_limits.limits.maxVertexAttributes;
          logger << "DEBUG: WebGPU adapter limits maxVertexBufferArrayStride: " << adapter_limits.limits.maxVertexBufferArrayStride;
          logger << "DEBUG: WebGPU adapter limits maxInterStageShaderComponents: " << adapter_limits.limits.maxInterStageShaderComponents;
          logger << "DEBUG: WebGPU adapter limits maxInterStageShaderVariables: " << adapter_limits.limits.maxInterStageShaderVariables;
          logger << "DEBUG: WebGPU adapter limits maxColorAttachments: " << adapter_limits.limits.maxColorAttachments;
          logger << "DEBUG: WebGPU adapter limits maxColorAttachmentBytesPerSample: " << adapter_limits.limits.maxColorAttachmentBytesPerSample;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupStorageSize: " << adapter_limits.limits.maxComputeWorkgroupStorageSize;
          logger << "DEBUG: WebGPU adapter limits maxComputeInvocationsPerWorkgroup: " << adapter_limits.limits.maxComputeInvocationsPerWorkgroup;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeX: " << adapter_limits.limits.maxComputeWorkgroupSizeX;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeY: " << adapter_limits.limits.maxComputeWorkgroupSizeY;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupSizeZ: " << adapter_limits.limits.maxComputeWorkgroupSizeZ;
          logger << "DEBUG: WebGPU adapter limits maxComputeWorkgroupsPerDimension: " << adapter_limits.limits.maxComputeWorkgroupsPerDimension;
        }

        std::set<wgpu::FeatureName> required_features{
          //wgpu::FeatureName::DepthClipControl,
          wgpu::FeatureName::Depth32FloatStencil8,
          #ifndef NDEBUG
            wgpu::FeatureName::TimestampQuery,
          #endif // NDEBUG
          wgpu::FeatureName::TextureCompressionBC,
          //wgpu::FeatureName::TextureCompressionETC2,
          //wgpu::FeatureName::TextureCompressionASTC,
          wgpu::FeatureName::IndirectFirstInstance,
          //wgpu::FeatureName::ShaderF16,
          //wgpu::FeatureName::RG11B10UfloatRenderable,
          //wgpu::FeatureName::BGRA8UnormStorage,
          //wgpu::FeatureName::Float32Filterable,
        };
        std::set<wgpu::FeatureName> desired_features{
          //wgpu::FeatureName::DepthClipControl,
          //wgpu::FeatureName::Depth32FloatStencil8,
          #ifndef NDEBUG
            //wgpu::FeatureName::TimestampQuery,
          #endif // NDEBUG
          //wgpu::FeatureName::TextureCompressionBC,
          //wgpu::FeatureName::TextureCompressionETC2,
          //wgpu::FeatureName::TextureCompressionASTC,
          //wgpu::FeatureName::IndirectFirstInstance,
          wgpu::FeatureName::ShaderF16,
          //wgpu::FeatureName::RG11B10UfloatRenderable,
          //wgpu::FeatureName::BGRA8UnormStorage,
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

        wgpu::DeviceDescriptor device_descriptor{
          .requiredFeatureCount = required_features_arr.size(),
          .requiredFeatures = required_features_arr.data(),
          .defaultQueue = {},
          // TODO: defaultQueue label
          // TODO: specify requiredLimits (use a required and desired limits mechanism again)
          .deviceLostCallback = [](WGPUDeviceLostReason reason_c, char const *message, void *data){
            /// Device lost callback
            auto &game{*static_cast<game_manager*>(data)};
            auto &logger{game.logger};
            logger << "ERROR: WebGPU lost device, reason " << enum_wgpu_name<wgpu::DeviceLostReason>(reason_c) << ": " << message;
          },
          .deviceLostUserdata = &game,
        };

        adapter.RequestDevice(
          &device_descriptor,
          [](WGPURequestDeviceStatus status_c, WGPUDevice device_ptr,  const char *message,  void *data){
            /// Request device callback
            auto &game{*static_cast<game_manager*>(data)};
            auto &logger{game.logger};
            if(message) logger << "WebGPU: Request device callback message: " << message;
            if(auto status{static_cast<wgpu::RequestDeviceStatus>(status_c)}; status != wgpu::RequestDeviceStatus::Success) {
              logger << "ERROR: WebGPU device request failure, status " << enum_wgpu_name<wgpu::RequestDeviceStatus>(status_c);
              throw std::runtime_error{"WebGPU: Could not get adapter"};
            }
            auto &device{game.webgpu.device};
            device = wgpu::Device::Acquire(device_ptr);

            std::set<wgpu::FeatureName> device_features;
            {
              auto const count{device.EnumerateFeatures(nullptr)};
              logger << "DEBUG: WebGPU device features count: " << count;
              std::vector<wgpu::FeatureName> device_features_arr(count);
              device.EnumerateFeatures(device_features_arr.data());
              for(unsigned int i{0}; i != device_features_arr.size(); ++i) {
                device_features.emplace(device_features_arr[i]);
              }
            }
            for(auto const feature : device_features) {
              logger << "DEBUG: WebGPU device features: " << magic_enum::enum_name(feature);
            }
            {
              wgpu::SupportedLimits adapter_limits;
              bool result{device.GetLimits(&adapter_limits)};
              logger << "DEBUG: WebGPU device limits result: " << std::boolalpha << result;
              logger << "DEBUG: WebGPU device limits nextInChain: " << adapter_limits.nextInChain;
              logger << "DEBUG: WebGPU device limits maxTextureDimension1D: " << adapter_limits.limits.maxTextureDimension1D;
              logger << "DEBUG: WebGPU device limits maxTextureDimension2D: " << adapter_limits.limits.maxTextureDimension2D;
              logger << "DEBUG: WebGPU device limits maxTextureDimension3D: " << adapter_limits.limits.maxTextureDimension3D;
              logger << "DEBUG: WebGPU device limits maxTextureArrayLayers: " << adapter_limits.limits.maxTextureArrayLayers;
              logger << "DEBUG: WebGPU device limits maxBindGroups: " << adapter_limits.limits.maxBindGroups;
              logger << "DEBUG: WebGPU device limits maxBindGroupsPlusVertexBuffers: " << adapter_limits.limits.maxBindGroupsPlusVertexBuffers;
              logger << "DEBUG: WebGPU device limits maxBindingsPerBindGroup: " << adapter_limits.limits.maxBindingsPerBindGroup;
              logger << "DEBUG: WebGPU device limits maxDynamicUniformBuffersPerPipelineLayout: " << adapter_limits.limits.maxDynamicUniformBuffersPerPipelineLayout;
              logger << "DEBUG: WebGPU device limits maxDynamicStorageBuffersPerPipelineLayout: " << adapter_limits.limits.maxDynamicStorageBuffersPerPipelineLayout;
              logger << "DEBUG: WebGPU device limits maxSamplersPerShaderStage: " << adapter_limits.limits.maxSamplersPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxStorageBuffersPerShaderStage: " << adapter_limits.limits.maxStorageBuffersPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxStorageTexturesPerShaderStage: " << adapter_limits.limits.maxStorageTexturesPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxUniformBuffersPerShaderStage: " << adapter_limits.limits.maxUniformBuffersPerShaderStage;
              logger << "DEBUG: WebGPU device limits maxUniformBufferBindingSize: " << adapter_limits.limits.maxUniformBufferBindingSize;
              logger << "DEBUG: WebGPU device limits maxStorageBufferBindingSize: " << adapter_limits.limits.maxStorageBufferBindingSize;
              logger << "DEBUG: WebGPU device limits minUniformBufferOffsetAlignment: " << adapter_limits.limits.minUniformBufferOffsetAlignment;
              logger << "DEBUG: WebGPU device limits minStorageBufferOffsetAlignment: " << adapter_limits.limits.minStorageBufferOffsetAlignment;
              logger << "DEBUG: WebGPU device limits maxVertexBuffers: " << adapter_limits.limits.maxVertexBuffers;
              logger << "DEBUG: WebGPU device limits maxBufferSize: " << adapter_limits.limits.maxBufferSize;
              logger << "DEBUG: WebGPU device limits maxVertexAttributes: " << adapter_limits.limits.maxVertexAttributes;
              logger << "DEBUG: WebGPU device limits maxVertexBufferArrayStride: " << adapter_limits.limits.maxVertexBufferArrayStride;
              logger << "DEBUG: WebGPU device limits maxInterStageShaderComponents: " << adapter_limits.limits.maxInterStageShaderComponents;
              logger << "DEBUG: WebGPU device limits maxInterStageShaderVariables: " << adapter_limits.limits.maxInterStageShaderVariables;
              logger << "DEBUG: WebGPU device limits maxColorAttachments: " << adapter_limits.limits.maxColorAttachments;
              logger << "DEBUG: WebGPU device limits maxColorAttachmentBytesPerSample: " << adapter_limits.limits.maxColorAttachmentBytesPerSample;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupStorageSize: " << adapter_limits.limits.maxComputeWorkgroupStorageSize;
              logger << "DEBUG: WebGPU device limits maxComputeInvocationsPerWorkgroup: " << adapter_limits.limits.maxComputeInvocationsPerWorkgroup;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeX: " << adapter_limits.limits.maxComputeWorkgroupSizeX;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeY: " << adapter_limits.limits.maxComputeWorkgroupSizeY;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupSizeZ: " << adapter_limits.limits.maxComputeWorkgroupSizeZ;
              logger << "DEBUG: WebGPU device limits maxComputeWorkgroupsPerDimension: " << adapter_limits.limits.maxComputeWorkgroupsPerDimension;
            }

            device.SetUncapturedErrorCallback(
              [](WGPUErrorType type, char const *message, void *data){
                /// Uncaptured error callback
                auto &game{*static_cast<game_manager*>(data)};
                auto &logger{game.logger};
                logger << "ERROR: WebGPU uncaptured error " << enum_wgpu_name<wgpu::ErrorType>(type) << ": " << message;
              },
              &game
            );
            game.webgpu.ready = true;
          },
          data
        );
      },
      this
    );
  }

  // TODO: can we just do this?
  //#include <emscripten/html5_webgpu.h>
  //wgpu::Device device{wgpu::Device::Acquire(emscripten_webgpu_get_device())};


  logger << "Entering WebGPU init loop";
  emscripten_set_main_loop_arg([](void *data){
    /// Dispatch the loop waiting for WebGPU to become ready
    auto &game{*static_cast<game_manager*>(data)};
    game.loop_wait_init();
  }, this, 0, true);                                                            // loop function, user data, FPS (0 to use browser requestAnimationFrame mechanism), simulate infinite loop
  // TODO: unreachable
}

void game_manager::loop_wait_init() {
  /// Main pseudo-loop waiting for initialisation to complete
  logger << "Waiting for WebGPU to become available...";

  if(webgpu.ready) {
    logger << "Entering main loop";
    emscripten_cancel_main_loop();
    emscripten_set_main_loop_arg([](void *data){
      /// Main pseudo-loop dispatcher
      auto &game{*static_cast<game_manager*>(data)};
      game.loop_main();
    }, this, 0, true);                                                          // loop function, user data, FPS (0 to use browser requestAnimationFrame mechanism), simulate infinite loop
    // TODO: unreachable
  }

  // TODO: sensible timeout
}

void game_manager::loop_main() {
  /// Main pseudo-loop
  logger << "Tick...";
  //glfwSwapBuffers(window.glfw_window);
}

auto main()->int {
  try {
    game_manager game;
    game.run();

  } catch (std::exception const &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    EM_ASM(alert("Error: Press F12 to see console for details."));
  }

  return EXIT_SUCCESS;
}
