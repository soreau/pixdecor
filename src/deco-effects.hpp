#pragma once
#include <wayfire/option-wrapper.hpp>
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <map>
#include <GLES3/gl32.h>

namespace wf
{
namespace decor
{
class smoke_t
{
    /** background effects */
    GLuint motion_program,
        diffuse1_program, diffuse2_program,
        project1_program, project2_program, project3_program,
        project4_program, project5_program, project6_program,
        advect1_program, advect2_program, render_program, render_overlay_program,
        texture, b0u, b0v, b0d, b1u, b1v, b1d, neural_network_tex;

    int saved_width = -1, saved_height = -1;

    wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
    wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};

  public:
    smoke_t();
    ~smoke_t();

    void run_shader(GLuint program, int width, int height, int title_height, int border_size);
    void step_effect(const wf::render_target_t& fb, wf::geometry_t rectangle,
        bool ink, wf::pointf_t p, wf::color_t decor_color, wf::color_t effect_color,
        int title_height, int border_size, int diffuse_iterations);
    void render_effect(const wf::render_target_t& fb, wf::geometry_t rectangle,
        const wf::geometry_t& scissor);
    void recreate_textures(wf::geometry_t rectangle);
    void create_programs();
    void destroy_programs();
    void create_textures();
    void destroy_textures();
    void effect_updated();
};
}
}
