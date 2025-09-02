#include "deco-layout.hpp"
#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/nonstd/reverse.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/toplevel.hpp>
#include <wayfire/util.hpp>

namespace wf
{
namespace pixdecor
{
/**
 * Represents an area of the decoration which reacts to input events.
 */
decoration_area_t::decoration_area_t(decoration_area_type_t type, wf::geometry_t g)
{
    this->type     = type;
    this->geometry = g;

    assert(type != DECORATION_AREA_BUTTON);
}

/**
 * Initialize a new decoration area holding a button
 */
decoration_area_t::decoration_area_t(wf::geometry_t g,
    std::function<void(wlr_box)> damage_callback,
    pixdecor_theme_t& theme)
{
    this->type     = DECORATION_AREA_BUTTON;
    this->geometry = g;
    this->damage_callback = damage_callback;

    this->button = std::make_unique<button_t>(theme,
        std::bind(damage_callback, g));
}

wf::geometry_t decoration_area_t::get_geometry() const
{
    return geometry;
}

void decoration_area_t::set_geometry(wf::geometry_t g)
{
    geometry = g;

    if (this->type == DECORATION_AREA_BUTTON)
    {
        this->button = std::make_unique<button_t>(this->button->theme,
            std::bind(this->damage_callback, g));
    }
}

button_t& decoration_area_t::as_button()
{
    assert(button);

    return *button;
}

decoration_area_type_t decoration_area_t::get_type() const
{
    return type;
}

pixdecor_layout_t::pixdecor_layout_t(pixdecor_theme_t& th,
    std::function<void(wlr_box)> callback) :

    titlebar_size(th.get_title_height()),
    border_size(th.get_input_size()),
    theme(th),
    damage_callback(callback)
{}

pixdecor_layout_t::~pixdecor_layout_t()
{
    this->layout_areas.clear();
}

wf::geometry_t pixdecor_layout_t::create_left_buttons(int width, int radius)
{
    // read the string from settings; start at the beginning and replace commas with spaces
    wf::option_wrapper_t<int> button_spacing{"pixdecor/left_button_spacing"};
    wf::option_wrapper_t<int> button_x_offset{"pixdecor/left_button_x_offset"};
    wf::option_wrapper_t<int> button_y_offset{"pixdecor/button_y_offset"};
    GSettings *settings = g_settings_new("org.gnome.desktop.wm.preferences");
    gchar *b_layout     = g_settings_get_string(settings, "button-layout");
    gchar *ptr = b_layout;
    int len    = 0;
    while (ptr <= b_layout + strlen(b_layout) - 1)
    {
        if (*ptr == ',')
        {
            *ptr = ' ';
        }

        if (*ptr == ':')
        {
            break;
        }

        ptr++;
        len++;
    }

    std::string layout = std::string(b_layout).substr(0, len);
    g_free(b_layout);
    g_object_unref(settings);

    std::stringstream stream((std::string)layout);
    std::vector<button_type_t> buttons;
    std::string button_name;
    while (stream >> button_name)
    {
        if (button_name == "minimize")
        {
            buttons.push_back(BUTTON_MINIMIZE);
        }

        if (button_name == "maximize")
        {
            buttons.push_back(BUTTON_TOGGLE_MAXIMIZE);
        }

        if (button_name == "close")
        {
            buttons.push_back(BUTTON_CLOSE);
        }
    }

    int total_width = 0;
    int per_button  = 0;
    int border = theme.get_border_size();
    wf::geometry_t button_geometry;
    button_geometry.x = radius * 2 + (maximized ? 4 : border) + button_x_offset;

    for (auto type : buttons)
    {
        auto button_area  = std::make_unique<decoration_area_t>(button_geometry, damage_callback, theme);
        auto surface_size = button_area->as_button().set_button_type(type);
        button_geometry.width  = surface_size.width;
        button_geometry.height = surface_size.height;
        int button_padding = (theme.get_title_height() - button_geometry.height) / 2 + button_y_offset;
        button_geometry.y = button_padding + border / 2 + (radius * 2);
        per_button = button_geometry.width + (buttons.back() == type ? 0 : button_spacing);
        button_area->set_geometry(button_geometry);
        button_area->as_button().set_button_type(type);
        this->layout_areas.push_back(std::move(button_area));
        button_geometry.x += per_button;
        total_width += per_button;
    }

    return {
        buttons.empty() ? 0 : (maximized ? 4 : border), maximized ? 4 : border + (radius * 2),
        total_width, theme.get_title_height()
    };
}

wf::geometry_t pixdecor_layout_t::create_right_buttons(int width, int radius)
{
    // read the string from settings; start at the colon and replace commas with spaces
    wf::option_wrapper_t<int> button_spacing{"pixdecor/right_button_spacing"};
    wf::option_wrapper_t<int> button_x_offset{"pixdecor/right_button_x_offset"};
    wf::option_wrapper_t<int> button_y_offset{"pixdecor/button_y_offset"};
    GSettings *settings = g_settings_new("org.gnome.desktop.wm.preferences");
    gchar *b_layout     = g_settings_get_string(settings, "button-layout");
    gchar *ptr = b_layout + strlen(b_layout) - 1;
    while (ptr >= b_layout)
    {
        if (*ptr == ',')
        {
            *ptr = ' ';
        }

        if (*ptr == ':')
        {
            break;
        }

        ptr--;
    }

    std::string layout = (*ptr == ':') ? ptr + 1 : std::string(b_layout);
    g_free(b_layout);
    g_object_unref(settings);

    std::stringstream stream((std::string)layout);
    std::vector<button_type_t> buttons;
    std::string button_name;
    while (stream >> button_name)
    {
        if (button_name == "minimize")
        {
            buttons.push_back(BUTTON_MINIMIZE);
        }

        if (button_name == "maximize")
        {
            buttons.push_back(BUTTON_TOGGLE_MAXIMIZE);
        }

        if (button_name == "close")
        {
            buttons.push_back(BUTTON_CLOSE);
        }
    }

    int total_width = 0;
    int per_button  = 0;
    int border = theme.get_border_size();
    wf::geometry_t button_geometry;
    button_geometry.x = (width - (maximized ? 4 : border)) + button_x_offset;

    for (auto type : wf::reverse(buttons))
    {
        auto button_area  = std::make_unique<decoration_area_t>(button_geometry, damage_callback, theme);
        auto surface_size = button_area->as_button().set_button_type(type);
        button_geometry.width  = surface_size.width;
        button_geometry.height = surface_size.height;
        int button_padding = (theme.get_title_height() - button_geometry.height) / 2 + button_y_offset;
        button_geometry.y = button_padding + border / 2 + (radius * 2);
        per_button = button_geometry.width + (buttons.back() == type ? 0 : button_spacing);
        button_geometry.x -= per_button;
        button_area->set_geometry(button_geometry);
        button_area->as_button().set_button_type(type);
        this->layout_areas.push_back(std::move(button_area));
        total_width += per_button;
    }

    total_width -= button_x_offset;

    return {
        button_geometry.x, maximized ? 4 : border + (radius * 2),
        total_width, theme.get_title_height()
    };
}

/** Regenerate layout using the new size */
void pixdecor_layout_t::resize(int width, int height)
{
    wf::option_wrapper_t<int> shadow_radius{"pixdecor/shadow_radius"};
    wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
    wf::option_wrapper_t<bool> maximized_borders{"pixdecor/maximized_borders"};
    bool rounded_corners = std::string(overlay_engine) == "rounded_corners";

    int border = theme.get_border_size();
    int radius = (rounded_corners && !maximized) ? int(shadow_radius) : 0;

    this->layout_areas.clear();

    if (this->theme.get_title_height() > 0)
    {
        auto button_left_geometry_expanded  = create_left_buttons((radius * 2), radius);
        auto button_right_geometry_expanded = create_right_buttons(width - (radius * 2), radius);

        /* Padding around the buttons, allows move */
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_MOVE, button_left_geometry_expanded));
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_MOVE, button_right_geometry_expanded));

        /* Titlebar dragging area (for move) */
        wf::geometry_t title_geometry = {
            border + button_left_geometry_expanded.x,
            maximized ? 0 : border / 2 + (radius * 2),
            /* Up to the button, but subtract the padding to the left of the
             * title and the padding between title and button */
            std::max(1, button_right_geometry_expanded.x - border),
            theme.get_title_height() + (maximized ? 0 : border / 2 + 1),
        };
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_TITLE, title_geometry));

        this->cached_titlebar = {
            border, border,
            width - 2 * border, height - 2 * border
        };
    }

    border = MIN_RESIZE_HANDLE_SIZE - theme.get_input_size();
    auto inverse_border = MIN_RESIZE_HANDLE_SIZE - theme.get_border_size();

    if (!maximized || maximized_borders)
    {
        /* Resizing edges - top */
        wf::geometry_t border_geometry =
        {0 + (radius * 2), -inverse_border + (radius * 2),
            width - (radius * 4) + MIN_RESIZE_HANDLE_SIZE, border};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_TOP, border_geometry));

        /* Resizing edges - bottom */
        border_geometry =
        {0 + (radius * 2), (height - border + inverse_border) - (radius * 2),
            width - (radius * 4) + MIN_RESIZE_HANDLE_SIZE, border};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_BOTTOM, border_geometry));

        /* Resizing edges - left */
        border_geometry =
        {-inverse_border + (radius * 2), 0 + (radius * 2), border,
            height - (radius * 4) + MIN_RESIZE_HANDLE_SIZE};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_LEFT, border_geometry));

        /* Resizing edges - right */
        border_geometry =
        {(width - border + inverse_border) - (radius * 2), 0 + (radius * 2), border,
            height - (radius * 4) + MIN_RESIZE_HANDLE_SIZE};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_RIGHT, border_geometry));

        if (rounded_corners)
        {
            /* Shadow - top */
            border_geometry = {0, 0, width, radius* 2};
            this->layout_areas.push_back(std::make_unique<decoration_area_t>(
                DECORATION_AREA_SHADOW, border_geometry));

            /* Shadow - bottom */
            border_geometry = {0, height - radius * 2, width, radius* 2};
            this->layout_areas.push_back(std::make_unique<decoration_area_t>(
                DECORATION_AREA_SHADOW, border_geometry));

            /* Shadow - left */
            border_geometry = {0, radius* 2, radius* 2, height - radius * 4};
            this->layout_areas.push_back(std::make_unique<decoration_area_t>(
                DECORATION_AREA_SHADOW, border_geometry));

            /* Shadow - right */
            border_geometry = {width - radius * 2, radius* 2, radius* 2, height - radius * 4};
            this->layout_areas.push_back(std::make_unique<decoration_area_t>(
                DECORATION_AREA_SHADOW, border_geometry));
        }
    }
}

/**
 * @return The decoration areas which need to be rendered, in top to bottom
 *  order.
 */
std::vector<nonstd::observer_ptr<decoration_area_t>> pixdecor_layout_t::get_renderable_areas()
{
    std::vector<nonstd::observer_ptr<decoration_area_t>> renderable;
    for (auto& area : layout_areas)
    {
        if (area->get_type() & DECORATION_AREA_RENDERABLE_BIT)
        {
            renderable.push_back({area});
        }
    }

    return renderable;
}

wf::region_t pixdecor_layout_t::calculate_region() const
{
    wf::region_t r{};
    for (auto& area : layout_areas)
    {
        auto g = area->get_geometry();
        auto b = theme.get_input_size();
        if (maximized && (area->get_type() & DECORATION_AREA_MOVE_BIT))
        {
            g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{b, b, b, b});
        }

        if (area->get_type() & DECORATION_AREA_RESIZE_BIT)
        {
            if (b <= MIN_RESIZE_HANDLE_SIZE)
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{b, b, b, b});
            } else if ((area->get_type() == DECORATION_AREA_RESIZE_TOP) ||
                       (area->get_type() == DECORATION_AREA_RESIZE_BOTTOM))
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{0, 0, b, b});
            } else if ((area->get_type() == DECORATION_AREA_RESIZE_LEFT) ||
                       (area->get_type() == DECORATION_AREA_RESIZE_RIGHT))
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{b, b, 0, 0});
            }
        }

        if ((g.width > 0) && (g.height > 0))
        {
            r |= g;
        }
    }

    return r;
}

wf::region_t pixdecor_layout_t::limit_region(wf::region_t & region) const
{
    wf::region_t out = region & this->cached_titlebar;
    return out;
}

void pixdecor_layout_t::unset_hover(wf::point_t position)
{
    auto area = find_area_at(position);
    if (area && (area->get_type() == DECORATION_AREA_BUTTON))
    {
        area->as_button().set_hover(false);
    }
}

/** Handle motion event to (x, y) relative to the decoration */
pixdecor_layout_t::action_response_t pixdecor_layout_t::handle_motion(
    int x, int y)
{
    auto previous_area = find_area_at(current_input);
    auto current_area  = find_area_at({x, y});

    if ((previous_area == current_area) && is_grabbed && current_area &&
        (current_area->get_type() & DECORATION_AREA_MOVE_BIT))
    {
        is_grabbed = false;
        return {DECORATION_ACTION_MOVE, 0};
    } else
    {
        unset_hover(current_input);
        if (current_area && (current_area->get_type() == DECORATION_AREA_BUTTON))
        {
            current_area->as_button().set_hover(true);
        }
    }

    this->current_input = {x, y};
    update_cursor();

    return {DECORATION_ACTION_NONE, 0};
}

/**
 * Handle press or release event.
 * @param pressed Whether the event is a press(true) or release(false)
 *  event.
 * @return The action which needs to be carried out in response to this
 *  event.
 * */
pixdecor_layout_t::action_response_t pixdecor_layout_t::handle_press_event(
    bool pressed)
{
    if (pressed)
    {
        auto area = find_area_at(current_input);
        if (area && (area->get_type() & DECORATION_AREA_MOVE_BIT))
        {
            if (timer.is_connected())
            {
                double_click_at_release = true;
            } else
            {
                timer.set_timeout(300, [] () { return false; });
            }
        }

        if (area && (area->get_type() & DECORATION_AREA_RESIZE_BIT))
        {
            return {DECORATION_ACTION_RESIZE, calculate_resize_edges()};
        }

        if (area && (area->get_type() == DECORATION_AREA_BUTTON))
        {
            area->as_button().set_pressed(true);
        }

        is_grabbed  = true;
        grab_origin = current_input;
    }

    if (!pressed && double_click_at_release)
    {
        double_click_at_release = false;
        return {DECORATION_ACTION_TOGGLE_MAXIMIZE, 0};
    } else if (!pressed && is_grabbed)
    {
        is_grabbed = false;
        auto begin_area = find_area_at(grab_origin);
        auto end_area   = find_area_at(current_input);

        if (begin_area && (begin_area->get_type() == DECORATION_AREA_BUTTON))
        {
            begin_area->as_button().set_pressed(false);
            if (end_area && (begin_area == end_area))
            {
                switch (begin_area->as_button().get_button_type())
                {
                  case BUTTON_CLOSE:
                    return {DECORATION_ACTION_CLOSE, 0};

                  case BUTTON_TOGGLE_MAXIMIZE:
                    return {DECORATION_ACTION_TOGGLE_MAXIMIZE, 0};

                  case BUTTON_MINIMIZE:
                    return {DECORATION_ACTION_MINIMIZE, 0};

                  default:
                    break;
                }
            }
        }
    }

    return {DECORATION_ACTION_NONE, 0};
}

pixdecor_layout_t::action_response_t pixdecor_layout_t::handle_axis_event(
    int delta)
{
    if (delta < 0)
    {
        return {DECORATION_ACTION_SHADE, 0};
    } else
    {
        return {DECORATION_ACTION_UNSHADE, 0};
    }
}

/**
 * Find the layout area at the given coordinates, if any
 * @return The layout area or null on failure
 */
nonstd::observer_ptr<decoration_area_t> pixdecor_layout_t::find_area_at(
    wf::point_t point)
{
    for (auto& area : this->layout_areas)
    {
        auto g = area->get_geometry();
        auto b = theme.get_input_size();
        if (area->get_type() & DECORATION_AREA_MOVE_BIT)
        {
            continue;
        }

        if (area->get_type() & DECORATION_AREA_RESIZE_BIT)
        {
            if (b <= MIN_RESIZE_HANDLE_SIZE)
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{b, b, b, b});
            } else if ((area->get_type() == DECORATION_AREA_RESIZE_TOP) ||
                       (area->get_type() == DECORATION_AREA_RESIZE_BOTTOM))
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{0, 0, b, b});
            } else if ((area->get_type() == DECORATION_AREA_RESIZE_LEFT) ||
                       (area->get_type() == DECORATION_AREA_RESIZE_RIGHT))
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{b, b, 0, 0});
            }
        }

        if (maximized && (area->get_type() & DECORATION_AREA_MOVE_BIT))
        {
            g.height += b;
        }

        if (g & point)
        {
            return {area};
        }
    }

    for (auto& area : this->layout_areas)
    {
        auto g = area->get_geometry();
        auto b = theme.get_input_size();
        if (area->get_type() & DECORATION_AREA_RESIZE_BIT)
        {
            continue;
        }

        if (maximized && (area->get_type() & DECORATION_AREA_MOVE_BIT))
        {
            g.height += b;
        }

        if (g & point)
        {
            return {area};
        }
    }

    return nullptr;
}

/** Calculate resize edges based on @current_input */
uint32_t pixdecor_layout_t::calculate_resize_edges() const
{
    wf::option_wrapper_t<int> shadow_radius{"pixdecor/shadow_radius"};
    wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
    int radius     = (std::string(overlay_engine) == "rounded_corners") ? int(shadow_radius) : 0;
    uint32_t edges = 0;
    for (auto& area : layout_areas)
    {
        auto g = area->get_geometry();
        auto b = theme.get_input_size();
        g.width  = g.width ?: 1;
        g.height = g.height ?: 1;
        if (area->get_type() & DECORATION_AREA_RESIZE_BIT)
        {
            if (b <= MIN_RESIZE_HANDLE_SIZE)
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{b, b, b, b});
            } else if ((area->get_type() == DECORATION_AREA_RESIZE_TOP) ||
                       (area->get_type() == DECORATION_AREA_RESIZE_BOTTOM))
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{0, 0, b, b});
            } else if ((area->get_type() == DECORATION_AREA_RESIZE_LEFT) ||
                       (area->get_type() == DECORATION_AREA_RESIZE_RIGHT))
            {
                g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{b, b, 0, 0});
            }
        }

        if (((b - radius * 2) > MIN_RESIZE_HANDLE_SIZE) && (area->get_type() == DECORATION_AREA_RESIZE_TOP))
        {
            g.height /= 2;
        }

        if (g & this->current_input)
        {
            if (area->get_type() & DECORATION_AREA_RESIZE_BIT)
            {
                edges |= (area->get_type() & ~DECORATION_AREA_RESIZE_BIT);
            }
        }
    }

    return edges;
}

/** Update the cursor based on @current_input */
void pixdecor_layout_t::update_cursor()
{
    uint32_t edges = calculate_resize_edges();
    auto area = find_area_at(this->current_input);
    if (area && (area->get_type() == DECORATION_AREA_BUTTON))
    {
        wf::get_core().set_cursor("default");
        return;
    }

    auto cursor_name = edges > 0 ?
        wlr_xcursor_get_resize_name((wlr_edges)edges) : "default";
    wf::get_core().set_cursor(cursor_name);
}

void pixdecor_layout_t::set_maximize(bool state)
{
    maximized = state;
}

void pixdecor_layout_t::handle_focus_lost()
{
    if (is_grabbed)
    {
        this->is_grabbed = false;
        auto area = find_area_at(grab_origin);
        if (area && (area->get_type() == DECORATION_AREA_BUTTON))
        {
            area->as_button().set_pressed(false);
        }
    }

    this->unset_hover(current_input);
}
}
}
