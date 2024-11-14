#pragma once

#include "vectorstorm/vector/vector3.h"

using triangle_index = vec3<uint16_t>;
static_assert(sizeof(triangle_index) == sizeof(uint16_t) * 3);                  // make sure the vector is packed
