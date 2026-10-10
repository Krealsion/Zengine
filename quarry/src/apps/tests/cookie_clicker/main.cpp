#include "cookie_clicker_state.h"

using namespace Zen;

int main(int argc, char *argv[]) {
  GameStateManager::singleton().initialize_with_state(new CookieClickerState());
  return 0;
}
