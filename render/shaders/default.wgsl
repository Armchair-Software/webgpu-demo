struct vertex_input {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) colour: vec4f,
};

struct vertex_output {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat, first) normal: vec3f,
  @location(1) @interpolate(flat, first) colour: vec4f,
};

struct uniform_struct {
  model_view_projection_matrix: mat4x4f,
  normal_matrix: mat3x3f,
};

@group(0) @binding(0) var<uniform> uniforms: uniform_struct;

@vertex
fn vs_main(in: vertex_input) -> vertex_output {
  var out: vertex_output;
  out.position = uniforms.model_view_projection_matrix * vec4f(in.position, 1.0);
  out.normal = uniforms.normal_matrix * in.normal;
  out.colour = in.colour;
  return out;
}

@fragment
fn fs_main(in: vertex_output) -> @location(0) vec4f {
  return in.colour;
}
