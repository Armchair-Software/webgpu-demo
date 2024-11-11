struct vertex_input {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) colour: vec4f,
};

struct vertex_output {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
    @location(1) colour: vec4f,
};

@group(0) @binding(0) var<uniform> model_view_projection_matrix: mat4x4f;
const normal_matrix = mat3x3f(
  1, 0, 0,
  0, 1, 0,
  0, 0, 1
);

@vertex
fn vs_main(in: vertex_input) -> vertex_output {
  var out: vertex_output;
  out.position = model_view_projection_matrix * vec4f(in.position, 1.0);
  out.normal = normal_matrix * in.normal;
  out.colour = in.colour;
  return out;
}

@fragment
fn fs_main(in: vertex_output) -> @location(0) vec4f {
  return in.colour;
}
