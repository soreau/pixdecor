#pragma once

#include <wayfire/output.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/core.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/window-manager.hpp>
#include <wayfire/txn/transaction-manager.hpp>
#include <wayfire/scene-render.hpp>
#include <wayfire/util/duration.hpp>

#include "deco-subsurface.hpp"

const std::string shade_transformer_name = "pixdecor_shade";

namespace wf
{
namespace pixdecor
{
using namespace wf::scene;
using namespace wf::animation;
class shade_animation_t : public duration_t
{
  public:
    using duration_t::duration_t;
    timed_transition_t shade{*this};
};
class pixdecor_shade : public wf::scene::view_2d_transformer_t
{
    nonstd::observer_ptr<simple_decorator_t> deco = nullptr;
    wayfire_view view;
    wf::output_t *output;
    int titlebar_height;
    wf::option_wrapper_t<wf::animation_description_t> shade_duration{"pixdecor/shade_duration"};

  public:
    bool last_direction;
    shade_animation_t progression{shade_duration};
    class simple_node_render_instance_t : public wf::scene::transformer_render_instance_t<transformer_base_node_t>
    {
        wf::signal::connection_t<node_damage_signal> on_node_damaged =
            [=] (node_damage_signal *ev)
        {
            push_to_parent(ev->region);
        };

        pixdecor_shade *self;
        wayfire_view view;
        damage_callback push_to_parent;

      public:
        simple_node_render_instance_t(pixdecor_shade *self, damage_callback push_damage,
            wayfire_view view) : wf::scene::transformer_render_instance_t<transformer_base_node_t>(self,
                push_damage,
                view->get_output())
        {
            this->self = self;
            this->view = view;
            this->push_to_parent = push_damage;
            self->connect(&on_node_damaged);
        }

        ~simple_node_render_instance_t()
        {}

        void schedule_instructions(
            std::vector<render_instruction_t>& instructions,
            const wf::render_target_t& target, wf::region_t& damage)
        {
            // We want to render ourselves only, the node does not have children
            instructions.push_back(render_instruction_t{
                        .instance = this,
                        .target   = target,
                        .damage   = damage & self->get_bounding_box(),
                    });
        }

        void render(const wf::scene::render_instruction_t& data)
        {
            auto src_box = self->get_children_bounding_box();
            gl_geometry src_geometry = {(float)src_box.x, (float)src_box.y,
                (float)src_box.x + src_box.width, (float)src_box.y + src_box.height};
            auto src_tex = wf::scene::transformer_render_instance_t<transformer_base_node_t>::get_texture(
                1.0);
            auto shade_region = data.damage;
            int height    = src_box.height;
            auto titlebar = self->titlebar_height + self->get_shadow_margin();
            src_box.y += self->titlebar_height;
            src_box.height *= 1.0 -
                (self->progression.shade *
                    ((src_box.height - titlebar) / float(src_box.height)));
            auto progress_height = src_box.height;
            shade_region &= wf::region_t{src_box};
            wf::gles::run_in_context([&]
            {
                wf::gles::bind_render_buffer(data.target);
                data.pass->custom_gles_subpass(data.target, [&]
                {
                    for (const auto& box : shade_region)
                    {
                        wf::gles::render_target_logic_scissor(data.target, wlr_box_from_pixman_box(box));
                        OpenGL::render_transformed_texture(wf::gles_texture_t{src_tex},
                        {src_geometry.x1, src_geometry.y1 - (height - progress_height), src_geometry.x2,
                            src_geometry.y2 - (height - progress_height)}, {},
                            wf::gles::render_target_orthographic_projection(data.target), glm::vec4(1.0), 0);
                    }
                });

                shade_region = data.damage;
                src_box = self->get_children_bounding_box();
                src_box.height = self->titlebar_height;
                shade_region  &= wf::region_t{src_box};
                data.pass->custom_gles_subpass(data.target, [&]
                {
                    for (const auto& box : shade_region)
                    {
                        wf::gles::render_target_logic_scissor(data.target, wlr_box_from_pixman_box(box));
                        OpenGL::render_transformed_texture(wf::gles_texture_t{src_tex}, src_geometry, {},
                            wf::gles::render_target_orthographic_projection(data.target), glm::vec4(1.0), 0);
                    }
                });
            });
        }
    };

    pixdecor_shade(wayfire_view view, int titlebar_height) : wf::scene::view_2d_transformer_t(view)
    {
        this->view   = view;
        this->output = view->get_output();
        this->titlebar_height = titlebar_height;
        this->progression.shade.set(0.0, 1.0);
        if (output)
        {
            output->render->add_effect(&pre_hook, wf::OUTPUT_EFFECT_PRE);
        }

        if (auto toplevel = wf::toplevel_cast(view))
        {
            this->deco = toplevel->toplevel()->get_data<simple_decorator_t>();
        }
    }

    void pop_transformer(wayfire_view view)
    {
        if (view->get_transformed_node()->get_transformer(shade_transformer_name))
        {
            view->get_transformed_node()->rem_transformer(shade_transformer_name);
        }

        if (!deco && view->has_data(custom_data_name))
        {
            view->erase_data(custom_data_name);
        }
    }

    wf::effect_hook_t pre_hook = [=] ()
    {
        if (auto toplevel = wf::toplevel_cast(view))
        {
            if (deco)
            {
                /* SSD */
                deco->get_margins(toplevel->toplevel()->pending());
            } else
            {
                /* CSD */
                auto bg = view->get_surface_root_node()->get_bounding_box();
                auto vg = toplevel->get_geometry();
                auto margins =
                    wf::decoration_margins_t{vg.x - bg.x, vg.y - bg.y,
                    bg.width - ((vg.x - bg.x) + vg.width), bg.height - ((vg.y - bg.y) + vg.height)};
                if (view->has_data(custom_data_name))
                {
                    view->get_data<wf_shadow_margin_t>(custom_data_name)->set_margins(
                        {0, 0, 0,
                            int(((toplevel->get_geometry().height + margins.bottom) - titlebar_height) *
                                progression.shade)});
                } else
                {
                    view->store_data(std::make_unique<wf_shadow_margin_t>(), custom_data_name);
                    view->get_data<wf_shadow_margin_t>(custom_data_name)->set_margins(
                        {0, 0, 0,
                            int(((toplevel->get_geometry().height + margins.bottom) - titlebar_height) *
                                progression.shade)});
                }
            }
        }

        view->damage();
        if (!this->progression.running() && !last_direction)
        {
            if (output)
            {
                output->render->rem_effect(&pre_hook);
            }

            pop_transformer(view);
        }
    };

    void gen_render_instances(std::vector<render_instance_uptr>& instances,
        damage_callback push_damage, wf::output_t *shown_on) override
    {
        instances.push_back(std::make_unique<simple_node_render_instance_t>(
            this, push_damage, view));
    }

    std::optional<wf::scene::input_node_t> find_node_at(const wf::pointf_t& at) override
    {
        auto bbox = this->get_children_bounding_box();
        if (((at.y - bbox.y) <
             ((1.0 -
               (this->progression.shade * ((bbox.height - this->titlebar_height) / float(bbox.height)))) *
              bbox.height)) &&
            ((at.y - bbox.y) > 0.0))
        {
            return floating_inner_node_t::find_node_at(at);
        }

        return {};
    }

    void init_animation(bool shade)
    {
        if (shade == last_direction)
        {
            return;
        }

        if (this->progression.running())
        {
            this->progression.reverse();
        } else
        {
            if ((shade && !this->progression.get_direction()) ||
                (!shade && this->progression.get_direction()))
            {
                this->progression.reverse();
            }

            this->progression.start();
        }

        last_direction = shade;
    }

    void set_titlebar_height(int titlebar_height)
    {
        this->titlebar_height = titlebar_height;
    }

    int get_shadow_margin()
    {
        if (deco)
        {
            return deco->shadow_thickness;
        } else
        {
            if (auto toplevel = wf::toplevel_cast(view))
            {
                auto bg = view->get_surface_root_node()->get_bounding_box();
                auto vg = toplevel->get_geometry();
                return bg.height - ((vg.y - bg.y) + vg.height);
            }
        }

        return 0;
    }

    bool get_direction()
    {
        return this->last_direction;
    }

    virtual ~pixdecor_shade()
    {
        if (output)
        {
            output->render->rem_effect(&pre_hook);
        }
    }
};
}
}
