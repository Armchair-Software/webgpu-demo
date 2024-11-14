#pragma once

#include "vectorstorm/matrix/matrix3.h"

struct alignas(16) uniforms {
  mat4f model_view_projection_matrix;
  mat3fwgpu normal_matrix;
};
