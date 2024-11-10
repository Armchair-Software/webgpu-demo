namespace render::shaders {
inline constexpr char const *default_wgsl{R"6353d0af5502145f(@vertex
fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
  return vec4f(in_vertex_position, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
  return vec4f(1.0, 1.0, 0.0, 1.0);
}
)6353d0af5502145f"};}
