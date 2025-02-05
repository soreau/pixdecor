#ifndef DECO_SUBSURFACE_HPP
#define DECO_SUBSURFACE_HPP

#include "wayfire/object.hpp"
#include "wayfire/toplevel.hpp"
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>

static std::string custom_data_name = "wf-decoration-shadow-margin";

class wf_shadow_margin_t : public wf::custom_data_t
{
  public:
    wf::decoration_margins_t get_margins()
    {
        return margins;
    }

    void set_margins(wf::decoration_margins_t margins)
    {
        this->margins = margins;
    }

  private:
    wf::decoration_margins_t margins = {0, 0, 0, 0};
};

namespace wf
{
namespace pixdecor
{
class simple_decoration_node_t;
/**
 * A decorator object attached as custom data to a toplevel object.
 */
class simple_decorator_t : public wf::custom_data_t
{
    wayfire_toplevel_view view;
    std::shared_ptr<simple_decoration_node_t> deco;

    wf::signal::connection_t<wf::view_activated_state_signal> on_view_activated;
    wf::signal::connection_t<wf::view_geometry_changed_signal> on_view_geometry_changed;
    wf::signal::connection_t<wf::view_fullscreen_signal> on_view_fullscreen;
    wf::signal::connection_t<wf::view_tiled_signal> on_view_tiled;

  public:
    void update_colors();
    void effect_updated();
    void recreate_frame();
    void update_decoration_size();
    simple_decorator_t(wayfire_toplevel_view view);
    ~simple_decorator_t();
    wf::decoration_margins_t get_margins(const wf::toplevel_state_t& state);
    int get_titlebar_height();
    void update_animation();
    int shadow_thickness;
};
}
}

#endif /* end of include guard: DECO_SUBSURFACE_HPP */
