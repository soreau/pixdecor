#include "deco-button.hpp"
#include "deco-theme.hpp"
#include <wayfire/opengl.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>
#include <stdlib.h>

#define NORMAL   1.0
#define PRESSED  0.5
#define HOVERED  0.25

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
        this->hover.animate(PRESSED);
    } else
    {
        this->hover.animate(is_hovered ? HOVERED : NORMAL);
    }

    add_idle_damage();
}

void button_t::render(const wf::render_target_t& fb, wf::geometry_t geometry, const wf::region_t& scissor)
{
    OpenGL::render_texture(button_texture.tex, fb, geometry, {1, 1, 1, this->hover},
        OpenGL::TEXTURE_TRANSFORM_INVERT_Y | OpenGL::RENDER_FLAG_CACHED);
    for (auto& box : scissor)
    {
        fb.logic_scissor(wlr_box_from_pixman_box(box));
        OpenGL::draw_cached();
    }

    OpenGL::clear_cached();

    if (this->hover.running())
    {
        add_idle_damage();
    }
}

wf::dimensions_t button_t::update_texture()
{
    pixdecor_theme_t::button_state_t state = {
        .width  = 1.0 * (theme.get_font_height_px() >= LARGE_ICON_THRESHOLD ? 26 : 18),
        .height = 1.0 * (theme.get_font_height_px() >= LARGE_ICON_THRESHOLD ? 26 : 18),
        .border = 1.0,
        .hover  = this->is_hovered,
    };

    auto surface = theme.get_button_surface(type, state, this->active);
    wf::dimensions_t size{cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface)};

    OpenGL::render_begin();
    cairo_surface_upload_to_texture(surface, this->button_texture);
    OpenGL::render_end();

    cairo_surface_destroy(surface);

    return size;
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
