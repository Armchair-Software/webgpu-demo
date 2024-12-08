#pragma once

#include "vectorstorm/matrix/matrix3.h"
#include "vectorstorm/matrix/matrix4.h"

namespace render {

struct alignas(16) uniforms {
  mat4f model_view_projection_matrix;
  mat3fwgpu normal_matrix;
};

}
