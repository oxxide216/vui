#version 450

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;

layout(location = 0) out vec4 o_color;

void main() {
  if (i_uv.x < -0.001 || i_uv.y < -0.001)
    discard;
  float len = length(i_uv);
  o_color = vec4(i_color.rgb, i_color.a * smoothstep(0.5 + fwidth(len), 0.5, len));
}
