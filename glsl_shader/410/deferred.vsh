#version 410


in vec2 vpos;
out vec2 texcoord;

void main() {
  gl_Position = vec4(2*vpos-1, 0.f, 1.f);
  texcoord = vpos;
}
