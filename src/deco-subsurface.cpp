#include "wayfire/geometry.hpp"
#include "wayfire/scene-input.hpp"
#include "wayfire/scene-operations.hpp"
#include "wayfire/scene-render.hpp"
#include "wayfire/scene.hpp"
#include "wayfire/signal-provider.hpp"
#include "wayfire/toplevel.hpp"
#include <memory>
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include <linux/input-event-codes.h>

#include <wayfire/nonstd/wlroots.hpp>
#include <wayfire/output.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/core.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include "deco-subsurface.hpp"
#include "deco-layout.hpp"
#include "deco-theme.hpp"
#include <wayfire/window-manager.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/txn/transaction-manager.hpp>
#include <wayfire/scene-render.hpp>

#include <wayfire/plugins/common/cairo-util.hpp>

#include <cairo.h>
#include "shade.hpp"


namespace wf
{
namespace pixdecor
{
wf::option_wrapper_t<wf::color_t> effect_color{"pixdecor/effect_color"};
wf::option_wrapper_t<int> shadow_radius{"pixdecor/shadow_radius"};
wf::option_wrapper_t<bool> titlebar_opt{"pixdecor/titlebar"};
wf::option_wrapper_t<int> csd_titlebar_height{"pixdecor/csd_titlebar_height"};
wf::option_wrapper_t<bool> enable_shade{"pixdecor/enable_shade"};
wf::option_wrapper_t<std::string> title_font{"pixdecor/title_font"};
wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
wf::option_wrapper_t<bool> maximized_borders{"pixdecor/maximized_borders"};
wf::option_wrapper_t<bool> maximized_shadows{"pixdecor/maximized_shadows"};
wf::option_wrapper_t<int> title_text_align{"pixdecor/title_text_align"};

class simple_decoration_node_t : public wf::scene::node_t, public wf::pointer_interaction_t,
    public wf::touch_interaction_t
{
    std::weak_ptr<wf::toplevel_view_interface_t> _view;
    wf::signal::connection_t<wf::view_title_changed_signal> title_set =
        [=] (wf::view_title_changed_signal *ev)
    {
        if (auto view = _view.lock())
        {
            view->damage();
        }
    };

    void update_title(int width, int height, int t_width, int border, int buttons_width, double scale)
    {
        if (auto view = _view.lock())
        {
            int target_width  = width * scale;
            int target_height = height * scale;

            if ((int(title_text_align) != title_texture.title_text_align) ||
                (view->get_title() != title_texture.current_text) ||
                (target_width != title_texture.tex.get_size().width) ||
                (std::string(title_font) != title_texture.title_font_string) ||
                (target_height != title_texture.tex.get_size().height) ||
                (view->activated != title_texture.rendered_for_activated_state))
            {
                auto surface = theme.render_text(view->get_title(),
                    target_width, target_height, t_width, border, buttons_width, view->activated);
                title_texture.tex = owned_texture_t{surface};
                cairo_surface_destroy(surface);
                title_texture.title_font_string = title_font;
                title_texture.current_text     = view->get_title();
                title_texture.title_text_align = int(title_text_align);
                title_texture.rendered_for_activated_state = view->activated;
            }
        }
    }

    struct
    {
        wf::owned_texture_t tex;
        std::string current_text = "";
        bool rendered_for_activated_state = false;
        int title_text_align = int(title_text_align);
        std::string title_font_string = title_font;
    } title_texture;

  public:
    pixdecor_theme_t theme;
    pixdecor_layout_t layout;
    wf::region_t cached_region;

    wf::dimensions_t size;

    int current_thickness;
    int current_titlebar;
    wf::pointf_t current_cursor_position;

    simple_decoration_node_t(wayfire_toplevel_view view) :
        node_t(false),
        theme{},
        layout{theme, [=] (wlr_box box) { wf::scene::damage_node(shared_from_this(), box + get_offset()); }}
    {
        this->_view = view->weak_from_this();
        view->connect(&title_set);

        // make sure to hide frame if the view is fullscreen
        update_decoration_size();

        current_cursor_position.x = current_cursor_position.y = FLT_MIN;
    }

    ~simple_decoration_node_t()
    {
        remove_shade_transformers();
    }

    wf::point_t get_offset()
    {
        auto view = _view.lock();
        if (view && view->pending_tiled_edges() && !maximized_borders && !maximized_shadows)
        {
            return {0, -current_titlebar};
        }

        return {-current_thickness, -current_titlebar};
    }

    void render_title(const wf::scene::render_instruction_t& data,
        const wf::geometry_t& geometry, int t_width, int border, int buttons_width)
    {
        update_title(geometry.width, geometry.height, t_width, border, buttons_width, data.target.scale);
        OpenGL::render_texture(wf::gles_texture_t{title_texture.tex.get_texture()}, data.target, geometry,
            glm::vec4(1.0f), OpenGL::RENDER_FLAG_CACHED);

        data.pass->custom_gles_subpass(data.target,[&]
        {
            for (auto& box : data.damage)
            {
                wf::gles::render_target_logic_scissor(data.target, wlr_box_from_pixman_box(box));
                OpenGL::draw_cached();
            }
        });

        OpenGL::clear_cached();
    }

    void render_region(const wf::scene::render_instruction_t& data, wf::point_t origin)
    {
        int border = theme.get_border_size();
        wlr_box geometry{origin.x, origin.y, size.width, size.height};

        bool activated = false;
        bool maximized = false;
        if (auto view = _view.lock())
        {
            activated = view->activated;
            maximized = view->pending_tiled_edges();
        }

        auto renderables = layout.get_renderable_areas();
        auto offset =
            wf::point_t{origin.x,
            origin.y -
            ((maximized && (!maximized_shadows || !maximized_borders)) ? -border / 2 : border / 4)};

        wf::gles::run_in_context([&]
        {
            wf::gles::bind_render_buffer(data.target);

            theme.render_background(data, geometry, activated, current_cursor_position);

            if (!titlebar_opt)
            {
                return;
            }

            int buttons_width = 0;
            for (auto item : renderables)
            {
                if (item->get_type() != DECORATION_AREA_TITLE)
                {
                    buttons_width += item->get_geometry().width;
                }
            }

            /* Draw title & buttons */
            auto title_border = border + ((std::string(overlay_engine) == "rounded_corners" &&
                (!maximized || maximized_shadows)) ? int(shadow_radius) * 2 : 0);
            for (auto item : renderables)
            {
                if (item->get_type() == DECORATION_AREA_TITLE)
                {
                    render_title(data,
                        item->get_geometry() + offset, size.width - border * 2, title_border, buttons_width);
                } else // button
                {
                    item->as_button().render(data,
                        item->get_geometry() + origin);
                }
            }
        });
    }

    std::optional<wf::scene::input_node_t> find_node_at(const wf::pointf_t& at) override
    {
        bool maximized = false;
        if (auto view = _view.lock())
        {
            maximized = view->pending_tiled_edges();
        }

        int border = theme.get_border_size();
        int r =
            (std::string(overlay_engine) == "rounded_corners" &&
                (!maximized || (maximized && maximized_shadows))) ? int(shadow_radius) * 2 : 0;
        r -= MIN_RESIZE_HANDLE_SIZE - std::min(border, MIN_RESIZE_HANDLE_SIZE);
        wf::pointf_t local = at - wf::pointf_t{get_offset()};
        if (auto view = _view.lock())
        {
            wf::geometry_t g = view->get_geometry();
            g.x = g.y = 0;
            g   = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{-r, -r, -r, -r});
            wf::region_t deco_region{g};
            g = wf::expand_geometry_by_margins(g, wf::decoration_margins_t{-border, -border, -border,
                -theme.get_title_height() - border});
            wf::region_t view_region{g};
            deco_region ^= view_region;
            if (deco_region.contains_pointf(local))
            {
                return wf::scene::input_node_t{
                    .node = this,
                    .local_coords = local,
                };
            }
        }

        return {};
    }

    pointer_interaction_t& pointer_interaction() override
    {
        return *this;
    }

    touch_interaction_t& touch_interaction() override
    {
        return *this;
    }

    class decoration_render_instance_t : public wf::scene::render_instance_t
    {
        simple_decoration_node_t *self;
        wf::scene::damage_callback push_damage;

        wf::signal::connection_t<wf::scene::node_damage_signal> on_surface_damage =
            [=] (wf::scene::node_damage_signal *data)
        {
            push_damage(data->region);
        };

      public:
        decoration_render_instance_t(simple_decoration_node_t *self, wf::scene::damage_callback push_damage)
        {
            this->self = self;
            this->push_damage = push_damage;
            self->connect(&on_surface_damage);
        }

        void schedule_instructions(std::vector<wf::scene::render_instruction_t>& instructions,
            const wf::render_target_t& target, wf::region_t& damage) override
        {
            auto our_region = self->cached_region + self->get_offset();
            wf::region_t our_damage = damage & our_region;
            if (!our_damage.empty())
            {
                instructions.push_back(wf::scene::render_instruction_t{
                            .instance = this,
                            .target   = target,
                            .damage   = std::move(our_damage),
                        });
            }
        }

        void render(const wf::scene::render_instruction_t& data) override
        {
            auto offset = self->get_offset();
            wlr_box rectangle{offset.x, offset.y, self->size.width, self->size.height};
            bool activated = false;
            bool maximized = false;
            if (auto view = self->_view.lock())
            {
                activated = view->activated;
                maximized = maximized_shadows ? false : view->pending_tiled_edges();
            }

            if ((std::string(effect_type) != "none") || (std::string(overlay_engine) != "none"))
            {
                self->theme.smoke.step_effect(data, rectangle, std::string(effect_type) == "ink",
                    self->current_cursor_position, self->theme.get_decor_color(activated), effect_color,
                    self->theme.get_title_height(), self->theme.get_border_size(),
                    (std::string(overlay_engine) == "rounded_corners" && !maximized) ? shadow_radius : 0);
            }

            self->render_region(data, offset);
        }
    };

    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
        wf::scene::damage_callback push_damage, wf::output_t *output = nullptr) override
    {
        instances.push_back(std::make_unique<decoration_render_instance_t>(this, push_damage));
    }

    wf::geometry_t get_bounding_box() override
    {
        return wf::construct_box(get_offset(), size);
    }

    /* wf::compositor_surface_t implementation */
    void handle_pointer_enter(wf::pointf_t point) override
    {
        point -= wf::pointf_t{get_offset()};
        layout.handle_motion(point.x, point.y);
    }

    void handle_pointer_leave() override
    {
        layout.handle_focus_lost();
        current_cursor_position.x = current_cursor_position.y = FLT_MIN;
    }

    void handle_pointer_motion(wf::pointf_t to, uint32_t) override
    {
        to -= wf::pointf_t{get_offset()};
        handle_action(layout.handle_motion(to.x, to.y));
        current_cursor_position = to;
    }

    void handle_pointer_button(const wlr_pointer_button_event& ev) override
    {
        if (ev.button != BTN_LEFT)
        {
            return;
        }

        handle_action(layout.handle_press_event(ev.state == WL_POINTER_BUTTON_STATE_PRESSED));
    }

    void handle_pointer_axis(const wlr_pointer_axis_event& ev) override
    {
        if (ev.orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
        {
            handle_action(layout.handle_axis_event(ev.delta));
        }
    }

    void pop_transformer(wayfire_view view)
    {
        if (view->get_transformed_node()->get_transformer(shade_transformer_name))
        {
            view->get_transformed_node()->rem_transformer(shade_transformer_name);
        }
    }

    void remove_shade_transformers()
    {
        for (auto& view : wf::get_core().get_all_views())
        {
            pop_transformer(view);
        }
    }

    std::shared_ptr<pixdecor_shade> ensure_transformer(wayfire_view view, int titlebar_height)
    {
        auto tmgr = view->get_transformed_node();
        if (auto tr = tmgr->get_transformer<pixdecor_shade>(shade_transformer_name))
        {
            return tr;
        }

        auto node = std::make_shared<pixdecor_shade>(view, titlebar_height);
        tmgr->add_transformer(node, wf::TRANSFORMER_2D, shade_transformer_name);
        auto tr = tmgr->get_transformer<pixdecor_shade>(shade_transformer_name);

        return tr;
    }

    void init_shade(wayfire_view view, bool shade, int titlebar_height)
    {
        if (!bool(enable_shade))
        {
            return;
        }

        if (shade)
        {
            if (view && view->is_mapped())
            {
                auto tr = ensure_transformer(view, titlebar_height);
                tr->set_titlebar_height(titlebar_height);
                tr->init_animation(shade);
            }
        } else
        {
            if (auto tr =
                    view->get_transformed_node()->get_transformer<pixdecor_shade>(
                        shade_transformer_name))
            {
                tr->set_titlebar_height(titlebar_height);
                tr->init_animation(shade);
            }
        }
    }

    void handle_action(pixdecor_layout_t::action_response_t action)
    {
        if (auto view = _view.lock())
        {
            switch (action.action)
            {
              case DECORATION_ACTION_MOVE:
                return wf::get_core().default_wm->move_request(view);

              case DECORATION_ACTION_RESIZE:
                return wf::get_core().default_wm->resize_request(view, action.edges);

              case DECORATION_ACTION_CLOSE:
                return view->close();

              case DECORATION_ACTION_TOGGLE_MAXIMIZE:
                if (view->pending_tiled_edges())
                {
                    return wf::get_core().default_wm->tile_request(view, 0);
                } else
                {
                    return wf::get_core().default_wm->tile_request(view, wf::TILED_EDGES_ALL);
                }

                break;

              case DECORATION_ACTION_SHADE:
                init_shade(view, true, current_titlebar);
                break;

              case DECORATION_ACTION_UNSHADE:
                init_shade(view, false, current_titlebar);
                break;

              case DECORATION_ACTION_MINIMIZE:
                return wf::get_core().default_wm->minimize_request(view, true);
                break;

              default:
                break;
            }
        }
    }

    void handle_touch_down(uint32_t time_ms, int finger_id, wf::pointf_t position) override
    {
        handle_touch_motion(time_ms, finger_id, position);
        handle_action(layout.handle_press_event());
    }

    void handle_touch_up(uint32_t time_ms, int finger_id, wf::pointf_t lift_off_position) override
    {
        handle_action(layout.handle_press_event(false));
        layout.handle_focus_lost();
    }

    void handle_touch_motion(uint32_t time_ms, int finger_id, wf::pointf_t position) override
    {
        position -= wf::pointf_t{get_offset()};
        layout.handle_motion(position.x, position.y);
        current_cursor_position = position;
    }

    void recreate_frame()
    {
        update_decoration_size();
        if (auto view = _view.lock())
        {
            auto size = wf::dimensions(view->get_pending_geometry());
            layout.resize(size.width, size.height);
            wf::get_core().tx_manager->schedule_object(view->toplevel());
        }
    }

    void resize(wf::dimensions_t dims)
    {
        if (auto view = _view.lock())
        {
            theme.set_maximize(view->pending_tiled_edges());
            layout.set_maximize(maximized_shadows ? 0 : view->pending_tiled_edges());
            view->damage();
            size = dims;
            layout.resize(size.width, size.height);
            if (!view->toplevel()->current().fullscreen)
            {
                this->cached_region = layout.calculate_region();
            }

            view->damage();
        }
    }

    void update_decoration_size()
    {
        if (auto view = _view.lock())
        {
            view->damage();
            bool fullscreen = view->toplevel()->pending().fullscreen;
            bool maximized  = view->toplevel()->pending().tiled_edges;
            if (fullscreen)
            {
                current_thickness = 0;
                current_titlebar  = 0;
                this->cached_region.clear();
            } else
            {
                int shadow_thickness = std::string(overlay_engine) == "rounded_corners" &&
                    (!maximized || (maximized && maximized_shadows)) ? int(shadow_radius) * 2 : 0;

                current_thickness = theme.get_border_size() + shadow_thickness;
                current_titlebar  = theme.get_title_height() +
                    ((maximized && !titlebar_opt && !maximized_borders &&
                        !maximized_shadows) ? 0 : current_thickness);
                this->cached_region = layout.calculate_region();
            }

            if (auto tr =
                    view->get_transformed_node()->get_transformer<pixdecor_shade>(
                        shade_transformer_name))
            {
                tr->set_titlebar_height(current_titlebar);
            }

            view->damage();
        }
    }
};

simple_decorator_t::simple_decorator_t(wayfire_toplevel_view view)
{
    this->view = view;
    this->shadow_thickness = 0;
    deco = std::make_shared<simple_decoration_node_t>(view);
    deco->resize(wf::dimensions(view->get_pending_geometry()));
    wf::scene::add_back(view->get_surface_root_node(), deco);

    view->connect(&on_view_activated);
    view->connect(&on_view_geometry_changed);
    view->connect(&on_view_fullscreen);
    view->connect(&on_view_tiled);

    on_view_activated = [this] (auto)
    {
        wf::scene::damage_node(deco, deco->get_bounding_box());
    };

    on_view_geometry_changed = [this] (auto)
    {
        deco->resize(wf::dimensions(this->view->get_geometry()));
        wf::get_core().tx_manager->schedule_object(this->view->toplevel());
    };

    on_view_tiled = [this] (auto)
    {
        deco->resize(wf::dimensions(this->view->get_geometry()));
        wf::get_core().tx_manager->schedule_object(this->view->toplevel());
    };

    on_view_fullscreen = [this] (auto)
    {
        if (!this->view->toplevel()->pending().fullscreen)
        {
            deco->resize(wf::dimensions(this->view->get_geometry()));
            wf::get_core().tx_manager->schedule_object(this->view->toplevel());
        }
    };
}

simple_decorator_t::~simple_decorator_t()
{
    wf::scene::remove_child(deco);
    deco.reset();
}

int simple_decorator_t::get_titlebar_height()
{
    return deco->current_titlebar;
}

void simple_decorator_t::recreate_frame()
{
    deco->recreate_frame();
}

void simple_decorator_t::update_decoration_size()
{
    deco->update_decoration_size();
}

void simple_decorator_t::update_colors()
{
    deco->theme.update_colors();
}

void simple_decorator_t::effect_updated()
{
    deco->theme.smoke.effect_updated();
}

wf::decoration_margins_t simple_decorator_t::get_margins(const wf::toplevel_state_t& state)
{
    if (state.fullscreen)
    {
        return {0, 0, 0, 0};
    }

    deco->theme.set_maximize(state.tiled_edges);

    this->shadow_thickness = std::string(overlay_engine) == "rounded_corners" &&
        (!state.tiled_edges || (state.tiled_edges && maximized_shadows)) ? int(shadow_radius) * 2 : 0;

    int thickness = deco->theme.get_border_size() + this->shadow_thickness;
    int titlebar  = deco->theme.get_title_height() +
        ((state.tiled_edges && !titlebar_opt && !maximized_borders && !maximized_shadows) ? 0 : thickness);
    if (state.tiled_edges && !maximized_borders)
    {
        if (maximized_shadows)
        {
            if (!titlebar_opt)
            {
                titlebar = thickness;
            }
        } else
        {
            thickness = 0;
        }
    }

    double shade_progress = 0.0;
    if (auto tr =
            view->get_transformed_node()->get_transformer<pixdecor_shade>(
                shade_transformer_name))
    {
        tr->set_titlebar_height(titlebar);
        shade_progress = tr->progression.shade;
    }

    if (view->has_data(custom_data_name))
    {
        view->get_data<wf_shadow_margin_t>(custom_data_name)->set_margins(
            {shadow_thickness, shadow_thickness, shadow_thickness,
                shadow_thickness +
                int((view->get_geometry().height - shadow_thickness - titlebar) * shade_progress)});
    } else
    {
        view->store_data(std::make_unique<wf_shadow_margin_t>(), custom_data_name);
        view->get_data<wf_shadow_margin_t>(custom_data_name)->set_margins(
            {shadow_thickness, shadow_thickness, shadow_thickness,
                shadow_thickness +
                int((view->get_geometry().height - shadow_thickness - titlebar) * shade_progress)});
    }

    return wf::decoration_margins_t{
        .left   = thickness,
        .right  = thickness,
        .bottom = thickness,
        .top    = titlebar,
    };
}

void simple_decorator_t::update_animation()
{
    auto margins = get_margins(view->toplevel()->current());
    auto bbox    = deco->get_bounding_box();

    wf::region_t region;
    region |= wlr_box{bbox.x, bbox.y, bbox.width, margins.top};
    region |= wlr_box{bbox.x, bbox.y, margins.left, bbox.height};
    region |= wlr_box{bbox.x, bbox.y + bbox.height - margins.bottom, bbox.width, margins.bottom};
    region |= wlr_box{bbox.x + bbox.width - margins.right, bbox.y, margins.right, bbox.height};
    wf::scene::damage_node(deco, region);
}
}
}
