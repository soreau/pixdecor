#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <map>
#include <stdlib.h>
#include <sys/inotify.h>

int handle_theme_updated (int fd, uint32_t mask, void *data)
{
    int bufsz = sizeof (inotify_event) + NAME_MAX + 1;
    char buf[bufsz];

    if ((mask & WL_EVENT_READABLE) == 0) return 0;
    if (read (fd, buf, bufsz) < 0) return 0;

    (*((std::function<void (void)>*) data))();

    return 0;
}

namespace wf
{
namespace decor
{
/** Create a new theme with the default parameters */
decoration_theme_t::decoration_theme_t()
{
    gs = g_settings_new ("org.gnome.desktop.interface");

    // set up the watch on the xsettings file
    inotify_fd = inotify_init1 (IN_CLOEXEC);
    evsrc = wl_event_loop_add_fd (wl_display_get_event_loop (wf::get_core ().display), inotify_fd, WL_EVENT_READABLE,
        handle_theme_updated, &this->update_event);

    // read initial colours
    update_colours ();

    this->update_event = [=] (void)
    {
        update_colours ();
    };

    // enable watches on xsettings file
    char *conf_dir = g_build_filename (g_get_user_config_dir (), "xsettingsd/", NULL);
    char *conf_file = g_build_filename (conf_dir, "xsettingsd.conf", NULL);
    wd_cfg_dir = inotify_add_watch (inotify_fd, conf_dir, IN_CREATE);
    wd_cfg_file = inotify_add_watch (inotify_fd, conf_file, IN_CLOSE_WRITE);
    g_free (conf_file);
    g_free (conf_dir);
}

decoration_theme_t::~decoration_theme_t()
{
    wl_event_source_remove (evsrc);
    inotify_rm_watch (inotify_fd, wd_cfg_file);
    inotify_rm_watch (inotify_fd, wd_cfg_dir);
}

void decoration_theme_t::update_colours (void)
{
    if (!read_colour ("theme_selected_bg_color", fg)) fg = {0.13, 0.13, 0.13, 0.67};
    if (!read_colour ("theme_selected_fg_color", fg_text)) fg_text = {1.0, 1.0, 1.0, 1.0};
    if (!read_colour ("theme_unfocused_bg_color", bg)) bg = {0.2, 0.2, 0.2, 0.87};
    if (!read_colour ("theme_unfocused_fg_color", bg_text)) bg_text = {0.7, 0.7, 0.7, 1.0};
}

gboolean decoration_theme_t::read_colour (const char *name, wf::color_t &col)
{
    FILE *fp;
    char *cmd, *line, *theme;
    size_t len;
    int i, n, ir, ig, ib;

    theme = g_settings_get_string (gs, "gtk-theme");

    for (i = 0; i < 2; i++)
    {
        n = 0;
        line = NULL;
        len = 0;
        cmd = g_strdup_printf ("bash -O extglob -c \"grep -hPo '(?<=@define-color\\s%s\\s)#[0-9A-Fa-f]{6}' %s/themes/%s/gtk-3.0/!(*-dark).css 2> /dev/null\"", name, i ? "/usr/share" : g_get_user_data_dir (), theme);
        fp = popen (cmd, "r");
        if (fp)
        {
            if (getline (&line, &len, fp) > 0)
            {
                n = sscanf (line, "#%02x%02x%02x", &ir, &ig, &ib);
                g_free (line);
            }
            pclose (fp);
        }
        g_free (cmd);

        if (n == 3)
        {
            col = { ir / 255.0, ig / 255.0, ib / 255.0, 1.0 };
            g_free (theme);
            return true;
        }
    }
    g_free (theme);
    return false;
}

/** @return The available height for displaying the title */
int decoration_theme_t::get_font_height_px() const
{
    char *font = g_settings_get_string (gs, "font-name");

    PangoFontDescription *font_desc = pango_font_description_from_string (font);
    int font_height = pango_font_description_get_size (font_desc);
    g_free (font);
    if (!pango_font_description_get_size_is_absolute (font_desc))
    {
        font_height *= 4;
        font_height /= 3;
    }
    return font_height / PANGO_SCALE;
}

int decoration_theme_t::get_title_height() const
{
    int height = get_font_height_px ();
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
    wf::geometry_t rectangle, const wf::geometry_t& scissor, bool active) const
{
    wf::color_t color = active ? fg : bg;
    OpenGL::render_begin(fb);
    fb.logic_scissor(scissor);
    OpenGL::render_rectangle(rectangle, color, fb.get_orthographic_projection());
    OpenGL::render_end();
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
    char *font = g_settings_get_string (gs, "font-name");
    int w, h;

    // render text
    font_desc = pango_font_description_from_string (font);

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), text.size());
    cairo_set_source_rgba(cr, active ? fg_text.r : bg_text.r, active ? fg_text.g : bg_text.g, active ? fg_text.b : bg_text.b, 1);
    pango_layout_get_pixel_size (layout, &w, &h);
    cairo_translate (cr, (t_width - w) / 2, (height - h) / 2);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font_desc);
    g_object_unref(layout);
    cairo_destroy(cr);
    g_free (font);

    return surface;
}

cairo_surface_t *decoration_theme_t::get_button_surface(button_type_t button,
    const button_state_t& state, bool active) const
{
    cairo_surface_t *cspng;
    const char *icon_name;
    char *iconfile, *theme;

    switch (button)
    {
        case BUTTON_CLOSE :             icon_name = "close";
                                        break;
        case BUTTON_TOGGLE_MAXIMIZE :   icon_name = "maximize";
                                        break;
        case BUTTON_MINIMIZE :          icon_name = "minimize";
                                        break;
    }

    theme = g_settings_get_string (gs, "gtk-theme");
    iconfile = g_strdup_printf ("/usr/share/themes/Greybird-dark/gtk-3.0/assets/titlebutton-%s%s-dark.png", icon_name, state.hover ? "-hover" : "");
    g_free (theme);

    // read the icon into a surface
    cspng = cairo_image_surface_create_from_png (iconfile);
    g_free (iconfile);

    return cspng;
}
}
}
