// Listeners — the callbacks told when a store changes. listen() returns an id
// for unlisten(); notify() calls a copy, so a callback may unlisten itself or
// listen again. Not thread-safe: the owners call it on the UI thread.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <map>
#include <utility>

class Listeners
{
  public:
    int listen(std::function<void()> changed)
    {
        const int id = m_next++;
        m_listeners[id] = std::move(changed);
        return id;
    }

    void unlisten(int id) { m_listeners.erase(id); }

    void notify() const
    {
        const auto listeners = m_listeners;
        for (const auto& [id, changed] : listeners) changed();
    }

  private:
    std::map<int, std::function<void()>> m_listeners;
    int                                  m_next = 1;
};
