#include "vis.h"

#include "hh.h"

// TODO: globe_tex_offset is a bandaid solution,
// at the very least, make sure its cross-platform consistent
#define OFFSET 5.f
#define STACKS 48
#define SLICES 64
#define COUNT_V (SLICES * (STACKS - 1) + 2)
#define COUNT_E (SLICES * 6 * (STACKS - 1))

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

struct vis_layer_globe_state {
    GLuint VAO, VBO, EBO, tex;
};

bool 
vis_layer_globe_events(void* const data, const RGFW_window* const win) {
    (void) data;
    (void) win;
    return false;
}

void 
vis_layer_globe_render(const void* const data) {
    const struct vis_layer_globe_state* state = data;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->tex);
    glBindVertexArray(state->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->EBO);
    glDrawElements(GL_TRIANGLES, COUNT_E, GL_UNSIGNED_INT, (GLvoid*) 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
}

void 
vis_layer_globe_deinit(void* const data) {
    struct vis_layer_globe_state* state = data;
    glDeleteTextures(1, &state->tex);
    glDeleteVertexArrays(1, &state->VAO);
    glDeleteBuffers(1, &state->VBO);
    glDeleteBuffers(1, &state->EBO);
}

static const char* shader_source_globe = \
    "#version 330 core\n"
    "in vec3 pos;\n"
    "uniform sampler2D tex_sampler;\n"
    "uniform float tex_offset;\n"
    "out vec4 color_out;"
    "void main() {"
    "    float lam = atan(pos.x, pos.y) - radians(tex_offset);\n"
    "    float u = 0.5 - lam / radians(360.0);\n"
    "    float v = 1.0 - pos.z / radians(180.0);\n"
    "    color_out = texture(tex_sampler, vec2(u, v));\n"
    "}\n";

bool
Vis_layer_globe(Vis* const vis, const char* path_globe_image) {
    // construct globe geometry
    HH_ASSERT(STACKS > 2 && SLICES > 2, "Unreachable!");
    GLfloat* vertices = malloc(sizeof(GLfloat) * COUNT_V * 4);
    if(vertices == NULL) {
        HH_ERR("Failed to allocate space for vertices.");
        return false;
    }
    size_t k_v = 0;
    vertices[k_v++] = 180.f;
    vertices[k_v++] = 0.f;
    vertices[k_v++] = 0.f;
    vertices[k_v++] = 1.f;
    for(size_t i = 0; i < (STACKS - 1); ++i) {
        float phi = 180.f * (float) (i + 1) / (float) STACKS;
        for(size_t j = 0; j < SLICES; ++j) {
            vertices[k_v++] = (GLfloat) 360.f * (GLfloat) j / (GLfloat) SLICES;
            vertices[k_v++] = phi;
            vertices[k_v++] = 0.f;
            vertices[k_v++] = 1.f;
        }
    }
    vertices[k_v++] = 180.f;
    vertices[k_v++] = 180.f;
    vertices[k_v++] = 0.f;
    vertices[k_v++] = 1.f;
    HH_ASSERT(k_v == COUNT_V * 4, "INVALID! %zu, %i", k_v, COUNT_V);
    GLuint* indices = malloc(sizeof(GLuint) * COUNT_E);
    if(indices == NULL) {
        HH_ERR("Failed to allocate space for indices.");
        free(vertices);
        return false;
    }
    size_t k_i = 0;
    for(GLuint i = 0; i < SLICES; ++i) {
        GLuint i0 = i + 1;
        GLuint i1 = (i0 % SLICES) + 1;
        indices[k_i++] = 0;
        indices[k_i++] = i1;
        indices[k_i++] = i0;
        i0 = i + SLICES * (STACKS - 2) + 1;
        i1 = (i + 1) % SLICES + SLICES * (STACKS - 2) + 1;
        indices[k_i++] = COUNT_V - 1;
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
    GLuint tex;
    tex = image_build_tex(image_globe, GL_TEXTURE0);
    if(tex == 0) return false;
    image_free(&image_globe);
    // create the fragment shader
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_globe);
    if(!frag) return false;
    // allocate space for the layer data
    struct vis_layer_globe_state* state = Vis_add_layer(vis, frag, (VisLayerMethods) {
        .events = vis_layer_globe_events,
        .render = vis_layer_globe_render,
        .deinit = vis_layer_globe_deinit }, sizeof(struct vis_layer_globe_state));
    // assign texture
    state->tex = tex;
    // set uniforms
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glUniform1i(glGetUniformLocation((GLuint) program, "tex_sampler"), 0);
    glUniform1f(glGetUniformLocation((GLuint) program, "tex_offset"), OFFSET);
    // buffers (vis->buf)
    glGenVertexArrays(1, &state->VAO);
    glGenBuffers(1, &state->VBO);
    glGenBuffers(1, &state->EBO);
    glBindVertexArray(state->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    size_t buffer_size;
    buffer_size = COUNT_V * 4 * sizeof(GLfloat);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) buffer_size, vertices, GL_STATIC_DRAW);
    free(vertices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->EBO);
    buffer_size = COUNT_E * sizeof(GLuint);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) buffer_size, indices, GL_STATIC_DRAW);
    free(indices);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (GLvoid*) 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    return true;
}
