#pragma once

#include <string>
#include <wayfire/util.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/util/duration.hpp>
#include <wayfire/plugins/common/simple-texture.hpp>

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

namespace wf
{
namespace pixdecor
{
class pixdecor_theme_t;

enum button_type_t
{
    BUTTON_CLOSE,
    BUTTON_TOGGLE_MAXIMIZE,
    BUTTON_MINIMIZE,
};

class button_t
{
  public:
    /**
     * Create a new button with the given theme.
     * @param theme  The theme to use.
     * @param damage_callback   A callback to execute when the button needs a
     * repaint. Damage won't be reported while render() is being called.
     */
    button_t(pixdecor_theme_t& theme,
        std::function<void()> damage_callback);

    /**
     * Set the type of the button. This will affect the displayed icon and
     * potentially other appearance like colors.
     */
    wf::dimensions_t set_button_type(button_type_t type);

    /** @return The type of the button */
    button_type_t get_button_type() const;

    /**
     * Set the button hover state.
     * Affects appearance.
     */
    void set_hover(bool is_hovered);

    /**
     * Set whether the button is pressed or not.
     * Affects appearance.
     */
    void set_pressed(bool is_pressed);

    /**
     * Render the button on the given framebuffer at the given coordinates.
     * Precondition: set_button_type() has been called, otherwise result is no-op
     *
     * @param buffer The target framebuffer
     * @param geometry The geometry of the button, in logical coordinates
     * @param scissor The scissor rectangle to render.
     */
    void render(const wf::render_target_t& fb, wf::geometry_t geometry,
        const wf::region_t& scissor);

    pixdecor_theme_t& theme;
    std::function<void()> damage_callback;

  private:

    button_type_t type;
    wf::simple_texture_t button_texture;
    bool active = false;
    wf::geometry_t geometry;

    /* Whether the button is currently being hovered */
    bool is_hovered = false;
    /* Whether the button is currently being held */
    bool is_pressed = false;
    /* The shade of button background to use. */
    wf::animation::simple_animation_t hover{wf::create_option(500)};

    wf::wl_idle_call idle_damage;
    /** Damage button the next time the main loop goes idle */
    void add_idle_damage();

    /**
     * Redraw the button surface and store it as a texture
     */
    wf::dimensions_t update_texture();
};
}
}
