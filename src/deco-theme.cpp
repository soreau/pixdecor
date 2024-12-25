#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <map>
#include <mutex>
#include <stdlib.h>

namespace wf
{
namespace pixdecor
{
wf::option_wrapper_t<int> border_size{"pixdecor/border_size"};
wf::option_wrapper_t<int> title_text_align{"pixdecor/title_text_align"};
wf::option_wrapper_t<bool> titlebar{"pixdecor/titlebar"};
wf::option_wrapper_t<bool> maximized_borders{"pixdecor/maximized_borders"};
wf::option_wrapper_t<bool> maximized_shadows{"pixdecor/maximized_shadows"};
wf::option_wrapper_t<std::string> title_font{"pixdecor/title_font"};
wf::option_wrapper_t<wf::color_t> fg_color{"pixdecor/fg_color"};
wf::option_wrapper_t<wf::color_t> bg_color{"pixdecor/bg_color"};
wf::option_wrapper_t<wf::color_t> fg_text_color{"pixdecor/fg_text_color"};
wf::option_wrapper_t<wf::color_t> bg_text_color{"pixdecor/bg_text_color"};
wf::option_wrapper_t<std::string> button_minimize_image{"pixdecor/button_minimize_image"};
wf::option_wrapper_t<std::string> button_maximize_image{"pixdecor/button_maximize_image"};
wf::option_wrapper_t<std::string> button_close_image{"pixdecor/button_close_image"};
wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
wf::option_wrapper_t<wf::color_t> button_color{"pixdecor/button_color"};
wf::option_wrapper_t<double> button_line_thickness{"pixdecor/button_line_thickness"};
/** Create a new theme with the default parameters */
decoration_theme_t::decoration_theme_t()
{
    gs = g_settings_new("org.gnome.desktop.interface");

    // read initial colours
    update_colors();
}

decoration_theme_t::~decoration_theme_t()
{
    g_object_unref(gs);
}

void decoration_theme_t::update_colors(void)
{
    fg = wf::color_t(fg_color);
    bg = wf::color_t(bg_color);
    fg_text = wf::color_t(fg_text_color);
    bg_text = wf::color_t(bg_text_color);
}

/**
 * @return A PangoFontDescription representing either a provided font from
 *  the Title Font option or the system font, scaled by the text-scaling-factor GSetting.
 */
PangoFontDescription *create_font_description()
{
    GSettings *gs = g_settings_new("org.gnome.desktop.interface");

    std::string title_font_val(title_font);
    bool using_system_font{};

    if (title_font_val.empty())
    {
        char *font = g_settings_get_string(gs, "font-name");
        title_font_val = font;
        using_system_font = true;
        g_free(font);
    }

    PangoFontDescription *font_desc = pango_font_description_from_string(title_font_val.c_str());
    int font_size = pango_font_description_get_size(font_desc);
    bool font_size_absolute;

    // font size will be 0 if nothing is specified - grab from system font if this is the case
    // if the font is the system font, then just pray it works
    if (!font_size && !using_system_font)
    {
        char *font = g_settings_get_string(gs, "font-name");
        PangoFontDescription *sys_font_desc = pango_font_description_from_string(font);

        font_size = pango_font_description_get_size(sys_font_desc);
        font_size_absolute = pango_font_description_get_size_is_absolute(sys_font_desc);

        pango_font_description_free(sys_font_desc);
        g_free(font);
    } else
    {
        font_size_absolute = pango_font_description_get_size_is_absolute(font_desc);
    }

    // scale the font size if we got it by text-scaling-factor
    if (font_size)
    {
        double scaling_factor = g_settings_get_double(gs, "text-scaling-factor");
        if (!scaling_factor) // should default to 1.0, but better safe than sorry
        {
            scaling_factor = 1.0;
        }

        if (font_size_absolute)
        {
            pango_font_description_set_absolute_size(font_desc, font_size * scaling_factor);
        } else
        {
            pango_font_description_set_size(font_desc, font_size * scaling_factor);
        }
    }

    g_object_unref(gs);
    return font_desc;
}

/**
 * @return A PangoFontDescription from create_font_description(),
 *  originating from a global, internally managed instance, freeing the data upon exit.
 *  It will also be updated with changes to the Title Font option.
 */
PangoFontDescription *get_font_description()
{
    static std::unique_ptr<PangoFontDescription, decltype(&pango_font_description_free)> font_desc(
        create_font_description(), &pango_font_description_free);

    static std::once_flag once_flag;
    std::call_once(once_flag, [] {
        title_font.set_callback([] {
            font_desc.reset(create_font_description());
        });
    });

    return font_desc.get();
}

/** @return The available height for displaying the title */
int decoration_theme_t::get_font_height_px() const
{
    PangoFontDescription *font_desc = get_font_description();
    int font_height = pango_font_description_get_size(font_desc);

    if (!pango_font_description_get_size_is_absolute(font_desc))
    {
        font_height *= 4;
        font_height /= 3;
    }

    return font_height / PANGO_SCALE;
}


int decoration_theme_t::get_title_height() const
{
    int height = get_font_height_px();
    height *= 3;
    height /= 2;
    height += 8;
    if (height < MIN_BAR_HEIGHT)
    {
        height = MIN_BAR_HEIGHT;
    }

    return titlebar ? height : 0;
}

/** @return The available border for resizing */
int decoration_theme_t::get_border_size() const
{
    return (!maximized_borders && maximized_shadows && maximized) ? 0 : border_size;
}

/** @return The input area for resizing */
int decoration_theme_t::get_input_size() const
{
    return std::max(get_border_size(), MIN_RESIZE_HANDLE_SIZE);
}

wf::color_t decoration_theme_t::get_decor_color(bool active) const
{
    return active ? fg : bg;
}

void decoration_theme_t::set_maximize(bool state)
{
    maximized = state;
}

/**
 * Fill the given rectangle with the background color(s).
 *
 * @param fb The target framebuffer, must have been bound already
 * @param rectangle The rectangle to redraw.
 * @param scissor The GL scissor rectangle to use.
 * @param active Whether to use active or inactive colors
 */
void decoration_theme_t::render_background(const wf::render_target_t& fb,
    wf::geometry_t rectangle, const wf::region_t& scissor, bool active, wf::pointf_t p)
{
    if ((std::string(effect_type) == "none") && (std::string(overlay_engine) == "none"))
    {
        for (auto& box : scissor)
        {
            fb.logic_scissor(wlr_box_from_pixman_box(box));
            OpenGL::render_rectangle(rectangle, get_decor_color(active), fb.get_orthographic_projection());
        }
    } else
    {
        smoke.render_effect(fb, rectangle, scissor);
    }
}

/**
 * Render the given text on a cairo_surface_t with the given size.
 * The caller is responsible for freeing the memory afterwards.
 */
cairo_surface_t*decoration_theme_t::render_text(std::string text,
    int width, int height, int t_width, int border, int buttons_width, bool active) const
{
    const auto format = CAIRO_FORMAT_ARGB32;
    auto surface = cairo_image_surface_create(format, width, height);

    if (height == 0)
    {
        return surface;
    }

    auto cr = cairo_create(surface);

    PangoFontDescription *font_desc;
    PangoLayout *layout;
    int x, w, h;

    // render text
    font_desc = get_font_description();

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), text.size());
    cairo_set_source_rgba(cr, active ? fg_text.r : bg_text.r, active ? fg_text.g : bg_text.g,
        active ? fg_text.b : bg_text.b, 1);
    pango_layout_get_pixel_size(layout, &w, &h);
    switch (int(title_text_align))
    {
      // left
      case 0:
        x = border;
        break;

      // right
      case 2:
        x = t_width - (w + buttons_width + border);
        break;

      // center
      case 1:
      default:
        x = (t_width - w) / 2;
        break;
    }

    cairo_translate(cr, x, (height - h) / 2);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    cairo_destroy(cr);

    return surface;
}

cairo_surface_t*decoration_theme_t::get_button_surface(button_type_t button,
    const button_state_t& state, bool active) const
{
    cairo_surface_t *button_surface = NULL;

    switch (button)
    {
      case BUTTON_CLOSE:
        if (!std::string(button_close_image).empty())
        {
            button_surface = cairo_image_surface_create_from_png(std::string(button_close_image).c_str());
        }
        break;

      case BUTTON_TOGGLE_MAXIMIZE:
        if (!std::string(button_maximize_image).empty())
        {
            button_surface = cairo_image_surface_create_from_png(std::string(button_maximize_image).c_str());
        }
        break;

      case BUTTON_MINIMIZE:
        if (!std::string(button_minimize_image).empty())
        {
            button_surface = cairo_image_surface_create_from_png(std::string(button_minimize_image).c_str());
        }
        break;
      default:
        break;
    }

    if (button_surface && cairo_surface_status(button_surface) == CAIRO_STATUS_SUCCESS)
    {
        return button_surface;
    }

    auto w = state.width;
    auto h = state.height;

    button_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, w, h);

    auto cr = cairo_create(button_surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /** Draw the button  */
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_source_rgba(cr,
        wf::color_t(button_color).r,
        wf::color_t(button_color).g,
        wf::color_t(button_color).b,
        wf::color_t(button_color).a);
    double line_width = button_line_thickness;
    switch (button)
    {
      case BUTTON_CLOSE:
        cairo_set_line_width(cr, line_width * state.border * 100.0);
        cairo_move_to(cr, 1.0 * w / 4.0,
            1.0 * h / 4.0);
        cairo_line_to(cr, 3.0 * w / 4.0,
            3.0 * h / 4.0);
        cairo_move_to(cr, 3.0 * w / 4.0,
            1.0 * h / 4.0);
        cairo_line_to(cr, 1.0 * w / 4.0,
            3.0 * h / 4.0);
        cairo_stroke(cr);
        break;

      case BUTTON_TOGGLE_MAXIMIZE:
        cairo_set_line_width(cr, line_width * state.border * 100.0);
        cairo_rectangle(cr, w / 4.0, h / 4.0, w / 2.0, h / 2.0);
        cairo_stroke(cr);
        break;

      case BUTTON_MINIMIZE:
        cairo_set_line_width(cr, line_width * state.border * 100.0);
        cairo_move_to(cr, 1.0 * w / 4.0,
            3.0 * h / 4.0);
        cairo_line_to(cr, 3.0 * w / 4.0,
            3.0 * h / 4.0);
        cairo_stroke(cr);
        break;

      default:
        assert(false);
    }

    cairo_destroy(cr);

    return button_surface;
}
}
}
