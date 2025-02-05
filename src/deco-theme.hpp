#pragma once
#include <gio/gio.h>
#include <wayfire/render-manager.hpp>
#include "deco-button.hpp"
#include "deco-effects.hpp"

#define MIN_RESIZE_HANDLE_SIZE 5
#define LARGE_ICON_THRESHOLD 20
#define MIN_BAR_HEIGHT 20

namespace wf
{
namespace pixdecor
{
/**
 * A  class which manages the outlook of decorations.
 * It is responsible for determining the background colors, sizes, etc.
 */
class pixdecor_theme_t
{
  public:
    wf::option_wrapper_t<std::string> title_font{"pixdecor/title_font"};
    wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
    wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
    wf::option_wrapper_t<bool> maximized_borders{"pixdecor/maximized_borders"};
    wf::option_wrapper_t<bool> maximized_shadows{"pixdecor/maximized_shadows"};
    wf::option_wrapper_t<int> title_text_align{"pixdecor/title_text_align"};
    /** Create a new theme with the default parameters */
    pixdecor_theme_t();
    ~pixdecor_theme_t();

    /** @return The height of the system font in pixels */
    int get_font_height_px();
    /** @return The available height for displaying the title */
    int get_title_height();
    /** @return The available border for rendering */
    int get_border_size() const;
    /** @return The available border for resizing */
    int get_input_size() const;
    /** @return The decoration color */
    wf::color_t get_decor_color(bool active) const;
    PangoFontDescription *create_font_description();
    PangoFontDescription *get_font_description();

    void update_colors(void);

    /**
     * Fill the given rectangle with the background color(s).
     *
     * @param fb The target framebuffer, must have been bound already.
     * @param rectangle The rectangle to redraw.
     * @param scissor The GL scissor rectangle to use.
     * @param active Whether to use active or inactive colors
     */
    void render_background(const wf::render_target_t& fb,
        wf::geometry_t rectangle, const wf::region_t& scissor, bool active, wf::pointf_t p);

    /**
     * Render the given text on a cairo_surface_t with the given size.
     * The caller is responsible for freeing the memory afterwards.
     */
    cairo_surface_t *render_text(std::string text, int width, int height, int t_width, int border,
        int buttons_width, bool active);

    struct button_state_t
    {
        /** Button width */
        double width;
        /** Button height */
        double height;
        /** Button outline size */
        double border;
        /* Hovering... */
        bool hover;
    };

    /** background effects */
    smoke_t smoke;

    /**
     * Get the icon for the given button.
     * The caller is responsible for freeing the memory afterwards.
     *
     * @param button The button type.
     * @param state The button state.
     */
    cairo_surface_t *get_button_surface(button_type_t button,
        const button_state_t& state, bool active) const;

    void set_maximize(bool state);

  private:

    GSettings *gs;
    wf::color_t fg;
    wf::color_t bg;
    wf::color_t fg_text;
    wf::color_t bg_text;
    bool maximized;
};
}
}
