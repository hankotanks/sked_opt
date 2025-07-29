#include "vis.h"

#include <math.h>
#include <stdbool.h>
#include <glenv.h>

#include "hh.h"

#define M_PI_2 1.57079632679489661923132169163975144209858469968755291048747229615390820314f

#define SENSITIVITY 0.002f
#define SCALAR 4.f
#define RADIUS 100.f
#define STACKS 48
#define SLICES 64
#define COUNT_VERT (SLICES * (STACKS - 1) + 2)
#define COUNT_IDX (SLICES * 6 * (STACKS - 1))

#define FOV M_PI_2
#define ZNEAR 1.f
#define ZFAR SCALAR * RADIUS * 2.f

struct BMP {
    unsigned char header[54];
    size_t data_offset, data_size;
    size_t w, h;
    unsigned char* data;
};

// fails gracefully, truthy if succeeded
bool
image_load(struct BMP* const img, const char* path) {
    FILE* stream = fopen(path, "rb");
    HH_CHECK_STREAM(stream, stream != NULL, "Failed to open image [%s].", path) return false;
    size_t header_size = fread(img->header, 1, 54, stream);
    HH_CHECK_STREAM(stream, header_size == 54 && img->header[0] == 'B' && img->header[1] == 'M', \
        "Image header was malformed [%s].", path) return false;
    img->data_offset = (size_t) (*(int*) &(img->header[0x0A]));
    img->data_size = (size_t) (*(int*) &(img->header[0x22]));
    img->w = (size_t) (*(int*) &(img->header[0x12]));
    img->h = (size_t) (*(int*) &(img->header[0x16]));
    if(img->data_size == 0) {
        HH_DBG("Image header does report size of data [%s].", path);
        img->data_size = img->w * img->h * 3;
    }
    if(img->data_offset == 0) {
        HH_DBG("Image data offset not indicated by header [%s].", path);
        img->data_offset = 54;
    }
    HH_MALLOC(img->data, img->data_size);
    size_t read_size = fread(img->data, 1, img->data_size, stream);
    HH_CHECK_STREAM(stream, read_size >= img->data_size, "Image data size did not match allocated buffer [%s].", path) {
        free(img->data);
        return false;
    }
    fclose(stream);
    return true;
}

void
image_free(const struct BMP* const img) {
    free(img->data);
}

// fails gracefully, 0 if failed
GLuint
image_build_tex(const struct BMP img, GLenum tex_unit) {
    GLuint tex;
    glActiveTexture(tex_unit);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLsizei w = (GLsizei) img.w;
    GLsizei h = (GLsizei) img.h;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_BGR, GL_UNSIGNED_BYTE, img.data);
    if(glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(0);
        return 0;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(0);
    return tex;
}

static const char* shader_source_vert = \
    "#version 330 core\n"
    "layout(location = 0) in vec3 v_lam_phi;\n"
    "uniform mat4 proj;\n"
    "uniform mat4 view;\n"
    "uniform float globe_radius;\n"
    "uniform float shell_radius;\n"
    "uniform float gmst;\n"
    "flat out uint f_type;\n"
    "out vec3 f_lam_phi;\n"
    "void main() {\n"
    "    bool v_type = (abs(v_lam_phi.z) != 0.f);\n"
    "    float rad = v_type ? shell_radius : globe_radius;\n"
    "    float lam = radians(v_type ? (gmst - v_lam_phi.x) : (v_lam_phi.x));\n"
    "    float phi = radians(v_lam_phi.y);\n"
    "    float x = sin(phi) * cos(lam) * rad;\n"
    "    float y = cos(phi) * rad;\n"
    "    float z = sin(phi) * sin(lam) * rad;\n"
    "    gl_Position = proj * view * vec4(x, y, z, 1.f);\n"
    "    f_type = v_type ? 0u : 1u;\n"
    "    f_lam_phi = vec3(sin(lam), cos(lam), phi);\n"
    "}\n";

static const char* shader_source_globe = \
    "#version 330 core\n"
    "in vec3 f_lam_phi;\n"
    "out vec4 color;"
    "uniform float globe_tex_offset;\n"
    "uniform sampler2D globe_tex_sampler;\n"
    "void main() {"
    "    float lam = atan(f_lam_phi.x, f_lam_phi.y) - radians(globe_tex_offset);\n"
    "    float u = 0.5 - lam / radians(360.0);\n"
    "    float v = 1.0 - f_lam_phi.z / radians(180.0);\n"
    "    color = texture(globe_tex_sampler, vec2(u, v));\n"
    "}\n";

#pragma GCC diagnostic ignored "-Wunused-variable"
static const char* shader_source_color = \
    "#version 330 core\n"
    "flat in uint f_type;\n"
    "uniform vec3 fst_color;\n"
    "uniform vec3 snd_color;\n"
    "out vec4 f_color;\n"
    "void main() {\n"
    "    bool b_type = (f_type != 0u);\n"
    "    f_color = vec4(b_type ? fst_color : snd_color, 1.f);\n"
    "}\n";

// fails gracefully, 0 on failure
GLuint
shader_compile_from_source(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        GLint log_size = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
        char log[log_size];
        glGetShaderInfoLog(shader, log_size, NULL, log);
        HH_ERR("Failed to %s compile shader.\n%s", (type == GL_VERTEX_SHADER) ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void
Vis_update_projection(Vis* const vis, const RGFW_window* const win) {
    vis->camera.aspect = (float) win->r.w / (float) win->r.h;
    memset(vis->camera.proj, 0, sizeof(GLfloat) * 16);
    vis->camera.proj[ 0] = (GLfloat) (1.f / (vis->camera.aspect * tanf(FOV / 2.f))); 
    vis->camera.proj[ 5] = (GLfloat) (1.f / tanf(FOV / 2.f));
    vis->camera.proj[10] = (GLfloat) ((ZFAR + ZNEAR) / (ZFAR - ZNEAR) * -1.f); 
    vis->camera.proj[11] = (GLfloat) (-1.f);
    vis->camera.proj[14] = (GLfloat) ((2.f * ZFAR * ZNEAR) / (ZFAR - ZNEAR) * -1.f); 
}

bool
Vis_init(Vis* const vis, const RGFW_window* const win, const char* path_globe_image) {
    // construct globe geometry
    HH_ASSERT(STACKS > 2 && SLICES > 2, "Unreachable!");
    GLfloat vertices[COUNT_VERT * 3];
    size_t k_v = 0;
    vertices[k_v++] = 180.f;
    vertices[k_v++] = 0.f;
    vertices[k_v++] = 0.f;
    for(size_t i = 0; i < (STACKS - 1); ++i) {
        float phi = 180.f * (float) (i + 1) / (float) STACKS;
        for(size_t j = 0; j < SLICES; ++j) {
            vertices[k_v++] = (GLfloat) 360.f * (GLfloat) j / (GLfloat) SLICES;
            vertices[k_v++] = phi;
            vertices[k_v++] = 0.f;
        }
    }
    vertices[k_v++] = 180.f;
    vertices[k_v++] = 180.f;
    vertices[k_v++] = 0.f;
    HH_ASSERT(k_v == COUNT_VERT * 3, "INVALID! %zu, %i", k_v, COUNT_VERT);
    GLuint indices[COUNT_IDX];
    size_t k_i = 0;
    for(GLuint i = 0; i < SLICES; ++i) {
        GLuint i0 = i + 1;
        GLuint i1 = (i0 % SLICES) + 1;
        indices[k_i++] = 0;
        indices[k_i++] = i1;
        indices[k_i++] = i0;
        i0 = i + SLICES * (STACKS - 2) + 1;
        i1 = (i + 1) % SLICES + SLICES * (STACKS - 2) + 1;
        indices[k_i++] = COUNT_VERT - 1;
        indices[k_i++] = i0;
        indices[k_i++] = i1;
    }
    for(GLuint j = 0; j < (STACKS - 2); ++j) {
        GLuint j0 = j * SLICES + 1;
        GLuint j1 = (j + 1) * SLICES + 1;
        for(GLuint i = 0; i < SLICES; ++i) {
            GLuint i0 = j0 + i;
            GLuint i1 = j0 + (i + 1) % SLICES;
            GLuint i2 = j1 + (i + 1) % SLICES;
            GLuint i3 = j1 + i;
            indices[k_i++] = i3; indices[k_i++] = i0; 
            indices[k_i++] = i1;
            indices[k_i++] = i1; indices[k_i++] = i2; 
            indices[k_i++] = i3;
        }
    }
    // load texture (vis->tex)
    struct BMP image_globe;
    if(!image_load(&image_globe, path_globe_image)) return false;
    vis->tex = image_build_tex(image_globe, GL_TEXTURE0);
    if(vis->tex == 0) return false;
    image_free(&image_globe);
    // shaders
    GLuint vert = shader_compile_from_source(GL_VERTEX_SHADER, shader_source_vert);
    HH_ASSERT(vert, "Failed to compile vertex shader.");
    if(!vert) return false;
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_globe);
    if(!frag) {
        glDeleteShader(vert);
        return false;
    }
    // shader program (vis->program)
    vis->program = glCreateProgram();
    glAttachShader(vis->program, vert);
    glAttachShader(vis->program, frag);
    glLinkProgram(vis->program);
    glDeleteShader(vert);
    glDeleteShader(frag);
    GLint success;
    glGetProgramiv(vis->program, GL_LINK_STATUS, &success);
    if(!success) {
        GLint log_size = 0;
        glGetProgramiv(vis->program, GL_INFO_LOG_LENGTH, &log_size);
        char log[log_size];
        glGetProgramInfoLog(vis->program, log_size, NULL, log);
        HH_ERR("Failed to link shader program: \n%s", log);
        glDeleteProgram(vis->program);
        return false;
    }
    glUseProgram(vis->program);
    // buffers (vis->buf)
    glGenVertexArrays(1, &vis->VAO);
    glGenBuffers(1, &vis->VBO);
    glGenBuffers(1, &vis->EBO);
    glBindVertexArray(vis->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, vis->VBO);
    size_t buffer_size;
    buffer_size = COUNT_VERT * 3 * sizeof(GLfloat);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) buffer_size, vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vis->EBO);
    buffer_size = COUNT_IDX * sizeof(GLuint);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) buffer_size, indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, (GLvoid*) 0);
    glEnableVertexAttribArray(0);
    // pass the sampler for the earth texture
    vis->loc_proj = glGetUniformLocation(vis->program, "proj");
    vis->loc_view = glGetUniformLocation(vis->program, "view");
    glUniform1f(glGetUniformLocation(vis->program, "globe_radius"), RADIUS);
    glUniform1f(glGetUniformLocation(vis->program, "globe_tex_offset"), 0.f);
    glUniform1i(glGetUniformLocation(vis->program, "globe_tex_sampler"), 0);
    glUseProgram(0);
    // camera (vis->camera)
    {
        vis->camera.azi = 0.f;
        vis->camera.ele = 0.f;
        vis->camera.rad = RADIUS * (SCALAR - 1.f);
        vis->camera.aspect = 1.f;
        vis->camera.min = RADIUS;
        vis->camera.max = RADIUS * SCALAR;
        memset(vis->camera.proj, 0, sizeof(GLfloat) * 16);
        memset(vis->camera.view, 0, sizeof(GLfloat) * 16);
    }
    // controller (vis->cont)
    {
        memset(&(vis->cont), 0, sizeof(vis->cont));
    }
    Vis_update_projection(vis, win);
    return true;
}

void
Vis_free(Vis* const vis) {
    glDeleteProgram(vis->program);
    glDeleteTextures(1, &(vis->tex));
    glDeleteVertexArrays(1, &(vis->VAO));
    glDeleteBuffers(2, &(vis->VBO));
}

inline static float 
mag(const GLfloat vec[static 3]) {
    return sqrtf((float) (vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]));
}

inline static void 
normalize(GLfloat vec[static 3]) {
    float length = mag(vec);
    if(length > 0.f) {
        vec[0] /= (GLfloat) length;
        vec[1] /= (GLfloat) length;
        vec[2] /= (GLfloat) length;
    }
}

inline static void 
cross(GLfloat out[static 3], const GLfloat a[static 3], const GLfloat b[static 3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

inline static float 
dot(const GLfloat a[static 3], const GLfloat b[static 3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline static void
look_at(GLfloat view[static 16], const GLfloat eye[static 3], const GLfloat up[static 3]) {
    GLfloat f[3], s[3], u[3];
    f[0] = eye[0] * -1.f;
    f[1] = eye[1] * -1.f;
    f[2] = eye[2] * -1.f;
    normalize(f);
    cross(s, f, up);
    normalize(s);
    cross(u, s, f);
    view[0] = s[0]; view[1] = u[0]; view[ 2] = -f[0]; view[ 3] = 0.f;
    view[4] = s[1]; view[5] = u[1]; view[ 6] = -f[1]; view[ 7] = 0.f;
    view[8] = s[2]; view[9] = u[2]; view[10] = -f[2]; view[11] = 0.f;
    view[12] = -dot(s, eye); 
    view[13] = -dot(u, eye); 
    view[14] =  dot(f, eye); view[15] = 1.f;
}

void
Vis_update_and_draw(Vis* const vis) {
    // update
    static const GLfloat up[3] = { 0.f, 1.f, 0.f };
    GLfloat eye[3];
    eye[0] = (GLfloat) (vis->camera.rad * cosf(vis->camera.ele) * sinf(vis->camera.azi));
    eye[1] = (GLfloat) (vis->camera.rad * sinf(vis->camera.ele));
    eye[2] = (GLfloat) (vis->camera.rad * cosf(vis->camera.ele) * cosf(vis->camera.azi));
    look_at(vis->camera.view, eye, up);
    // draw
    glUseProgram(vis->program);
    glUniformMatrix4fv(vis->loc_proj, 1, GL_FALSE, vis->camera.proj);
    glUniformMatrix4fv(vis->loc_view, 1, GL_FALSE, vis->camera.view);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glUseProgram(vis->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vis->tex);
    glBindVertexArray(vis->VAO);
    glDrawElements(GL_TRIANGLES, COUNT_IDX, GL_UNSIGNED_INT, (GLvoid*) 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glActiveTexture(0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
}

void
Vis_handle_events(Vis* const vis, const RGFW_window* const win) {
    int32_t x, y, dx, dy;
    switch(win->event.type) {
        case RGFW_windowResized:
            Vis_update_projection(vis, win);
            break;
        case RGFW_mouseButtonPressed:
            vis->cont.drag = true;
            float rad_vel = vis->camera.min * sqrtf(SENSITIVITY) * 2.f;
            float rad_min = vis->camera.min + rad_vel;
            switch(win->event.button) {
                case RGFW_mouseScrollUp:
                    vis->camera.rad = HH_MAX(rad_min, vis->camera.rad - rad_vel);
                    break;
                case RGFW_mouseScrollDown:
                    vis->camera.rad = HH_MIN(vis->camera.max, vis->camera.rad + rad_vel);
                    break;
                default: break;
            }
            break;
        case RGFW_mouseButtonReleased:
            vis->cont.drag = false;
            break;
        case RGFW_mousePosChanged:
            x = win->event.point.x;
            y = win->event.point.y;
            if(vis->cont.drag && vis->cont.init) {
                dx = x - vis->cont.mouse_x;
                dy = y - vis->cont.mouse_y;
                vis->camera.azi -= SENSITIVITY * (float) dx;
                vis->camera.ele += SENSITIVITY * (float) dy;
                if(vis->camera.ele > M_PI_2 * 0.9f)  vis->camera.ele = M_PI_2 *  0.9f;
                if(vis->camera.ele < M_PI_2 * -0.9f) vis->camera.ele = M_PI_2 * -0.9f;
            }
            vis->cont.mouse_x = x;
            vis->cont.mouse_y = y;
            vis->cont.init = true;
            break;
        default: break;
    }
}

