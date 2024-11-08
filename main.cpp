#include <iostream>
#include <thread>
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
    //WGPUAdapterImpl *adapter{nullptr};
    //wgpu::Adapter adapter;
  } webgpu;

  //render::window window{logger, "Loading: Armchair WebGPU Demo"};
  //gui::world_gui gui{logger, window};

public:
  void run();

private:
  static void loop_main_dispatcher(void *data);
  void loop_main();
};

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
      [](WGPURequestAdapterStatus status_c, WGPUAdapterImpl *adapter_ptr, const char *message, void *data) {
        auto &game{*static_cast<game_manager*>(data)};
        auto &logger{game.logger};
        if(message) logger << "WebGPU: Request adapter callback message: " << message;
        if(auto status{static_cast<wgpu::RequestAdapterStatus>(status_c)}; status != wgpu::RequestAdapterStatus::Success) {
          logger << "ERROR: WebGPU adapter request failure, status " << magic_enum::enum_name(status);
          throw std::runtime_error{"WebGPU: Could not get adapter"};
        }
        //game.webgpu.adapter = adapter;

        wgpu::Adapter adapter{wgpu::Adapter::Acquire(adapter_ptr)};

        // TODO: inspect the adapter


        wgpu::DeviceDescriptor device_descriptor{};
        // TODO: specify requiredFeatures, requiredLimits, deviceLostCallback etc

        adapter.RequestDevice(
          &device_descriptor,
          [](WGPURequestDeviceStatus status_c, WGPUDevice device_ptr,  const char *message,  void *data) {
          auto &game{*static_cast<game_manager*>(data)};
          auto &logger{game.logger};
          if(message) logger << "WebGPU: Request device callback message: " << message;
          if(auto status{static_cast<wgpu::RequestDeviceStatus>(status_c)}; status != wgpu::RequestDeviceStatus::Success) {
            logger << "ERROR: WebGPU device request failure, status " << magic_enum::enum_name(status);
            throw std::runtime_error{"WebGPU: Could not get adapter"};
          }
          wgpu::Device device{wgpu::Device::Acquire(device_ptr)};

          // TODO: inspect the device

          // TODO: ready
        }, data);
      },
      this
    );
  }

  // TODO: can we just do this?
  //wgpu::Device device = wgpu::Device::Acquire(emscripten_webgpu_get_device());


  logger << "Entering main loop";

  emscripten_set_main_loop_arg([](void *data){
    /// Main pseudo-loop dispatcher
    auto &game{*static_cast<game_manager*>(data)};
    game.loop_main();
  }, this, 0, true);                                                            // loop function, user data, FPS (0 to use browser requestAnimationFrame mechanism), simulate infinite loop
}

void game_manager::loop_main() {
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
