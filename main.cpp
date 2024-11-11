#include <iostream>
#include <set>
#include <thread>
#include <vector>
#include <boost/throw_exception.hpp>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/val.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <magic_enum/magic_enum.hpp>
#include "logstorm/logstorm.h"
#include "vectorstorm/vector/vector2.h"
#include "vectorstorm/vector/vector3.h"
#include "vectorstorm/vector/vector4.h"
#include "render/shaders/default.wgsl.h"

#ifdef BOOST_NO_EXCEPTIONS
void boost::throw_exception(std::exception const & e) {
  /// Custom exception replacement function when boost exceptions are disabled
  std::cerr << "ERROR: Boost would have thrown exception: " << e.what() << std::endl;
  abort();
  std::unreachable();
}
#endif // BOOST_NO_EXCEPTIONS

struct vertex {
  vec2f position;
  vec4f colour;
};
static_assert(sizeof(vertex) == sizeof(vec2f) + sizeof(vec4f));                 // make sure the struct is packed

using triangle_index = vec3<uint16_t>;
static_assert(sizeof(triangle_index) == sizeof(uint16_t) * 3);                  // make sure the vector is packed

class game_manager {
  logstorm::manager logger{logstorm::manager::build_with_sink<logstorm::sink::console>()}; // logging system

  struct {
    wgpu::Surface surface;                                                      // the canvas surface for rendering
    wgpu::Adapter adapter;                                                      // WebGPU adapter once it has been acquired
    wgpu::Device device;                                                        // WebGPU device once it has been acquired
    wgpu::Queue queue;                                                          // the queue for this device, once it has been acquired
    wgpu::RenderPipeline pipeline;                                              // the render pipeline currently in use

    wgpu::TextureFormat surface_preferred_format{wgpu::TextureFormat::Undefined}; // preferred texture format for this surface
  } webgpu;

  struct {
    GLFWwindow *glfw_window{nullptr};                                           // GLFW handle for the window

    vec2i viewport_size;                                                        // our idea of the size of the viewport we render to, in real pixels
    vec2i canvas_size;                                                          // implementation-reported canvas size
    vec2ui document_body_size;                                                  // these sizes are before pixel ratio scaling, i.e. they change when the browser window is zoomed
    vec2ui window_inner_size;
    vec2ui window_outer_size;
    float device_pixel_ratio{1.0f};
  } window;

  //render::window window{logger, "Loading: Armchair WebGPU Demo"};
  //gui::world_gui gui{logger, window};

  std::function<void(int, char const*)> glfw_callback_error;                    // callback for unhandled GLFW errors

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

  // TODO: move this to a window init
  if(glfwInit() != GLFW_TRUE) {                                                 // initialise the opengl window
    logger << "render::window: ERROR: GLFW initialisation failed!  Cannot continue.";
    throw std::runtime_error{"Could not initialize GLFW"};
  }

  glfw_callback_error = [&](int error_code, char const *description){
    logger << "ERROR: GLFW: " << error_code << ": " << description;
  };
  glfwSetErrorCallback(glfw_callback_error.target<void(int, char const*)>());   // pass the target to GLFW to set the callback as a function pointer

  logger << "render::window: GLFW monitor name: " << glfwGetMonitorName(nullptr);

  // find out about the initial canvas size and the current window and doc sizes
  emscripten_get_canvas_element_size("#canvas", &window.canvas_size.x, &window.canvas_size.y);
  window.document_body_size.assign(emscripten::val::global("document")["body"]["clientWidth"].as<unsigned int>(),
                                   emscripten::val::global("document")["body"]["clientHeight"].as<unsigned int>());
  window.window_inner_size.assign( emscripten::val::global("window")["innerWidth"].as<unsigned int>(),
                                   emscripten::val::global("window")["innerHeight"].as<unsigned int>());
  window.window_outer_size.assign( emscripten::val::global("window")["outerWidth"].as<unsigned int>(),
                                   emscripten::val::global("window")["outerHeight"].as<unsigned int>());
  window.device_pixel_ratio = emscripten::val::global("window")["devicePixelRatio"].as<float>(); // query device pixel ratio using JS
  logger << "render::window: Window outer size: " << window.window_outer_size << " (device pixels: approx " << static_cast<vec2f>(window.window_outer_size) * window.device_pixel_ratio << ")";
  logger << "render::window: Window inner size: " << window.window_inner_size << " (device pixels: approx " << static_cast<vec2f>(window.window_inner_size) * window.device_pixel_ratio << ")";
  logger << "render::window: Document body size: " << window.document_body_size << " (device pixels: approx " << static_cast<vec2f>(window.document_body_size) * window.device_pixel_ratio << ")";
  logger << "render::window: Default canvas size: " << window.canvas_size << " (device pixels: approx " << static_cast<vec2f>(window.canvas_size) * window.device_pixel_ratio << ")";
  logger << "render::window: Device pixel ratio: " << window.device_pixel_ratio << " canvas pixels to 1 device pixel (" << static_cast<unsigned int>(std::round(100.0f * window.device_pixel_ratio)) << "% zoom)";

  window.viewport_size = window.window_inner_size;
  logger << "render::window: Setting viewport requested size: " << window.viewport_size << " (device pixels: approx " << static_cast<vec2f>(window.viewport_size) * window.device_pixel_ratio << ")";

  // TODO: resize callback here (from Project Raindrop)
  // TODO: resize callback should have surface.Configure ... size updates

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  // TODO: disable no-resize hint when resize callback has been implemented

  window.glfw_window = glfwCreateWindow(window.canvas_size.x,                   // use initial canvas size
                                        window.canvas_size.y,
                                        "Armchair WebGPU Demo",                 // window title
                                        nullptr,                                // monitor to use fullscreen, NULL here means run windowed - we always do under emscripten
                                        nullptr);                               // the context to share with, see http://stackoverflow.com/a/17792242/1678468
  if(!window.glfw_window) {
    logger << "render::window: ERROR: GLFW window creation failed!  Cannot continue.";
    throw std::runtime_error{"Could not create a GLFW window"};
  }

  glfwSetWindowUserPointer(window.glfw_window, this);                           // set callback userdata

  {
    // create a WebGPU instance
    wgpu::Instance instance{wgpu::CreateInstance()};
    if(!instance) throw std::runtime_error{"Could not initialize WebGPU"};

    // create a surface
    {
      wgpu::SurfaceDescriptorFromCanvasHTMLSelector surface_descriptor_from_canvas;
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
      .compatibleSurface{webgpu.surface},
      .powerPreference{wgpu::PowerPreference::HighPerformance},
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

        auto &adapter{game.webgpu.adapter};
        adapter = wgpu::Adapter::Acquire(adapter_ptr);

        // report surface and adapter capabilities
        {
          #ifndef NDEBUG
            wgpu::SurfaceCapabilities surface_capabilities;
            game.webgpu.surface.GetCapabilities(adapter, &surface_capabilities);
            for(size_t i{0}; i != surface_capabilities.formatCount; ++i) {
              logger << "DEBUG: WebGPU surface capabilities: texture formats: " << magic_enum::enum_name(surface_capabilities.formats[i]);
            }
            for(size_t i{0}; i != surface_capabilities.presentModeCount; ++i) {
              logger << "DEBUG: WebGPU surface capabilities: present modes: " << magic_enum::enum_name(surface_capabilities.presentModes[i]);
            }
            for(size_t i{0}; i != surface_capabilities.alphaModeCount; ++i) {
              logger << "DEBUG: WebGPU surface capabilities: alpha modes: " << magic_enum::enum_name(surface_capabilities.alphaModes[i]);
            }
          #endif // NDEBUG
        }
        game.webgpu.surface_preferred_format = game.webgpu.surface.GetPreferredFormat(adapter);
        logger << "WebGPU surface preferred format for this adapter: " << magic_enum::enum_name(game.webgpu.surface_preferred_format);

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
          #ifndef NDEBUG
            wgpu::AdapterProperties adapter_properties;
            adapter.GetProperties(&adapter_properties);
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

        wgpu::SupportedLimits adapter_limits;
        bool result{adapter.GetLimits(&adapter_limits)};
        #ifndef NDEBUG
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

        // specify required limits for the device
        struct limit {
          wgpu::Limits required{
            .maxVertexBuffers{1},
            .maxBufferSize{6 * 2 * sizeof(float)},
            .maxVertexAttributes{1},
            .maxVertexBufferArrayStride{2 * sizeof(float)},
          };
          wgpu::Limits desired{};
        } requested_limits;

        auto require_limit{[&]<typename T>(std::string const &name, T available, T required, T desired){
          constexpr auto undefined{std::numeric_limits<T>::max()};
          if(required == undefined) {                                           // no hard requirement for this value
            if(desired == undefined) {                                          //   no specific desire for this value
              return undefined;                                                 //     we don't care about the value
            } else {                                                            //   we have a desire for a specific value
              if(available == undefined) {                                      //     but it's not available
                logger << "WebGPU: Desired minimum limit for " << name << " is " << desired << " but is unavailable, ignoring";
                return undefined;                                               //       that's fine, we don't care
              } else {                                                          //     some limit is available
                logger << "WebGPU: Desired minimum limit for " << name << " is " << desired << ", requesting " << std::min(desired, available);
                return std::min(desired, available);                            //       we'll accept our desired amount or the limit, whichever is lowest
              }
            }
          } else {                                                              // we have a hard requirement for this value
            if(available == undefined) {                                        //   but it's not available
              logger << "WebGPU: Required minimum limit " << required << " is not available for " << name << " (limit undefined), cannot continue";
              throw std::runtime_error("WebGPU: Required adapter limits not met (limit undefined)");
            } else {                                                            //   some limit is available
              if(available < required) {                                        //     but the limit is below our requirement
                logger << "WebGPU: Required minimum limit " << required << " is not available for " << name << " (max " << available << "), cannot continue";
                throw std::runtime_error("WebGPU: Required adapter limits not met");
              } else {                                                          //     the limit is acceptable
                if(desired == undefined) {                                      //       we have no desire beyond the basic requirement
                  logger << "WebGPU: Required minimum limit for " << name << " is " << required << ", available";
                  return required;                                              //         we'll accept the required minimum
                } else {                                                        //       we desire a value beyond the basic requirement
                  assert(desired > required);                                   //         make sure we're not requesting nonsense with desired values below required minimum
                  logger << "WebGPU: Desired minimum limit for " << name << " is " << desired << ", requesting " << std::min(desired, available);
                  return std::min(desired, available);                          //         we'll accept our desired amount or the limit, whichever is lowest
                }
              }
            }
          }
        }};

        wgpu::RequiredLimits const required_limits{
          .limits{                                                              // see https://www.w3.org/TR/webgpu/#limit-default
            #define REQUIRE_LIMIT(limit) .limit{require_limit("limit", adapter_limits.limits.limit, requested_limits.required.limit, requested_limits.desired.limit)}
            REQUIRE_LIMIT(maxTextureDimension1D),
            REQUIRE_LIMIT(maxTextureDimension2D),
            REQUIRE_LIMIT(maxTextureDimension3D),
            REQUIRE_LIMIT(maxTextureArrayLayers),
            REQUIRE_LIMIT(maxBindGroups),
            REQUIRE_LIMIT(maxBindGroupsPlusVertexBuffers),
            REQUIRE_LIMIT(maxBindingsPerBindGroup),
            REQUIRE_LIMIT(maxDynamicUniformBuffersPerPipelineLayout),
            REQUIRE_LIMIT(maxDynamicStorageBuffersPerPipelineLayout),
            REQUIRE_LIMIT(maxSampledTexturesPerShaderStage),
            REQUIRE_LIMIT(maxSamplersPerShaderStage),
            REQUIRE_LIMIT(maxStorageBuffersPerShaderStage),
            REQUIRE_LIMIT(maxStorageTexturesPerShaderStage),
            REQUIRE_LIMIT(maxUniformBuffersPerShaderStage),
            REQUIRE_LIMIT(maxUniformBufferBindingSize),
            REQUIRE_LIMIT(maxStorageBufferBindingSize),
            REQUIRE_LIMIT(minUniformBufferOffsetAlignment),
            REQUIRE_LIMIT(minStorageBufferOffsetAlignment),
            // special treatment for minimum rather than maximum limits may be required, see notes for "alignment" at https://www.w3.org/TR/webgpu/#limit-default:
            //.minUniformBufferOffsetAlignment{adapter_limits.limits.minUniformBufferOffsetAlignment},
            //.minStorageBufferOffsetAlignment{adapter_limits.limits.minStorageBufferOffsetAlignment},
            REQUIRE_LIMIT(maxVertexBuffers),
            REQUIRE_LIMIT(maxBufferSize),
            REQUIRE_LIMIT(maxVertexAttributes),
            REQUIRE_LIMIT(maxVertexBufferArrayStride),
            REQUIRE_LIMIT(maxInterStageShaderComponents),
            REQUIRE_LIMIT(maxInterStageShaderVariables),
            REQUIRE_LIMIT(maxColorAttachments),
            REQUIRE_LIMIT(maxColorAttachmentBytesPerSample),
            REQUIRE_LIMIT(maxComputeWorkgroupStorageSize),
            REQUIRE_LIMIT(maxComputeInvocationsPerWorkgroup),
            REQUIRE_LIMIT(maxComputeWorkgroupSizeX),
            REQUIRE_LIMIT(maxComputeWorkgroupSizeY),
            REQUIRE_LIMIT(maxComputeWorkgroupSizeZ),
            REQUIRE_LIMIT(maxComputeWorkgroupsPerDimension),
            #undef REQUIRE_LIMIT
          },
        };

        // request a device
        wgpu::DeviceDescriptor device_descriptor{
          .requiredFeatureCount{required_features_arr.size()},
          .requiredFeatures{required_features_arr.data()},
          .requiredLimits{&required_limits},
          .defaultQueue{
            .label{"Default queue"},
          },
          .deviceLostCallback{[](WGPUDeviceLostReason reason_c, char const *message, void *data){
            /// Device lost callback
            auto &game{*static_cast<game_manager*>(data)};
            auto &logger{game.logger};
            logger << "ERROR: WebGPU lost device, reason " << enum_wgpu_name<wgpu::DeviceLostReason>(reason_c) << ": " << message;
          }},
          .deviceLostUserdata{&game},
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

            // report device capabilities
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
          },
          data
        );
      },
      this
    );
  }

  logger << "Entering WebGPU init loop";
  emscripten_set_main_loop_arg([](void *data){
    /// Dispatch the loop waiting for WebGPU to become ready
    auto &game{*static_cast<game_manager*>(data)};
    game.loop_wait_init();
  }, this, 0, true);                                                            // loop function, user data, FPS (0 to use browser requestAnimationFrame mechanism), simulate infinite loop
  std::unreachable();
}

void game_manager::loop_wait_init() {
  /// Main pseudo-loop waiting for initialisation to complete
  if(!webgpu.device) {
    logger << "Waiting for WebGPU to become available";
    // TODO: sensible timeout
    return;
  }

  logger << "WebGPU device ready, configuring surface";
  // configure the surface
  {
    wgpu::SurfaceConfiguration surface_configuration{
      .device{webgpu.device},
      .format{webgpu.surface_preferred_format},
      .viewFormats{nullptr},
      .width {static_cast<uint32_t>(window.canvas_size.x)},
      .height{static_cast<uint32_t>(window.canvas_size.y)},
    };
    webgpu.surface.Configure(&surface_configuration);
  }

  logger << "WebGPU acquiring queue";
  webgpu.queue = webgpu.device.GetQueue();

  logger << "WebGPU assembling shaders";
  {
    wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl_decriptor;
    shader_module_wgsl_decriptor.code = render::shaders::default_wgsl;
    wgpu::ShaderModuleDescriptor shader_module_descriptor{
      .nextInChain{&shader_module_wgsl_decriptor},
      .label{"Shader module 1"},
    };
    wgpu::ShaderModule shader_module{webgpu.device.CreateShaderModule(&shader_module_descriptor)};

    logger << "WebGPU configuring pipeline";

    std::vector<wgpu::VertexAttribute> vertex_attributes{
      {
        .format{wgpu::VertexFormat::Float32x2},
        .offset{offsetof(vertex, position)},
        .shaderLocation{0},
      },
      {
        .format{wgpu::VertexFormat::Float32x4},
        .offset{offsetof(vertex, colour)},
        .shaderLocation{1},
      },
    };
    wgpu::VertexBufferLayout vertex_buffer_layout{
      .arrayStride{sizeof(vertex)},
      .attributeCount{vertex_attributes.size()},
      .attributes{vertex_attributes.data()},
    };

    wgpu::BlendState blend_state{
      .color{
        .operation{wgpu::BlendOperation::Add},                                  // initial values from https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/hello-triangle.html
        .srcFactor{wgpu::BlendFactor::SrcAlpha},
        .dstFactor{wgpu::BlendFactor::OneMinusSrcAlpha},
      },
      .alpha{
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

    wgpu::RenderPipelineDescriptor render_pipeline_descriptor{
      .label{"Render pipeline 1"},
      .vertex{
        .module{shader_module},
        .entryPoint{"vs_main"},
        .constantCount{0},
        .constants{nullptr},
        .bufferCount{1},
        .buffers{&vertex_buffer_layout},
      },
      .primitive{
        // TODO: cullmode front etc
      },
      // TODO:
      //optional .depthStencil =
      .multisample{},
      .fragment{&fragment_state},
    };
    webgpu.pipeline = webgpu.device.CreateRenderPipeline(&render_pipeline_descriptor);
  }

  logger << "Entering main loop";
  emscripten_cancel_main_loop();
  emscripten_set_main_loop_arg([](void *data){
    /// Main pseudo-loop dispatcher
    auto &game{*static_cast<game_manager*>(data)};
    game.loop_main();
  }, this, 0, true);                                                            // loop function, user data, FPS (0 to use browser requestAnimationFrame mechanism), simulate infinite loop
  std::unreachable();
}

void game_manager::loop_main() {
  /// Main pseudo-loop
  logger << "Tick...";
  glfwPollEvents();

  wgpu::TextureView texture_view{[&]{
    /// Return a texture view from the current surface texture
    // get the surface texture for rendering to
    wgpu::SurfaceTexture surface_texture;
    webgpu.surface.GetCurrentTexture(&surface_texture);
    if(surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
      logger << "ERROR: WebGPU failed to get surface texture: " << magic_enum::enum_name(surface_texture.status);
    }
    if(surface_texture.suboptimal) {
      logger << "WARNING: WebGPU surface texture is suboptimal";
    }

    // get a texture view of the surface texture
    wgpu::TextureViewDescriptor texture_view_descriptor{
      .label{"Surface texture view"},
      .format{surface_texture.texture.GetFormat()},
      .dimension{wgpu::TextureViewDimension::e2D},
      .mipLevelCount{1},
      .arrayLayerCount{1},
    };
    return surface_texture.texture.CreateView(&texture_view_descriptor);
  }()};

  {
    wgpu::CommandEncoderDescriptor command_encoder_descriptor{
      .label = "Command encoder 1"
    };
    wgpu::CommandEncoder command_encoder{webgpu.device.CreateCommandEncoder(&command_encoder_descriptor)};

    {
      wgpu::RenderPassColorAttachment render_pass_colour_attachment{
        .view{texture_view},
        .loadOp{wgpu::LoadOp::Clear},
        .storeOp{wgpu::StoreOp::Store},
        .clearValue{wgpu::Color{0, 0.5, 0.5, 1.0}},
      };

      wgpu::RenderPassDescriptor render_pass_descriptor{
        .label{"Render pass 1"},
        .colorAttachmentCount{1},
        .colorAttachments{&render_pass_colour_attachment},
      };
      wgpu::RenderPassEncoder render_pass_encoder{command_encoder.BeginRenderPass(&render_pass_descriptor)};

      render_pass_encoder.SetPipeline(webgpu.pipeline);                         // select which render pipeline to use

      // set up test buffers
      std::vector<vertex> vertex_data{
        {{-0.5, -0.5}, {1.0, 0.0, 0.0, 1.0}},
        {{+0.5, -0.5}, {0.0, 1.0, 0.0, 1.0}},
        {{+0.5, +0.5}, {0.0, 0.0, 1.0, 1.0}},
        {{-0.5, +0.5}, {1.0, 1.0, 0.0, 1.0}},
      };
      std::vector<triangle_index> index_data{
        {0, 1, 2},
        {0, 2, 3},
      };

      wgpu::BufferDescriptor vertex_buffer_descriptor{
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

      wgpu::BufferDescriptor index_buffer_descriptor{
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

      render_pass_encoder.SetVertexBuffer(0, vertex_buffer, 0, vertex_buffer.GetSize()); // slot, buffer, offset, size
      render_pass_encoder.SetIndexBuffer(index_buffer, wgpu::IndexFormat::Uint16, 0, index_buffer.GetSize()); // buffer, format, offset, size
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
    //    auto &game{*static_cast<game_manager*>(data)};
    //    auto &logger{game.logger};
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
