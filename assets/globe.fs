#version 330 core
in vec3 f_lam_phi;
out vec4 color;
uniform float globe_tex_offset;
uniform sampler2D globe_tex_sampler;
void main() {
    float lam = atan(f_lam_phi.x, f_lam_phi.y) - radians(globe_tex_offset);
    float u = 0.5 - lam / radians(360.0);
    float v = 1.0 - f_lam_phi.z / radians(180.0);
    color = texture(globe_tex_sampler, vec2(u, v));
}