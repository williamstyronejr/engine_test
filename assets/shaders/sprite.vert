#version 460 core
layout(location=0) in vec2 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in vec4 tint;
out vec2 uv;
out vec4 color;
void main() { gl_Position=vec4(position,0,1); uv=texcoord; color=tint; }
