#include "shape.h"

#include "3d/vector3.h"

namespace Zen {
void Shape::connect(Vector3 A, Vector3 B){
  if (A == B){
    return;
  }
  int index = -1;
  for (int i = 0; i < _points.size(); i++){
    if (_points[i] == A){
      if (index == -1){
        index = i;
      }else{
        _edges.push_back(std::make_tuple(index, i));
      }
    }
    if (_points[i] == B){
      if (index == -1){
        index = i;
      } else {
        _edges.push_back(std::make_tuple(index, i));
      }
    }
  }
}
}
