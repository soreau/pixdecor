#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <map>
#include <stdlib.h>

namespace wf
{
namespace decor
{
wf::option_wrapper_t<int> border_size{"pixdecor/border_size"};
wf::option_wrapper_t<wf::color_t> fg_color{"pixdecor/fg_color"};
wf::option_wrapper_t<wf::color_t> bg_color{"pixdecor/bg_color"};
wf::option_wrapper_t<wf::color_t> fg_text_color{"pixdecor/fg_text_color"};
wf::option_wrapper_t<wf::color_t> bg_text_color{"pixdecor/bg_text_color"};
wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
wf::option_wrapper_t<wf::color_t> effect_color{"pixdecor/effect_color"};
/** Create a new theme with the default parameters */
decoration_theme_t::decoration_theme_t()
{
    gs = g_settings_new("org.gnome.desktop.interface");

    // read initial colours
    update_colors();
}

decoration_theme_t::~decoration_theme_t()
{}

void decoration_theme_t::update_colors(void)
{
    if (!read_colour("theme_selected_bg_color", fg))
    {
        fg = wf::color_t(fg_color);
    }

    if (!read_colour("theme_selected_fg_color", fg_text))
    {
        fg_text = wf::color_t(fg_text_color);
    }

    if (!read_colour("theme_unfocused_bg_color", bg))
    {
        bg = wf::color_t(bg_color);
    }

    if (!read_colour("theme_unfocused_fg_color", bg_text))
    {
        bg_text = wf::color_t(bg_text_color);
    }
}

gboolean decoration_theme_t::read_colour(const char *name, wf::color_t & col)
{
    FILE *fp;
    char *cmd, *line, *theme;
    size_t len;
    int i, n, ir, ig, ib;

    theme = g_settings_get_string(gs, "gtk-theme");

    for (i = 0; i < 2; i++)
    {
        n    = 0;
        line = NULL;
        len  = 0;
        cmd  = g_strdup_printf(
            "bash -O extglob -c \"grep -hPo '(?<=@define-color\\s%s\\s)#[0-9A-Fa-f]{6}' %s/themes/%s/gtk-3.0/!(*-dark).css 2> /dev/null\"", name,
            i ? "/usr/share" : g_get_user_data_dir(), theme);
        fp = popen(cmd, "r");
        if (fp)
        {
            if (getline(&line, &len, fp) > 0)
            {
                n = sscanf(line, "#%02x%02x%02x", &ir, &ig, &ib);
                g_free(line);
            }

            pclose(fp);
        }

        g_free(cmd);

        if (n == 3)
        {
            col = {ir / 255.0, ig / 255.0, ib / 255.0, 1.0};
            g_free(theme);
            return true;
        }
    }

    g_free(theme);
    return false;
}

/** @return The available height for displaying the title */
int decoration_theme_t::get_font_height_px() const
{
    char *font = g_settings_get_string(gs, "font-name");

    PangoFontDescription *font_desc = pango_font_description_from_string(font);
    int font_height = pango_font_description_get_size(font_desc);
    g_free(font);
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

    return height;
}

/** @return The available border for resizing */
int decoration_theme_t::get_border_size() const
{
    return border_size;
}

/** @return The input area for resizing */
int decoration_theme_t::get_input_size() const
{
    return std::max(int(border_size), MIN_RESIZE_HANDLE_SIZE);
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
    wf::geometry_t rectangle, const wf::geometry_t& scissor, bool active, wf::pointf_t p)
{
    if (std::string(effect_type) == "none")
    {
        OpenGL::render_begin(fb);
        fb.logic_scissor(scissor);
        OpenGL::render_rectangle(rectangle, get_decor_color(active), fb.get_orthographic_projection());
        OpenGL::render_end();
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
    int width, int height, int t_width, bool active) const
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
    char *font = g_settings_get_string(gs, "font-name");
    int w, h;

    // render text
    font_desc = pango_font_description_from_string(font);

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), text.size());
    cairo_set_source_rgba(cr, active ? fg_text.r : bg_text.r, active ? fg_text.g : bg_text.g,
        active ? fg_text.b : bg_text.b, 1);
    pango_layout_get_pixel_size(layout, &w, &h);
    cairo_translate(cr, (t_width - w) / 2, (height - h) / 2);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font_desc);
    g_object_unref(layout);
    cairo_destroy(cr);
    g_free(font);

    return surface;
}

cairo_surface_t*decoration_theme_t::get_button_surface(button_type_t button,
    const button_state_t& state, bool active) const
{
    cairo_surface_t *cspng;
    const char *icon_name;
    char *iconfile, *theme;

    switch (button)
    {
      case BUTTON_CLOSE:
        icon_name = "close";
        break;

      case BUTTON_TOGGLE_MAXIMIZE:
        icon_name = "maximize";
        break;

      case BUTTON_MINIMIZE:
        icon_name = "minimize";
        break;
    }

    theme    = g_settings_get_string(gs, "icon-theme");
    iconfile = g_strdup_printf("/usr/share/icons/%s/%s/ui/window-%s-symbolic.symbolic.png", theme,
        get_font_height_px() >= LARGE_ICON_THRESHOLD ? "24x24" : "16x16", icon_name);
    g_free(theme);

    // read the icon into a surface
    cspng = cairo_image_surface_create_from_png(iconfile);
    g_free(iconfile);

    return cspng;
}
}
}
