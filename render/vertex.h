#pragma once

#include "vectorstorm/vector/vector3.h"
#include "vectorstorm/vector/vector4.h"

struct vertex {
  vec3f position;
  vec3f normal;
  vec4f colour;
};
static_assert(sizeof(vertex) == sizeof(vertex::position) + sizeof(vertex::normal) + sizeof(vertex::colour)); // make sure the struct is packed
