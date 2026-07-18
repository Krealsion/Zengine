#include <utility_states/build_state.h>

using namespace Zen;

int main(int argc, char *argv[]) {
  GameStateManager::singleton().initialize_with_state(new BuildState());
  return 0;
}
