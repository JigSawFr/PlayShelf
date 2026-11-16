// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/link_flow.hpp"

#include <aurore.hpp>
#include <string>
#include <string_view>

#include "action/download_flow.hpp"
#include "app.hpp"
#include "bl_relay_nx.h"
#include "core/install.h"
#include "util/config.hpp"
#include "link/link_state.hpp"

namespace link_flow
{

namespace
{
constexpr int LOOK_EVERY = 15;   // seconds between two looks for the agent
constexpr int BUILD_EVERY = 5;   // seconds between two builds of the document

bool        s_started = false;
bool        s_open = false;        // a session with the agent
bool        s_held = false;        // an install, an update or a stop of the agent goes on: no session
int         s_idle = LOOK_EVERY;   // seconds since the last look (the first turn looks)
int         s_since_build = BUILD_EVERY;
std::string s_sent;                // the document the agent has

void gone_if_closed()
{
    if (s_open && !bl_relay_connected()) {
        s_open = false;
        s_sent.clear();
        s_idle = 0;
        aurore::Logger::info("link: the agent went away");
    }
}

void look_for_agent()
{
    if (s_held || !config::get().link_publish || !bl_relay_available()) return;
    BlRelayHelloReply reply;
    uint32_t          rc = 0;
    if (!bl_relay_open(link_state::MODULE, app::version().c_str(), &reply, &rc) || !reply.accepted) return;
    s_open = true;
    s_sent.clear();             // the agent has nothing of ours yet
    s_since_build = BUILD_EVERY;
    aurore::Logger::info("link: publishing through the agent");
}

void send_state()
{
    u64        free_bytes = 0;
    const bool free_known = core_free_space(&free_bytes) == 0;
    const auto live = download_flow::live();
    const auto doc = link_state::state_json(
        link_state::summarize(download_flow::jobs(), live.done, live.total, free_known, free_bytes, app::version()));
    if (doc.empty() || doc == s_sent) return;
    if (bl_relay_publish("state", doc.data(), doc.size(), true) == 0) s_sent = doc;
    gone_if_closed();
}

void take_orders()
{
    for (int n = 0; n < 4 && s_open; n++) {
        BlRelayInOrder in;
        bool           got = false;
        if (bl_relay_pop_order(&in, &got) != 0 || !got) break;
        const std::string sub = in.sub;
        const std::string_view payload(static_cast<const char*>(in.payload), in.payload_len);
        if (link_state::parse_order(sub, payload) == link_state::Order::PauseDownloads) {
            download_flow::pause_all();
            s_since_build = BUILD_EVERY;   // say so soon
            aurore::Logger::info("link: downloads paused from Home Assistant");
        }
        // Taken: the retained order is cleared on the broker, or a reconnection would bring it back.
        if (in.retained && bl_relay_publish(sub.c_str(), "", 0, true) != 0) break;
    }
    gone_if_closed();
}

void tick()
{
    if (s_open && (s_held || !config::get().link_publish)) {   // switched off in Preferences, or held: leave the agent
        bl_relay_close();
        s_open = false;
        s_sent.clear();
        s_idle = 0;
        if (s_held) aurore::Logger::info("link: leaving the agent alone");
        else aurore::Logger::info("link: not publishing any more");
    }
    if (!s_open) {
        if (++s_idle >= LOOK_EVERY) {
            s_idle = 0;
            look_for_agent();
        }
    } else {
        take_orders();
        if (s_open && ++s_since_build >= BUILD_EVERY) {
            s_since_build = 0;
            send_state();
        }
    }
    aurore::delay(1000, []() { tick(); });
}
}   // namespace

void hold(bool on)
{
    s_held = on;
    if (on && s_open) {   // at once: the agent is about to leave
        bl_relay_close();
        s_open = false;
        s_sent.clear();
    }
    if (!on) s_idle = LOOK_EVERY;   // look for the agent at the next turn
}

void start()
{
    if (s_started) return;
    s_started = true;
    aurore::delay(1000, []() { tick(); });
}

}   // namespace link_flow
