#include <iostream>
#include <thread>
#include <boost/throw_exception.hpp>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/val.h>
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
  //logger.add_sink<logstorm::sink::console>();
  logger << "Starting Armchair WebGPU Demo";

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
