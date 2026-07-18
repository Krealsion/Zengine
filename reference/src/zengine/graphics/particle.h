#pragma once

#include "types/color.h"
#include "types/vector2.h"

namespace Zen {
struct ParticleContructor {
  Vector2 position;
  Vector2 movement;
  Color color;
  bool emission;
  float emission_strength;
  Color emission_color;
};

class Particle {
public:


};
}
