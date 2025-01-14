#version 410

in vec2 texcoord;

uniform sampler2D colortex0;  // albedo
uniform sampler2D colortex1;  // 
uniform sampler2D colortex2;  // normal
uniform sampler2D colortex7;  // lighting

uniform sampler2D depthtex0;  // depth

out vec4 FragColor;

void main() {
  FragColor = texture(colortex7, texcoord);
}
