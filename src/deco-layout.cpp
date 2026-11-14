#include "deco-layout.hpp"
#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel.hpp>
#include <wayfire/util.hpp>
#include <wayfire/seat.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>

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
    std::function<void(wf::geometry_t)> damage_callback,
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
    std::function<void(wf::geometry_t)> callback) :

    titlebar_size(th.get_title_height()),
    border_size(th.get_input_size()),
    theme(th),
    damage_callback(callback)
{}

pixdecor_layout_t::~pixdecor_layout_t()
{
    this->layout_areas.clear();
}

// A layout item without a button action is the application icon.
static std::vector<std::optional<button_type_t>> parse_layout_items(std::string layout)
{
    std::replace(layout.begin(), layout.end(), ',', ' ');
    std::stringstream stream(layout);
    std::vector<std::optional<button_type_t>> items;
    std::string name;
    while (stream >> name)
    {
        if (name == "minimize")
        {
            items.push_back(BUTTON_MINIMIZE);
        } else if (name == "maximize")
        {
            items.push_back(BUTTON_TOGGLE_MAXIMIZE);
        } else if (name == "close")
        {
            items.push_back(BUTTON_CLOSE);
        } else if (name == "icon")
        {
            items.push_back(std::nullopt);
        }
    }

    return items;
}

wf::geometry_t pixdecor_layout_t::create_layout_items(
    const std::string& layout, double edge, double radius, bool on_right)
{
    wf::option_wrapper_t<int> spacing{on_right ?
        "pixdecor/right_button_spacing" : "pixdecor/left_button_spacing"};
    wf::option_wrapper_t<int> x_offset{on_right ?
        "pixdecor/right_button_x_offset" : "pixdecor/left_button_x_offset"};
    wf::option_wrapper_t<int> y_offset{"pixdecor/button_y_offset"};
    auto items = parse_layout_items(layout);
    if (on_right)
    {
        std::reverse(items.begin(), items.end());
    }

    const double border = theme.get_border_size();
    const double inset = maximized ? 4 : border;
    const int title_height = theme.get_title_height();
    double x = edge + (on_right ? -inset : inset) + int(x_offset);
    double total_width = 0;
    for (size_t i = 0; i < items.size(); ++i)
    {
        auto type = items[i];
        wf::geometry_t geometry{x, 0, 0, 0};
        std::unique_ptr<decoration_area_t> area;
        if (type)
        {
            area = std::make_unique<decoration_area_t>(geometry, damage_callback, theme);
            auto dims = area->as_button().set_button_type(*type);
            geometry.width = dims.width;
            geometry.height = dims.height;
        } else
        {
            geometry.width = geometry.height = std::min(20, std::max(1, title_height - 8));
        }

        geometry.y = (title_height - geometry.height) / 2 + int(y_offset) + border / 2 + radius * 2;
        const double gap = (on_right ? i > 0 : i + 1 < items.size()) ? int(spacing) : 0;
        const double item_width = geometry.width + gap;
        if (on_right)
        {
            x -= item_width;
            geometry.x = x;
        }

        if (type)
        {
            area->set_geometry(geometry);
            area->as_button().set_button_type(*type);
        } else
        {
            area = std::make_unique<decoration_area_t>(DECORATION_AREA_ICON, geometry);
        }

        layout_areas.push_back(std::move(area));
        if (!on_right)
        {
            x += item_width;
        }

        total_width += item_width;
    }

    return {
        on_right ? x : (items.empty() ? 0 : inset),
        maximized ? 4 : border + radius * 2,
        on_right ? total_width - int(x_offset) : total_width,
        double(title_height)
    };
}

/** Regenerate layout using the new size */
void pixdecor_layout_t::resize(int width, int height)
{
    wf::option_wrapper_t<int> shadow_radius{"pixdecor/shadow_radius"};
    wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
    wf::option_wrapper_t<bool> maximized_borders{"pixdecor/maximized_borders"};
    bool rounded_corners = std::string(overlay_engine) == "rounded_corners";

    double border = theme.get_border_size();
    double radius = (rounded_corners && !maximized) ? int(shadow_radius) : 0;

    this->layout_areas.clear();

    if (this->theme.get_title_height() > 0)
    {
        wf::option_wrapper_t<std::string> button_layout{"pixdecor/button_layout"};
        const std::string layout = button_layout;
        const auto first_colon = layout.find(':');
        const auto last_colon = layout.rfind(':');
        auto button_left_geometry_expanded = create_layout_items(
            layout.substr(0, first_colon), radius * 2, radius, false);
        const auto left_items_end = layout_areas.size();
        auto button_right_geometry_expanded = create_layout_items(
            last_colon == std::string::npos ? layout : layout.substr(last_colon + 1),
            width - radius * 2, radius, true);
        title_left_edge = 0;
        title_right_edge = width;
        for (size_t i = 0; i < layout_areas.size(); ++i)
        {
            const auto geometry = layout_areas[i]->get_geometry();
            if (i < left_items_end)
            {
                title_left_edge = std::max(title_left_edge, int(std::ceil(geometry.x + geometry.width)));
            } else
            {
                title_right_edge = std::min(title_right_edge, int(std::floor(geometry.x)));
            }
        }

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
            std::max<double>(1, button_right_geometry_expanded.x - border),
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
        double w = width;
        double h = height;

        /* Resizing edges - top */
        wf::geometry_t border_geometry =
        {0 + (radius * 2), -inverse_border + (radius * 2),
            w - (radius * 4) + MIN_RESIZE_HANDLE_SIZE, border};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_TOP, border_geometry));

        /* Resizing edges - bottom */
        border_geometry =
        {0 + (radius * 2), (h - border + inverse_border) - (radius * 2),
            w - (radius * 4) + MIN_RESIZE_HANDLE_SIZE, border};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_BOTTOM, border_geometry));

        /* Resizing edges - left */
        border_geometry =
        {-inverse_border + (radius * 2), 0 + (radius * 2), border,
            h - (radius * 4) + MIN_RESIZE_HANDLE_SIZE};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_LEFT, border_geometry));

        /* Resizing edges - right */
        border_geometry =
        {(w - border + inverse_border) - (radius * 2), 0 + (radius * 2), border,
            h - (radius * 4) + MIN_RESIZE_HANDLE_SIZE};
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_RESIZE_RIGHT, border_geometry));

        if (rounded_corners)
        {
            /* Shadow - top */
            border_geometry = {0, 0, w, radius* 2};
            this->layout_areas.push_back(std::make_unique<decoration_area_t>(
                DECORATION_AREA_SHADOW, border_geometry));

            /* Shadow - bottom */
            border_geometry = {0, h - radius * 2, w, radius* 2};
            this->layout_areas.push_back(std::make_unique<decoration_area_t>(
                DECORATION_AREA_SHADOW, border_geometry));

            /* Shadow - left */
            border_geometry = {0, radius* 2, radius* 2, h - radius * 4};
            this->layout_areas.push_back(std::make_unique<decoration_area_t>(
                DECORATION_AREA_SHADOW, border_geometry));

            /* Shadow - right */
            border_geometry = {w - radius * 2, radius* 2, radius* 2, h - radius * 4};
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

wf::regionf_t pixdecor_layout_t::calculate_region() const
{
    wf::regionf_t r{};
    for (auto& area : layout_areas)
    {
        auto g   = area->get_geometry();
        double b = theme.get_input_size();
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

wf::regionf_t pixdecor_layout_t::limit_region(wf::regionf_t & region) const
{
    wf::regionf_t out = region & this->cached_titlebar;
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
        wf::point_t position = {x, y};
        wf::point_t delta    = position - grab_origin;

        if (wf::abs(wf::pointf_t{(double)delta.x, (double)delta.y}) > 5)
        {
            is_grabbed = false;
            return {DECORATION_ACTION_MOVE, 0};
        }
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
pixdecor_layout_t::action_response_t pixdecor_layout_t::handle_press_event(uint32_t button,
    bool pressed)
{
    if (button == BTN_RIGHT)
    {
        if (!pressed)
        {
            wayfire_view active_view = wf::get_core().seat->get_active_view();
            if (active_view)
            {
                wf::view_show_window_menu_signal data;
                data.view = active_view;
                if (active_view->get_output())
                {
                    active_view->get_output()->emit(&data);
                }

                wf::get_core().emit(&data);
            }
        }

        return {DECORATION_ACTION_NONE, 0};
    }

    if (button != BTN_LEFT)
    {
        return {DECORATION_ACTION_NONE, 0};
    }

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
        is_grabbed = false;

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
        auto g   = area->get_geometry();
        double b = theme.get_input_size();
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
        auto g   = area->get_geometry();
        double b = theme.get_input_size();
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
