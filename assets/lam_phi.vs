#version 330 core
layout(location = 0) in vec3 v_lam_phi;
uniform mat4 proj;
uniform mat4 view;
uniform float globe_radius;
uniform float shell_radius;
uniform float gmst;
flat out uint f_type;
out vec3 f_lam_phi;
void main() {
    bool v_type = (abs(v_lam_phi.z) != 0.f);
    float rad = v_type ? shell_radius : globe_radius;
    float lam = radians(v_type ? (gmst - v_lam_phi.x) : (v_lam_phi.x));
    float phi = radians(v_lam_phi.y);
    float x = sin(phi) * cos(lam) * rad;
    float y = cos(phi) * rad;
    float z = sin(phi) * sin(lam) * rad;
    gl_Position = proj * view * vec4(x, y, z, 1.f);
    f_type = v_type ? 0u : 1u;
    f_lam_phi = vec3(sin(lam), cos(lam), phi);
}