#include <iostream>
#include <emscripten.h>
#include "logstorm/logstorm.h"
#include "render/webgpu_renderer.h"

class game_manager {
  logstorm::manager logger{logstorm::manager::build_with_sink<logstorm::sink::console>()}; // logging system
  render::webgpu_renderer renderer{logger, [&]{loop_main();}};                  // WebGPU rendering system

  void loop_main();
};

void game_manager::loop_main() {
  /// Main pseudo-loop
  renderer.draw();
}

auto main()->int {
  try {
    game_manager game;

  } catch (std::exception const &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    EM_ASM(alert("Error: Press F12 to see console for details."));
  }

  return EXIT_SUCCESS;
}
