#include <wayfire/per-output-plugin.hpp>
#include <wayfire/view.hpp>
#include <wayfire/workarea.hpp>
#include <wayfire/matcher.hpp>
#include <wayfire/workspace-set.hpp>
#include <wayfire/output.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/txn/transaction-manager.hpp>
#include <wayfire/render-manager.hpp>

#include "deco-subsurface.hpp"
#include "wayfire/core.hpp"
#include "wayfire/plugin.hpp"
#include "wayfire/signal-provider.hpp"
#include "wayfire/toplevel-view.hpp"
#include "wayfire/toplevel.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <gio/gio.h>

int handle_theme_updated(int fd, uint32_t mask, void *data)
{
    int bufsz = sizeof(inotify_event) + NAME_MAX + 1;
    char buf[bufsz];

    if ((mask & WL_EVENT_READABLE) == 0)
    {
        return 0;
    }

    if (read(fd, buf, bufsz) < 0)
    {
        return 0;
    }

    (*((std::function<void(void)>*)data))();

    return 0;
}

class wayfire_pixdecor : public wf::plugin_interface_t
{
    wf::option_wrapper_t<int> border_size{"pixdecor/border_size"};
    wf::option_wrapper_t<wf::color_t> fg_color{"pixdecor/fg_color"};
    wf::option_wrapper_t<wf::color_t> bg_color{"pixdecor/bg_color"};
    wf::option_wrapper_t<wf::color_t> fg_text_color{"pixdecor/fg_text_color"};
    wf::option_wrapper_t<wf::color_t> bg_text_color{"pixdecor/bg_text_color"};
    wf::option_wrapper_t<std::string> ignore_views_string{"pixdecor/ignore_views"};
    wf::option_wrapper_t<std::string> always_decorate_string{"pixdecor/always_decorate"};
    wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
    wf::view_matcher_t ignore_views{"pixdecor/ignore_views"};
    wf::view_matcher_t always_decorate{"pixdecor/always_decorate"};
    wf::wl_idle_call idle_update_views;
    int inotify_fd;
    int wd_cfg_file;
    int wd_cfg_dir;
    wl_event_source *evsrc;
    std::function<void(void)> update_event;
    wf::effect_hook_t pre_hook;
    wf::output_t *output;
    bool hook_set = false;

    wf::signal::connection_t<wf::txn::new_transaction_signal> on_new_tx =
        [=] (wf::txn::new_transaction_signal *ev)
    {
        // For each transaction, we need to consider what happens with participating views
        for (const auto& obj : ev->tx->get_objects())
        {
            if (auto toplevel = std::dynamic_pointer_cast<wf::toplevel_t>(obj))
            {
                // First check whether the toplevel already has decoration
                // In that case, we should just set the correct margins
                if (auto deco = toplevel->get_data<wf::simple_decorator_t>())
                {
                    toplevel->pending().margins = deco->get_margins(toplevel->pending());
                    continue;
                }

                // Second case: the view is already mapped, or the transaction does not map it.
                // The view is not being decorated, so nothing to do here.
                if (toplevel->current().mapped || !toplevel->pending().mapped)
                {
                    continue;
                }

                // Third case: the transaction will map the toplevel.
                auto view = wf::find_view_for_toplevel(toplevel);
                wf::dassert(view != nullptr, "Mapping a toplevel means there must be a corresponding view!");
                if (should_decorate_view(view))
                {
                    adjust_new_decorations(view);
                }
            }
        }
    };

    wf::signal::connection_t<wf::view_decoration_state_updated_signal> on_decoration_state_changed =
        [=] (wf::view_decoration_state_updated_signal *ev)
    {
        update_view_decoration(ev->view);
    };

  public:

    void init() override
    {
        wf::get_core().connect(&on_decoration_state_changed);
        wf::get_core().tx_manager->connect(&on_new_tx);

        for (auto& view : wf::get_core().get_all_views())
        {
            update_view_decoration(view);
        }

        border_size.set_callback([=]
        {
            for (auto& view : wf::get_core().get_all_views())
            {
                auto toplevel = wf::toplevel_cast(view);
                if (!toplevel || !toplevel->toplevel()->get_data<wf::simple_decorator_t>())
                {
                    continue;
                }

                remove_decoration(toplevel);
                adjust_new_decorations(toplevel);
                wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
            }
        });

        fg_color.set_callback([=] { update_colors(); });
        bg_color.set_callback([=] { update_colors(); });
        fg_text_color.set_callback([=] { update_colors(); });
        bg_text_color.set_callback([=] { update_colors(); });
        ignore_views_string.set_callback([=]
        {
            idle_update_views.run_once([=] ()
            {
                for (auto& view : wf::get_core().get_all_views())
                {
                    auto toplevel = wf::toplevel_cast(view);
                    if (!toplevel)
                    {
                        continue;
                    }

                    update_view_decoration(view);
                }
            });
        });
        always_decorate_string.set_callback([=]
        {
            idle_update_views.run_once([=] ()
            {
                for (auto& view : wf::get_core().get_all_views())
                {
                    auto toplevel = wf::toplevel_cast(view);
                    if (!toplevel)
                    {
                        continue;
                    }

                    update_view_decoration(view);
                }
            });
        });

        pre_hook = [=] ()
        {
            for (auto& view : wf::get_core().get_all_views())
            {
                auto toplevel = wf::toplevel_cast(view);
                if (!toplevel || !toplevel->toplevel()->get_data<wf::simple_decorator_t>())
                {
                    continue;
                }

                toplevel->toplevel()->get_data<wf::simple_decorator_t>()->damage(view);
            }
        };

        if (std::string(effect_type) != "none")
        {
            for (auto& o : wf::get_core().output_layout->get_outputs())
            {
                o->render->add_effect(&pre_hook, wf::OUTPUT_EFFECT_PRE);
            }
        }

        effect_type.set_callback([=] ()
        {
            if (std::string(effect_type) == "none")
            {
                if (hook_set)
                {
                    for (auto& o : wf::get_core().output_layout->get_outputs())
                    {
                        o->render->rem_effect(&pre_hook);
                    }

                    hook_set = false;
                }
            } else
            {
                if (!hook_set)
                {
                    for (auto& o : wf::get_core().output_layout->get_outputs())
                    {
                        o->render->add_effect(&pre_hook, wf::OUTPUT_EFFECT_PRE);
                    }

                    hook_set = true;
                }
            }

            for (auto& view : wf::get_core().get_all_views())
            {
                auto toplevel = wf::toplevel_cast(view);
                if (!toplevel || !toplevel->toplevel()->get_data<wf::simple_decorator_t>())
                {
                    continue;
                }

                toplevel->toplevel()->get_data<wf::simple_decorator_t>()->damage(view);
                toplevel->toplevel()->get_data<wf::simple_decorator_t>()->effect_updated();
            }
        });

        // set up the watch on the xsettings file
        inotify_fd = inotify_init1(IN_CLOEXEC);
        evsrc = wl_event_loop_add_fd(wf::get_core().ev_loop, inotify_fd, WL_EVENT_READABLE,
            handle_theme_updated, &this->update_event);

        // enable watches on xsettings file
        char *conf_dir  = g_build_filename(g_get_user_config_dir(), "xsettingsd/", NULL);
        char *conf_file = g_build_filename(conf_dir, "xsettingsd.conf", NULL);
        wd_cfg_dir  = inotify_add_watch(inotify_fd, conf_dir, IN_CREATE);
        wd_cfg_file = inotify_add_watch(inotify_fd, conf_file, IN_CLOSE_WRITE);
        g_free(conf_file);
        g_free(conf_dir);

        update_event = [=] (void)
        {
            update_colors();
        };
    }

    void fini() override
    {
        for (auto view : wf::get_core().get_all_views())
        {
            if (auto toplevel = wf::toplevel_cast(view))
            {
                remove_decoration(toplevel);
                wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
            }
        }

        if (hook_set)
        {
            for (auto& o : wf::get_core().output_layout->get_outputs())
            {
                o->render->rem_effect(&pre_hook);
            }
        }

        wl_event_source_remove(evsrc);
        inotify_rm_watch(inotify_fd, wd_cfg_file);
        inotify_rm_watch(inotify_fd, wd_cfg_dir);
        close(inotify_fd);
    }

    void update_colors()
    {
        for (auto& view : wf::get_core().get_all_views())
        {
            auto toplevel = wf::toplevel_cast(view);
            if (!toplevel || !toplevel->toplevel()->get_data<wf::simple_decorator_t>())
            {
                continue;
            }

            auto deco = toplevel->toplevel()->get_data<wf::simple_decorator_t>();
            deco->update_colors();
            view->damage();
        }
    }

    /**
     * Uses view_matcher_t to match whether the given view needs to be
     * ignored for decoration
     *
     * @param view The view to match
     * @return Whether the given view should be decorated?
     */
    bool ignore_decoration_of_view(wayfire_view view)
    {
        return ignore_views.matches(view);
    }

    bool should_decorate_view(wayfire_toplevel_view view)
    {
        return (view->should_be_decorated() && !ignore_decoration_of_view(view)) || always_decorate.matches(
            view);
    }

    void adjust_new_decorations(wayfire_toplevel_view view)
    {
        auto toplevel = view->toplevel();

        if (!toplevel->get_data<wf::simple_decorator_t>())
        {
            toplevel->store_data(std::make_unique<wf::simple_decorator_t>(view));
        }

        auto deco     = toplevel->get_data<wf::simple_decorator_t>();
        auto& pending = toplevel->pending();
        pending.margins = deco->get_margins(pending);

        if (!pending.fullscreen && !pending.tiled_edges)
        {
            toplevel->pending().geometry = wf::expand_geometry_by_margins(
                toplevel->pending().geometry, pending.margins);
        }
    }

    void remove_decoration(wayfire_toplevel_view view)
    {
        view->toplevel()->erase_data<wf::simple_decorator_t>();
        auto& pending = view->toplevel()->pending();
        if (!pending.fullscreen && !pending.tiled_edges)
        {
            pending.geometry = wf::shrink_geometry_by_margins(pending.geometry, pending.margins);
        }

        pending.margins = {0, 0, 0, 0};
    }

    void update_view_decoration(wayfire_view view)
    {
        if (auto toplevel = wf::toplevel_cast(view))
        {
            if (!toplevel)
            {
                return;
            }

            if (!toplevel->toplevel()->get_data<wf::simple_decorator_t>() && (always_decorate.matches(view) ||
                                                                              (should_decorate_view(toplevel)
                                                                               &&
                                                                               !ignore_views.matches(view))))
            {
                adjust_new_decorations(toplevel);
            } else if ((!always_decorate.matches(view) &&
                        (!should_decorate_view(toplevel) || ignore_views.matches(view))))
            {
                remove_decoration(toplevel);
            }

            wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
        }
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_pixdecor);
