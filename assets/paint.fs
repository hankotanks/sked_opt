#version 330 core
flat in uint f_type;
uniform vec3 fst_color;
uniform vec3 snd_color;
out vec4 f_color;
void main() {
    bool b_type = (f_type != 0u);
    f_color = vec4(b_type ? fst_color : snd_color, 1.f);
}