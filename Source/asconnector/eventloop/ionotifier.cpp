//
//  ionotifier.cpp
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#include "ionotifier.h"
#include "ionotifier_p.h"
#include "eventloop_p.h"
#include "sky/log.h"


using namespace sky;


IONotifier::IONotifier(IONotifierPrivate *priv)
    : m_private(priv)
{
}

IONotifier::~IONotifier()
{
    delete m_private;
}

void IONotifier::setEnabled(bool enable)
{
    m_private->setEnabled(enable);
}

bool IONotifier::isEnabled() const
{
    return m_private->isEnabled();
}

int IONotifier::fd() const
{
    return m_private->fd();
}

void IONotifier::setEvents(unsigned events)
{
    m_private->setEvents(events);
}

unsigned IONotifier::getEvents() const
{
    return m_private->getEvents();
}



IONotifierPrivate::IONotifierPrivate(int fd, unsigned events,
                                     std::function<void(unsigned)> &&func)
    : m_callback(std::make_shared<std::function<void(unsigned)>>(std::move(func)))
    , m_source(nullptr)
    , m_fd(fd)
    , m_events(events)
{
}

IONotifierPrivate::~IONotifierPrivate()
{
    if (m_source)
    {
        EventLoopPrivate::assertCorrectThread(m_source);

        sd_event_source_set_enabled(m_source, SD_EVENT_OFF);
        sd_event_source_unref(m_source);
        m_source = nullptr;
    }
}

void IONotifierPrivate::setSource(sd_event_source *src)
{
    m_source = src;
}

int IONotifierPrivate::handler(sd_event_source *source, int fd, uint32_t revents, void *userData)
{
    int enabled = SD_EVENT_OFF;
    if (sd_event_source_get_enabled(source, &enabled) > 0)
    {
        LOG_WARNING("odd, event disabled or not valid in callback");
        return -1;
    }

    IONotifierPrivate *self = reinterpret_cast<IONotifierPrivate*>(userData);

    // debugging sanity check
    if ((self->m_source != source) || (self->m_fd != fd))
    {
        LOG_ERROR("odd, source pointers or descriptors don't match ?");
        return 0;
    }

    // call the callback
    if (self->m_callback)
    {
        auto cb = self->m_callback;
        if (cb->operator bool())
            cb->operator()(convertFromEPollEvents(revents));
    }

    return 0;
}

void IONotifierPrivate::setEnabled(bool enable)
{
    EventLoopPrivate::assertCorrectThread(m_source);

    int rc = sd_event_source_set_enabled(m_source, enable ? SD_EVENT_ON : SD_EVENT_OFF);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to enable io listener");
    }
}

bool IONotifierPrivate::isEnabled() const
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

int IONotifierPrivate::fd() const
{
    return m_fd;
}

unsigned IONotifierPrivate::convertToEPollEvents(unsigned events)
{
    unsigned result = 0;
    if (events & IONotifier::ReadableEvent)
        result |= EPOLLIN;
    if (events & IONotifier::WritableEvent)
        result |= EPOLLOUT;
    if (events & IONotifier::ErrorEvent)
        result |= EPOLLERR;
    if (events & IONotifier::UpdateEvent)
        result |= EPOLLHUP;

    return result;
}

unsigned IONotifierPrivate::convertFromEPollEvents(const unsigned events)
{
    unsigned result = 0;
    if (events & EPOLLIN)
        result |= IONotifier::ReadableEvent;
    if (events & EPOLLOUT)
        result |= IONotifier::WritableEvent;
    if (events & EPOLLERR)
        result |= IONotifier::ErrorEvent;
    if (events & EPOLLHUP)
        result |= IONotifier::UpdateEvent;

    return result;
}

void IONotifierPrivate::setEvents(unsigned events)
{
    EventLoopPrivate::assertCorrectThread(m_source);

    m_events = events;

    int rc = sd_event_source_set_io_events(m_source, convertToEPollEvents(events));
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to set io events mask");
    }

}

unsigned IONotifierPrivate::getEvents() const
{
    return m_events;
}
