/* Copyright 2015 Samsung Electronics Co., LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/***************************************************************************
 * Renders a cube map texture in reflection mode without light.
 ***************************************************************************/

#include "cubemap_reflection_shader.h"

#include "gl/gl_program.h"
#include "objects/material.h"
#include "objects/mesh.h"
#include "objects/components/render_data.h"
#include "objects/textures/texture.h"
#include "shaders/gl_names.h"
#include "util/gvr_gl.h"

// OpenGL Cube map texture uses coordinate system different to other OpenGL functions:
// Positive x pointing right, positive y pointing up, positive z pointing inward.
// It is a left-handed system, while other OpenGL functions use right-handed system.
// The side faces are also oriented up-side down as illustrated below.
//
// Since the origin of Android bitmap is at top left, and the origin of OpenGL texture
// is at bottom left, when we use Android bitmap to create OpenGL texture, it is already
// up-side down. So we do not need to flip them again.
//
// We do need to flip the z-coordinate to be consistent with the left-handed system.
//    _________
//   /        /|
//  /________/ |
//  |        | |    +y
//  |        | |    |  +z
//  |        | /    | /
//  |________|/     |/___ +x
//
//  Positive x    Positive y    Positive z
//      ______        ______        ______
//     |      |      |      |      |      |
//  -y |      |   +z |      |   -y |      |
//  |  |______|   |  |______|   |  |______|
//  |___ -z       |___ +x       |___ +x
//
//  Negative x    Negative y    Negative z
//      ______        ______        ______
//     |      |      |      |      |      |
//  -y |      |   -z |      |   -y |      |
//  |  |______|   |  |______|   |  |______|
//  |___ +z       |___ +x       |___ -x
//
// (http://www.nvidia.com/object/cube_map_ogl_tutorial.html)
// (http://stackoverflow.com/questions/11685608/convention-of-faces-in-opengl-cubemapping)

namespace gvr {
static const char VERTEX_SHADER[] =
        "attribute vec4 "A_POSITION";\n"
        "attribute vec3 "A_NORMAL";\n"
        "uniform mat4 "U_MV";\n"
        "uniform mat4 "U_MV_IT";\n"
        "uniform mat4 "U_MVP";\n"
        "uniform mat4 "U_VIEW_I";\n"
        "varying vec3 "V_TEX_COORD";\n"
        "void main() {\n"
        "  vec4 v_viewspace_position_vec4 = "U_MV" * "A_POSITION";\n"
        "  vec3 v_viewspace_position = v_viewspace_position_vec4.xyz / v_viewspace_position_vec4.w;\n"
        "  vec3 v_viewspace_normal = ("U_MV_IT" * vec4("A_NORMAL", 1.0f)).xyz;\n"
        "  vec3 v_reflected_position = reflect(v_viewspace_position, normalize(v_viewspace_normal));\n"
        "  "V_TEX_COORD" = ("U_VIEW_I" * vec4(v_reflected_position, 1.0f)).xyz;\n"
        "  "V_TEX_COORD".z = -"V_TEX_COORD".z;\n"
        "  gl_Position = "U_MVP" * "A_POSITION";\n"
        "}\n";

static const char FRAGMENT_SHADER[] =
        "precision highp float;\n"
                "uniform samplerCube "U_TEXTURE";\n"
        "uniform vec3 "U_COLOR";\n"
        "uniform float "U_OPACITY";\n"
        "varying vec3 "V_TEX_COORD";\n"
        "void main()\n"
        "{\n"
        "  vec4 color = textureCube("U_TEXTURE", "V_TEX_COORD".xyz);\n"
        "  gl_FragColor = vec4(color.r * "U_COLOR".r * "U_OPACITY", color.g * "U_COLOR".g * "U_OPACITY", color.b * "U_COLOR".b * "U_OPACITY", color.a * "U_OPACITY");\n"
        "}\n";

CubemapReflectionShader::CubemapReflectionShader() :
        program_(0), a_position_(0), a_normal_(0), u_mv_(0), u_mv_it_(0), u_mvp_(
                0), u_view_i_(0), u_texture_(0), u_color_(0), u_opacity_(0) {
    program_ = new GLProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    a_position_ = glGetAttribLocation(program_->id(), A_POSITION);
    a_normal_ = glGetAttribLocation(program_->id(), A_NORMAL);
    u_mv_ = glGetUniformLocation(program_->id(), U_MV);
    u_mv_it_ = glGetUniformLocation(program_->id(), U_MV_IT);
    u_mvp_ = glGetUniformLocation(program_->id(), U_MVP);
    u_view_i_ = glGetUniformLocation(program_->id(), U_VIEW_I);
    u_texture_ = glGetUniformLocation(program_->id(), U_TEXTURE);
    u_color_ = glGetUniformLocation(program_->id(), U_COLOR);
    u_opacity_ = glGetUniformLocation(program_->id(), U_OPACITY);
}

CubemapReflectionShader::~CubemapReflectionShader() {
    if (program_ != 0) {
        recycle();
    }
}

void CubemapReflectionShader::recycle() {
    delete program_;
    program_ = 0;
}

void CubemapReflectionShader::render(const glm::mat4& mv_matrix,
        const glm::mat4& mv_it_matrix, const glm::mat4& view_invers_matrix,
        const glm::mat4& mvp_matrix, RenderData* render_data) {
    Mesh* mesh = render_data->mesh();
    Texture* texture = render_data->material()->getTexture(MAIN_TEXTURE);
    glm::vec3 color = render_data->material()->getVec3(COLOR);
    float opacity = render_data->material()->getFloat(OPACITY);

    if (texture->getTarget() != GL_TEXTURE_CUBE_MAP) {
        std::string error =
                "CubemapReflectionShader::render : texture with wrong target";
        throw error;
    }

#if _GVRF_USE_GLES3_
    mesh->setVertexLoc(a_position_);
    mesh->setNormalLoc(a_normal_);
    mesh->generateVAO();

    glUseProgram(program_->id());

    glUniformMatrix4fv(u_mv_, 1, GL_FALSE, glm::value_ptr(mv_matrix));
    glUniformMatrix4fv(u_mv_it_, 1, GL_FALSE, glm::value_ptr(mv_it_matrix));
    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(mvp_matrix));
    glUniformMatrix4fv(u_view_i_, 1, GL_FALSE,
            glm::value_ptr(view_invers_matrix));
    glActiveTexture (GL_TEXTURE0);
    glBindTexture(texture->getTarget(), texture->getId());
    glUniform1i(u_texture_, 0);
    glUniform3f(u_color_, color.r, color.g, color.b);
    glUniform1f(u_opacity_, opacity);

    glBindVertexArray(mesh->getVAOId());
    glDrawElements(GL_TRIANGLES, mesh->triangles().size(), GL_UNSIGNED_SHORT,
            0);
    glBindVertexArray(0);
#else
    glUseProgram(program_->id());

    glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0,
            mesh->vertices().data());
    glEnableVertexAttribArray(a_position_);

    glUniformMatrix4fv(u_mv_, 1, GL_FALSE, glm::value_ptr(mv_matrix));
    glUniformMatrix4fv(u_mv_it_, 1, GL_FALSE, glm::value_ptr(mv_it_matrix));
    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(mvp_matrix));
    glUniformMatrix4fv(u_view_i_, 1, GL_FALSE, glm::value_ptr(view_invers_matrix));

    glActiveTexture (GL_TEXTURE0);
    glBindTexture(texture->getTarget(), texture->getId());
    glUniform1i(u_texture_, 0);

    glUniform3f(u_color_, color.r, color.g, color.b);

    glUniform1f(u_opacity_, opacity);

    glDrawElements(GL_TRIANGLES, mesh->triangles().size(), GL_UNSIGNED_SHORT,
            mesh->triangles().data());
#endif

    checkGlError("CubemapReflectionShader::render");
}

}
;
