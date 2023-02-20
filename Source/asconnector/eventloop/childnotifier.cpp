//
//  childnotifier.cpp
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#include "childnotifier.h"
#include "childnotifier_p.h"
#include "eventloop_p.h"
#include "sky/log.h"


using namespace sky;


ChildNotifier::ChildNotifier(ChildNotifierPrivate *priv)
    : m_private(priv)
{
}

ChildNotifier::~ChildNotifier()
{
    delete m_private;
}

void ChildNotifier::setEnabled(bool enable)
{
    m_private->setEnabled(enable);
}

bool ChildNotifier::isEnabled() const
{
    return m_private->isEnabled();
}

pid_t ChildNotifier::pid() const
{
    return m_private->pid();
}


ChildNotifierPrivate::ChildNotifierPrivate(pid_t pid, std::function<void()> &&func)
    : m_callback(std::make_shared<std::function<void()>>(std::move(func)))
    , m_source(nullptr)
    , m_pid(pid)
{
}

ChildNotifierPrivate::~ChildNotifierPrivate()
{
    if (m_source)
    {
        EventLoopPrivate::assertCorrectThread(m_source);

        sd_event_source_set_enabled(m_source, SD_EVENT_OFF);
        sd_event_source_unref(m_source);
        m_source = nullptr;
    }
}

void ChildNotifierPrivate::setSource(sd_event_source *src)
{
    m_source = src;
}

int ChildNotifierPrivate::handler(sd_event_source *source, const siginfo_t *si, void *userData)
{
    int enabled = SD_EVENT_OFF;
    if (sd_event_source_get_enabled(source, &enabled) > 0)
    {
        LOG_WARNING("odd, event disabled or not valid in callback");
        return -1;
    }

    ChildNotifierPrivate *self = reinterpret_cast<ChildNotifierPrivate*>(userData);

    // debugging sanity check
    if (self->m_source != source)
    {
        LOG_ERROR("odd, source pointers or signal numbers don't match ?");
        return 0;
    }

    // call the callback
    if (self->m_callback)
    {
        auto cb = self->m_callback;
        if (cb->operator bool())
            cb->operator()();
    }

    return 0;
}

void ChildNotifierPrivate::setEnabled(bool enable)
{
    EventLoopPrivate::assertCorrectThread(m_source);

    int rc = sd_event_source_set_enabled(m_source, enable ? SD_EVENT_ON : SD_EVENT_OFF);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to enable io listener");
    }
}

bool ChildNotifierPrivate::isEnabled() const
{
    EventLoopPrivate::assertCorrectThread(m_source);

    int enabled = SD_EVENT_OFF;
    int rc = sd_event_source_get_enabled(m_source, &enabled);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to disable io listener");
    }

    return (enabled == SD_EVENT_ON);
}

pid_t ChildNotifierPrivate::pid() const
{
    return m_pid;
}
