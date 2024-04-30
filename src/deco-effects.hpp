#pragma once
#include <wayfire/option-wrapper.hpp>
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <GLES3/gl32.h>

namespace wf
{
namespace pixdecor
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

    OpenGL::program_t fragment_effect_only_program{};
    int saved_width = -1, saved_height = -1;

    wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
    wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
    wf::option_wrapper_t<std::string> compute_fragment_shader{"pixdecor/compute_fragment_shader"};
    wf::option_wrapper_t<bool> effect_animate{"pixdecor/animate"};
    wf::option_wrapper_t<int> rounded_corner_radius{"pixdecor/rounded_corner_radius"};
    wf::option_wrapper_t<wf::color_t> shadow_color{"pixdecor/shadow_color"};
    wf::option_wrapper_t<int> beveled_radius{"pixdecor/beveled_radius"};
    int last_shadow_radius = 0;

  public:
    smoke_t();
    ~smoke_t();

    void run_shader(GLuint program, int width, int height, int title_height, int border_size, int radius, int beveled_radius);
    void run_shader_region(GLuint program, const wf::region_t & region, const wf::dimensions_t & size);
    void run_fragment_shader(const wf::render_target_t& fb,
        wf::geometry_t rectangle, const wf::region_t& scissor,int border_size, int title_height);

    void dispatch_region(const wf::region_t& region);

    void step_effect(const wf::render_target_t& fb, wf::geometry_t rectangle,
        bool ink, wf::pointf_t p, wf::color_t decor_color, wf::color_t effect_color,
        int title_height, int border_size, int shadow_radius);
    void render_effect(const wf::render_target_t& fb, wf::geometry_t rectangle, const wf::region_t& scissor,int border_size,int title_height );
    void recreate_textures(wf::geometry_t rectangle);
    void create_programs();
    void destroy_programs();
    void create_textures();
    void destroy_textures();
    void effect_updated();
};
}
}
