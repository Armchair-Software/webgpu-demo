struct vertex_input {
  @location(0) position: vec3f,
  @location(1) colour: vec4f,
};

struct fragment_input {
    @builtin(position) position: vec4f,
    @location(0) colour: vec4f,
};
alias vertex_output = fragment_input;

@vertex
fn vs_main(in: vertex_input) -> vertex_output {
  var out: vertex_output;
  out.position = vec4f(in.position, 1.0);
  out.colour = in.colour;
  return out;
}

@fragment
fn fs_main(in: fragment_input) -> @location(0) vec4f {
  return in.colour;
}
