//
//  signalnotifier.cpp
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#include "signalnotifier.h"
#include "signalnotifier_p.h"
#include "eventloop_p.h"
#include "sky/log.h"

using namespace sky;


SignalNotifier::SignalNotifier(SignalNotifierPrivate *priv)
    : m_private(priv)
{
}

SignalNotifier::~SignalNotifier()
{
    delete m_private;
}

void SignalNotifier::setEnabled(bool enable)
{
    m_private->setEnabled(enable);
}

bool SignalNotifier::isEnabled() const
{
    return m_private->isEnabled();
}

int SignalNotifier::signal() const
{
    return m_private->signal();
}


SignalNotifierPrivate::SignalNotifierPrivate(int signum, std::function<void()> &&func)
    : m_callback(std::make_shared<std::function<void()>>(std::move(func)))
    , m_source(nullptr)
    , m_signalNumber(signum)
{
}

SignalNotifierPrivate::~SignalNotifierPrivate()
{
    if (m_source)
    {
        EventLoopPrivate::assertCorrectThread(m_source);

        sd_event_source_set_enabled(m_source, SD_EVENT_OFF);
        sd_event_source_unref(m_source);
        m_source = nullptr;
    }
}

void SignalNotifierPrivate::setSource(sd_event_source *src)
{
    m_source = src;
}

int SignalNotifierPrivate::handler(sd_event_source *source, const struct signalfd_siginfo *si, void *userData)
{
    int enabled = SD_EVENT_OFF;
    if (sd_event_source_get_enabled(source, &enabled) > 0)
    {
        LOG_WARNING("odd, event disabled or not valid in callback");
        return -1;
    }

    SignalNotifierPrivate *self = reinterpret_cast<SignalNotifierPrivate*>(userData);

    // debugging sanity check
    if ((self->m_source != source) || (self->m_signalNumber != (int)si->ssi_signo))
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

void SignalNotifierPrivate::setEnabled(bool enable)
{
    EventLoopPrivate::assertCorrectThread(m_source);

    int rc = sd_event_source_set_enabled(m_source, enable ? SD_EVENT_ON : SD_EVENT_OFF);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to enable io listener");
    }
}

bool SignalNotifierPrivate::isEnabled() const
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

int SignalNotifierPrivate::signal() const
{
    return m_signalNumber;
}
