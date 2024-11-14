#include <iostream>
#include <boost/throw_exception.hpp>
#include <emscripten.h>
//#include <GLFW/glfw3.h>
#include "logstorm/logstorm.h"
#include "render/webgpu_renderer.h"

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
  render::webgpu_renderer renderer{logger};                                     // WebGPU rendering system

  //render::window window{logger, "Loading: Armchair WebGPU Demo"};
  //gui::world_gui gui{logger, window};

  //std::function<void(int, char const*)> glfw_callback_error;                    // callback for unhandled GLFW errors

public:
  void run();

private:
  static void loop_main_dispatcher(void *data);
  void loop_wait_init();
  void loop_main();
};


void game_manager::run() {
  /// Launch the game pseudo-loop
  logger << "Starting Armchair WebGPU Demo";

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
  if(!renderer.ready_for_configure()) {
    logger << "Waiting for WebGPU device to become available";
    // TODO: sensible timeout
    return;
  }
  renderer.configure();

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
  //glfwPollEvents();

  renderer.draw();

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
