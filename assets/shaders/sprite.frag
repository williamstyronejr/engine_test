#version 460 core
layout(binding=0) uniform sampler2D atlas;
layout(location=0) uniform int premultiplied;
in vec2 uv;
in vec4 color;
layout(location=0) out vec4 output_color;
void main() {
    output_color=texture(atlas,uv)*color;
    if (premultiplied != 0) output_color.rgb *= color.a;
}
