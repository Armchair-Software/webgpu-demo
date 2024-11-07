#include <iostream>
#include <thread>
#include <boost/throw_exception.hpp>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/val.h>
#include <magic_enum/magic_enum.hpp>
#define WEBGPU_CPP_IMPLEMENTATION
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

  /*
  wgpu::Instance instance{wgpu::createInstance()};
  if(!instance) throw std::runtime_error{"Could not initialize WebGPU"};

  logger << "DEBUG: WebGPU instance " << instance;

  wgpu::RequestAdapterOptions options{wgpu::Default};
  options.powerPreference = wgpu::PowerPreference::HighPerformance;

  wgpu::RequestAdapterCallback callback{[&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, char const *message) {
    logger << "DEBUG: WebGPU callback called, status " << magic_enum::enum_name(status) << ", adapter " << adapter << ", message " << message;
  }};

  auto adapter{instance.requestAdapter(options, std::move(callback))};

  logger << "DEBUG: WebGPU adapter " << adapter;
  */




  wgpu::Instance instance{wgpu::CreateInstance()};
  if(!instance) throw std::runtime_error{"Could not initialize WebGPU"};

  wgpu::RequestAdapterOptions options{
    .powerPreference = wgpu::PowerPreference::HighPerformance,
  };

  wgpu::RequestAdapterCallback callback{[](WGPURequestAdapterStatus status, WGPUAdapterImpl *adapter, const char *message, void *data) {
  //wgpu::RequestAdapterCallback callback{[](wgpu::RequestAdapterStatus status, WGPUAdapterImpl *adapter, const char *message, void *data) {
    auto &game{*static_cast<game_manager*>(data)};
    auto &logger{game.logger};
    logger << "DEBUG: WebGPU callback called, status " << magic_enum::enum_name(status) << ", message " << message;
  }};

  instance.RequestAdapter(&options, callback, this);






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
