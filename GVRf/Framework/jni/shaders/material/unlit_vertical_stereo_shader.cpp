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
 * Renders a vertically split texture without light.
 ***************************************************************************/

#include "unlit_vertical_stereo_shader.h"

#include "gl/gl_program.h"
#include "objects/material.h"
#include "objects/mesh.h"
#include "objects/components/render_data.h"
#include "objects/textures/texture.h"
#include "shaders/gl_names.h"
#include "util/gvr_gl.h"

namespace gvr {
static const char VERTEX_SHADER[] = //
        "attribute vec4 "A_POSITION";\n"
        "attribute vec4 "A_TEX_COORD";\n"
        "uniform mat4 "U_MVP";\n"
        "varying vec2 "V_TEX_COORD";\n"
        "void main() {\n"
        "  "V_TEX_COORD" = "A_TEX_COORD".xy;\n"
        "  gl_Position = "U_MVP" * "A_POSITION";\n"
        "}\n";

static const char FRAGMENT_SHADER[] = //
        "precision highp float;\n"
                "uniform sampler2D "U_TEXTURE";\n"
        "uniform vec3 "U_COLOR";\n"
        "uniform float "U_OPACITY";\n"
        "uniform int "U_RIGHT";\n"
        "varying vec2 "V_TEX_COORD";\n"
        "void main()\n"
        "{\n"
        "  vec2 tex_coord = vec2("V_TEX_COORD".x, 0.5 * ("V_TEX_COORD".y + float("U_RIGHT")));\n"
        "  vec4 color = texture2D("U_TEXTURE", tex_coord);\n"
        "  gl_FragColor = vec4(color.r * "U_COLOR".r * "U_OPACITY", color.g * "U_COLOR".g * "U_OPACITY", color.b * "U_COLOR".b * "U_OPACITY", color.a * "U_OPACITY");\n"
        "}\n";

UnlitVerticalStereoShader::UnlitVerticalStereoShader() :
        program_(0), a_position_(0), a_tex_coord_(0), u_mvp_(0), u_texture_(0), u_color_(
                0), u_opacity_(0), u_right_(0) {
    program_ = new GLProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    a_position_ = glGetAttribLocation(program_->id(), A_POSITION);
    a_tex_coord_ = glGetAttribLocation(program_->id(), A_TEX_COORD);
    u_mvp_ = glGetUniformLocation(program_->id(), U_MVP);
    u_texture_ = glGetUniformLocation(program_->id(), U_TEXTURE);
    u_color_ = glGetUniformLocation(program_->id(), U_COLOR);
    u_opacity_ = glGetUniformLocation(program_->id(), U_OPACITY);
    u_right_ = glGetUniformLocation(program_->id(), U_RIGHT);
}

UnlitVerticalStereoShader::~UnlitVerticalStereoShader() {
    if (program_ != 0) {
        recycle();
    }
}

void UnlitVerticalStereoShader::recycle() {
    delete program_;
    program_ = 0;
}

void UnlitVerticalStereoShader::render(const glm::mat4& mvp_matrix,
        RenderData* render_data, bool right) {
    Mesh* mesh = render_data->mesh();
    Texture* texture = render_data->material()->getTexture(MAIN_TEXTURE);
    glm::vec3 color = render_data->material()->getVec3(COLOR);
    float opacity = render_data->material()->getFloat(OPACITY);

    if (texture->getTarget() != GL_TEXTURE_2D) {
        std::string error =
                "UnlitVerticalStereoShader::render : texture with wrong target";
        throw error;
    }

#if _GVRF_USE_GLES3_
    mesh->setVertexLoc(a_position_);
    mesh->setTexCoordLoc(a_tex_coord_);
    mesh->generateVAO();

    glUseProgram(program_->id());

    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(mvp_matrix));
    glActiveTexture (GL_TEXTURE0);
    glBindTexture(texture->getTarget(), texture->getId());
    glUniform1i(u_texture_, 0);
    glUniform3f(u_color_, color.r, color.g, color.b);
    glUniform1f(u_opacity_, opacity);
    glUniform1i(u_right_, right ? 1 : 0);

    glBindVertexArray(mesh->getVAOId());
    glDrawElements(GL_TRIANGLES, mesh->triangles().size(), GL_UNSIGNED_SHORT,
            0);
    glBindVertexArray(0);
#else
    glUseProgram(program_->id());

    glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0,
            mesh->vertices().data());
    glEnableVertexAttribArray(a_position_);

    glVertexAttribPointer(a_tex_coord_, 2, GL_FLOAT, GL_FALSE, 0,
            mesh->tex_coords().data());
    glEnableVertexAttribArray(a_tex_coord_);

    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(mvp_matrix));

    glActiveTexture (GL_TEXTURE0);
    glBindTexture(texture->getTarget(), texture->getId());
    glUniform1i(u_texture_, 0);

    glUniform3f(u_color_, color.r, color.g, color.b);

    glUniform1f(u_opacity_, opacity);

    glUniform1i(u_right_, right ? 1 : 0);

    glDrawElements(GL_TRIANGLES, mesh->triangles().size(), GL_UNSIGNED_SHORT,
            mesh->triangles().data());
#endif

    checkGlError("UnlitShader::render");
}

}
