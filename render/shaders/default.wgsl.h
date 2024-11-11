namespace render::shaders {
inline constexpr char const *default_wgsl{R"85c0de8c4a74f357(struct vertex_input {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) colour: vec4f,
};
struct vertex_output {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
    @location(1) colour: vec4f,
};
const model_view_projection_matrix = mat4x4f(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
const normal_matrix = mat3x3f(1, 1, 1, 1, 1, 1, 1, 1, 1);
@vertex
fn vs_main(in: vertex_input) -> vertex_output {
  var out: vertex_output;
  out.position = vec4f(in.position, 1.0);
  out.normal = normal_matrix * in.normal;
  out.colour = in.colour;
  return out;
}
@fragment
fn fs_main(in: vertex_output) -> @location(0) vec4f {
  return in.colour;
}
)85c0de8c4a74f357"};}
