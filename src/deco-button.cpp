#include "deco-button.hpp"
#include "deco-theme.hpp"
#include <wayfire/opengl.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>
#include <stdlib.h>

#define NORMAL   1.0
#define HOVERED  0.0

namespace wf
{
namespace pixdecor
{
button_t::button_t(pixdecor_theme_t& t, std::function<void()> damage) :
    theme(t), damage_callback(damage)
{}

wf::dimensions_t button_t::set_button_type(button_type_t type)
{
    this->type = type;
    this->hover.animate(NORMAL, NORMAL);
    auto size = update_texture();
    add_idle_damage();

    return size;
}

button_type_t button_t::get_button_type() const
{
    return this->type;
}

void button_t::set_hover(bool is_hovered)
{
    this->is_hovered = is_hovered;
    if (!this->is_pressed)
    {
        if (is_hovered)
        {
            this->hover.animate(HOVERED);
        } else
        {
            this->hover.animate(NORMAL);
        }
    }

    add_idle_damage();
}

/**
 * Set whether the button is pressed or not.
 * Affects appearance.
 */
void button_t::set_pressed(bool is_pressed)
{
    this->is_pressed = is_pressed;
    if (is_pressed)
    {
        this->hover.animate(NORMAL);
    } else
    {
        this->hover.animate(is_hovered ? HOVERED : NORMAL);
    }

    add_idle_damage();
}

void button_t::render(const wf::scene::render_instruction_t& data, wf::geometry_t geometry)
{
    if (this->hover.running())
    {
        add_idle_damage();
    }

    OpenGL::render_texture(
        wf::gles_texture_t{button_texture.get_texture()}, data.target, geometry, {1, 1, 1, this->hover},
        OpenGL::RENDER_FLAG_CACHED);
    data.pass->custom_gles_subpass(data.target, [&]
    {
        for (auto& box : data.damage)
        {
            wf::gles::render_target_logic_scissor(data.target, wlr_box_from_pixman_box(box));
            OpenGL::draw_cached();
        }
    });
    OpenGL::clear_cached();

    OpenGL::render_texture(
        wf::gles_texture_t{button_texture_hovered.get_texture()}, data.target, geometry, {1, 1, 1, 1.0 - this->hover},
        OpenGL::RENDER_FLAG_CACHED);
    data.pass->custom_gles_subpass(data.target, [&]
    {
        for (auto& box : data.damage)
        {
            wf::gles::render_target_logic_scissor(data.target, wlr_box_from_pixman_box(box));
            OpenGL::draw_cached();
        }
    });
    OpenGL::clear_cached();
}

wf::dimensions_t button_t::update_texture()
{
    pixdecor_theme_t::button_state_t state = {
        .width  = 1.0 * (theme.get_font_height_px() >= LARGE_ICON_THRESHOLD ? 26 : 18),
        .height = 1.0 * (theme.get_font_height_px() >= LARGE_ICON_THRESHOLD ? 26 : 18),
        .border = 1.0,
    };

    auto surfaces = theme.get_button_surface(type, state);
    wf::dimensions_t size_normal{cairo_image_surface_get_width(surfaces->normal),
        cairo_image_surface_get_height(surfaces->normal)};
    // wf::dimensions_t size_hovered{cairo_image_surface_get_width(surfaces->hovered),
    // cairo_image_surface_get_height(surfaces->hovered)};

    wf::gles::run_in_context([&]
    {
        this->button_texture = owned_texture_t{surfaces->normal};
        this->button_texture_hovered = owned_texture_t{surfaces->hovered};
    });

    cairo_surface_destroy(surfaces->normal);
    cairo_surface_destroy(surfaces->hovered);
    surfaces.reset();

    return size_normal;
}

void button_t::add_idle_damage()
{
    this->idle_damage.run_once([=] ()
    {
        this->damage_callback();
    });
}
}
}
