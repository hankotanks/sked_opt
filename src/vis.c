#include "vis.h"

#include <math.h>
#include <stdbool.h>
#include <glenv.h>

#include "hh.h"

#define M_PI_2 1.57079632679489661923132169163975144209858469968755291048747229615390820314f

#define SENSITIVITY 0.002f
#define SCALAR 4.f

#define RADIUS 100.f
#define FOV M_PI_2
#define ZNEAR 1.f
#define ZFAR SCALAR * RADIUS * 2.f

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
VisCamera_init(VisCamera* camera, const RGFW_window* const win) {
    camera->azi = 0.f;
    camera->ele = 0.f;
    camera->rad = RADIUS * (SCALAR - 1.f);
    camera->aspect = 1.f;
    camera->min = RADIUS;
    camera->max = RADIUS * SCALAR;
    memset(camera->view, 0, sizeof(GLfloat) * 16);
    VisCamera_update_projection(camera, win);
}

void
VisCamera_update_projection(VisCamera* camera, const RGFW_window* const win) {
    camera->aspect = (float) win->r.w / (float) win->r.h;
    memset(camera->proj, 0, sizeof(GLfloat) * 16);
    camera->proj[ 0] = (GLfloat) (1.f / (camera->aspect * tanf(FOV / 2.f))); 
    camera->proj[ 5] = (GLfloat) (1.f / tanf(FOV / 2.f));
    camera->proj[10] = (GLfloat) ((ZFAR + ZNEAR) / (ZFAR - ZNEAR) * -1.f); 
    camera->proj[11] = (GLfloat) (-1.f);
    camera->proj[14] = (GLfloat) ((2.f * ZFAR * ZNEAR) / (ZFAR - ZNEAR) * -1.f); 
}

void
VisLayerDesc_init(VisLayerDesc* const desc, size_t data_size) {
    desc->type = NONE;
    desc->data_size = data_size;
}

void
VisLayerDesc_configure_panel(VisLayerDesc* const desc, glenv_Panel* panel, const char* parent_title) {
    desc->type = (desc->type == PASS) ? BOTH : PANEL;
    desc->panel.panel = panel;
    desc->panel.parent_title = parent_title;
}

void
VisLayerDesc_configure_pass(VisLayerDesc* const desc, GLuint frag, VisPassMethods methods) {
    desc->type = (desc->type == PANEL) ? BOTH : PASS;
    desc->pass.frag = frag;
    desc->pass.methods = methods;
}

// NOTE:
// vertices are given in the form [lon, lat, rad, active] to the shader
// where rad == 0.f for points on the globe and rad == 1.f for celestial objects
// active == 1.f if the vertex is included in the schedule
static const char* shader_source_vert = \
    "#version 330 core\n"
    "layout(location = 0) in vec4 lam_phi;\n"
    "uniform mat4 proj;\n"
    "uniform mat4 view;\n"
    "uniform float globe_radius;\n"
    "uniform float shell_radius;\n"
    "uniform float gmst;\n"
    "flat out uint state;\n"
    "out vec3 pos;\n"
    "void main() {\n"
    "    bool shell = (abs(lam_phi.z) != 0.f);\n"
    "    float rad = shell ? shell_radius : globe_radius;\n"
    "    float lam = radians(shell ? (gmst - lam_phi.x) : (lam_phi.x));\n"
    "    float phi = radians(lam_phi.y);\n"
    "    float x = sin(phi) * cos(lam) * rad;\n"
    "    float y = cos(phi) * rad;\n"
    "    float z = sin(phi) * sin(lam) * rad;\n"
    "    state = (abs(lam_phi.w) < 0.5f) ? 0u : 1u;\n"
    "    gl_Position = proj * view * vec4(x, y, z, 1.f);\n"
    "    gl_PointSize = (state != 0u) ? 8.0 : 5.0;\n"
    "    pos = vec3(sin(lam), cos(lam), phi);\n"
    "}\n";

bool
Vis_init(Vis* const vis, RGFW_window* const win) {
    // camera (vis->camera)
    VisCamera_init(&vis->camera, win);
    // controller (vis->cont)
    memset(&(vis->cont), 0, sizeof(vis->cont));
    // vert
    vis->vert = shader_compile_from_source(GL_VERTEX_SHADER, shader_source_vert);
    if(!(vis->vert)) return false;
    // layers
    vis->layers = NULL;
    return true;
}

void
Vis_free(Vis* const vis) {
    for(size_t i = 0, len = hh_arrlen(vis->layers); i < len; ++i) {
        switch(vis->layers[i].type) {
        case BOTH:
        case PASS:
            (vis->layers[i].pass.methods.deinit)(vis->layers[i].data);
            glDeleteProgram(vis->layers[i].pass.program);
            if(vis->layers[i].type != BOTH) break;
        case PANEL:
            free(vis->layers[i].panel);
            break;
        case NONE:
        default: HH_UNREACHABLE;
        }
        free(vis->layers[i].data);
    }
    glDeleteShader(vis->vert);
    hh_arrfree(vis->layers);
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
    view[12] = dot(s, eye) * -1.f; 
    view[13] = dot(u, eye) * -1.f; 
    view[14] = dot(f, eye); view[15] = 1.f;
}

void
Vis_update_and_draw(Vis* const vis, const float gmst) {
    // update camera
    static const GLfloat up[3] = { 0.f, 1.f, 0.f };
    GLfloat eye[3];
    eye[0] = (GLfloat) (vis->camera.rad * cosf(vis->camera.ele) * sinf(vis->camera.azi));
    eye[1] = (GLfloat) (vis->camera.rad * sinf(vis->camera.ele));
    eye[2] = (GLfloat) (vis->camera.rad * cosf(vis->camera.ele) * cosf(vis->camera.azi));
    look_at(vis->camera.view, eye, up);
    // update layer vertex uniforms and render
    for(size_t i = 0, len = hh_arrlen(vis->layers); i < len; ++i) {
        switch(vis->layers[i].type) {
        case BOTH:
        case PASS:
            glUseProgram(vis->layers[i].pass.program);
            glEnable(GL_PROGRAM_POINT_SIZE);
            glUniformMatrix4fv(vis->layers[i].pass.loc_proj, 1, GL_FALSE, vis->camera.proj);
            glUniformMatrix4fv(vis->layers[i].pass.loc_view, 1, GL_FALSE, vis->camera.view);
            glUniform1f(vis->layers[i].pass.loc_gmst, gmst);
            (vis->layers[i].pass.methods.render)(vis->layers[i].data);
            glDisable(GL_PROGRAM_POINT_SIZE);
            glUseProgram(0);
            if(vis->layers[i].type != BOTH) break;
        case PANEL:
            glenv_Panel_render(vis->layers[i].panel, vis->layers[i].data);
            break;
        case NONE:
        default: HH_UNREACHABLE;
        }
    }
}

void
Vis_handle_events(Vis* const vis, const RGFW_window* const win) {
    int x, y, dx, dy;
    switch(win->event.type) {
    case RGFW_windowResized:
        glViewport(0, 0, (GLsizei) win->r.w, (GLsizei) win->r.h);
        VisCamera_update_projection(&vis->camera, win);
        for(size_t i = 0, len = hh_arrlen(vis->layers); i < len; ++i) {
            switch(vis->layers[i].type) {
            case BOTH:
            case PANEL:
                glenv_Panel_resize(vis->layers[i].panel, vis);
                break;
            case PASS:
                break;
            case NONE:
            default: HH_UNREACHABLE;
            }
        }
        break;
    case RGFW_mouseButtonPressed:
        if(glenv_consumed_mouse()) break;
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

glenv_Panel*
Vis_get_panel(Vis* const vis, const char* title) {
    if(title == NULL || title[0] == '\0') return NULL;
    const char* title_curr;
    for(size_t i = 0, len = hh_arrlen(vis->layers); i < len; ++i) {
        if(vis->layers[i].panel == NULL) continue;
        title_curr = glenv_Panel_title(vis->layers[i].panel);
        if(strcmp(title, title_curr) == 0) {
            return vis->layers[i].panel;
        }
    }
    return NULL;
}

void*
Vis_add_layer(Vis* const vis, VisLayerDesc desc) {
    if(desc.type == NONE) return NULL;
    hh_arradd(vis->layers, 1);
    hh_arrlast(vis->layers).type = desc.type;
    hh_arrlast(vis->layers).data = NULL;
    if(desc.data_size) hh_arrlast(vis->layers).data = malloc(desc.data_size);
    if(desc.data_size && hh_arrlast(vis->layers).data == NULL) {
        hh_arrpop(vis->layers);
        return NULL;
    }
    glenv_Panel* parent;
    switch(hh_arrlast(vis->layers).type) {
    case BOTH:
    case PASS: 
        hh_arrlast(vis->layers).pass.methods = desc.pass.methods;
        hh_arrlast(vis->layers).pass.program = glCreateProgram();
        glAttachShader(hh_arrlast(vis->layers).pass.program, vis->vert);
        glAttachShader(hh_arrlast(vis->layers).pass.program, desc.pass.frag);
        glLinkProgram(hh_arrlast(vis->layers).pass.program);
        glDeleteShader(desc.pass.frag);
        GLint success;
        glGetProgramiv(hh_arrlast(vis->layers).pass.program, GL_LINK_STATUS, &success);
        if(!success) {
            GLint log_size = 0;
            glGetProgramiv(hh_arrlast(vis->layers).pass.program, GL_INFO_LOG_LENGTH, &log_size);
            char log[log_size];
            glGetProgramInfoLog(hh_arrlast(vis->layers).pass.program, log_size, NULL, log);
            HH_ERR("Failed to link shader program: \n%s", log);
            glDeleteProgram(hh_arrlast(vis->layers).pass.program);
            return false;
        }
        glUseProgram(hh_arrlast(vis->layers).pass.program);
        hh_arrlast(vis->layers).pass.loc_proj = glGetUniformLocation(hh_arrlast(vis->layers).pass.program, "proj");
        hh_arrlast(vis->layers).pass.loc_view = glGetUniformLocation(hh_arrlast(vis->layers).pass.program, "view");
        hh_arrlast(vis->layers).pass.loc_gmst = glGetUniformLocation(hh_arrlast(vis->layers).pass.program, "gmst");
        glUniform1f(glGetUniformLocation(hh_arrlast(vis->layers).pass.program, "globe_radius"), RADIUS);
        glUniform1f(glGetUniformLocation(hh_arrlast(vis->layers).pass.program, "shell_radius"), RADIUS * SCALAR);
        if(desc.type != BOTH) break;
    case PANEL:
        parent = Vis_get_panel(vis, desc.panel.parent_title);
        if(parent != NULL) glenv_Panel_set_parent(desc.panel.panel, parent);
        else HH_ASSERT(desc.panel.parent_title == NULL, "Unable to set %s's parent panel to %s.", 
            glenv_Panel_title(desc.panel.panel), desc.panel.parent_title);
        glenv_Panel_resize(desc.panel.panel, vis);
        hh_arrlast(vis->layers).panel = desc.panel.panel;
        break;
    case NONE:
    default: HH_UNREACHABLE;
    }
    return hh_arrlast(vis->layers).data;
}
