namespace render::shaders {
inline constexpr char const *default_wgsl{R"af74c7699af757e5(struct vertex_input {
  @location(0) position: vec2f,
  //@location(1) colour: vec4f,
};

struct fragment_input {
    @builtin(position) position: vec4f,
    @location(0) colour: vec4f,
};
alias vertex_output = fragment_input;

@vertex
fn vs_main(in: vertex_input) -> vertex_output {
  var out: vertex_output;
  out.position = vec4f(in.position, 0.0, 1.0);
  out.colour = vec4f(1.0, 1.0, 0.0, 1.0);
  return out;
}

@fragment
fn fs_main(in: fragment_input) -> @location(0) vec4f {
  return in.colour;
}
)af74c7699af757e5"};}
