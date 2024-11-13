namespace render::shaders {
inline constexpr char const *default_wgsl{R"93b4d71f9569621b(struct vertex_input {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) colour: vec4f,
};
struct vertex_output {
  @builtin(position) position: vec4f,
  @location(1) @interpolate(flat, first) colour: vec4f,
};
struct uniform_struct {
  model_view_projection_matrix: mat4x4f,
  normal_matrix: mat3x3f,
};
@group(0) @binding(0) var<uniform> uniforms: uniform_struct;
const light_dir: vec3f = normalize(vec3f(1.0, 0.25, -0.5));
const ambient = 0.5f;
@vertex
fn vs_main(in: vertex_input) -> vertex_output {
  var out: vertex_output;
  out.position = uniforms.model_view_projection_matrix * vec4f(in.position, 1.0);
  let transformed_normal = uniforms.normal_matrix * in.normal;
  let diffuse_intensity = (max(dot(transformed_normal, light_dir), 0.0) * (1.0 - ambient)) + ambient;
  out.colour = vec4f(in.colour.rgb * diffuse_intensity, in.colour.a);
  return out;
}
@fragment
fn fs_main(in: vertex_output) -> @location(0) vec4f {
  return in.colour;
}
)93b4d71f9569621b"};}
