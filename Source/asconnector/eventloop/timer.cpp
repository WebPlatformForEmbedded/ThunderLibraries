//
//  timer.cpp
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#include "timer.h"
#include "timer_p.h"
#include "eventloop_p.h"
#include "sky/log.h"


using namespace sky;


Timer::Timer(TimerPrivate *priv)
    : m_private(priv)
{
}

Timer::~Timer()
{
    delete m_private;
}

void Timer::start()
{
    m_private->start();
}

void Timer::startImpl(std::chrono::milliseconds &&value)
{
    m_private->start(std::move(value));
}

void Timer::stop()
{
    m_private->stop();
}

std::chrono::milliseconds Timer::interval() const
{
    return m_private->interval();
}

void Timer::setIntervalImpl(std::chrono::milliseconds &&value)
{
    m_private->setInterval(std::move(value));
}

bool Timer::isSingleShot() const
{
    return m_private->isSingleShot();
}

void Timer::setSingleShot(bool singleShot)
{
    m_private->setSingleShot(singleShot);
}




TimerPrivate::TimerPrivate(std::function<void()> &&func)
    : m_callback(std::make_shared<std::function<void()>>(std::move(func)))
    , m_source(nullptr)
    , m_oneShot(true)
    , m_interval(0)
{
}

TimerPrivate::~TimerPrivate()
{
    if (m_source)
    {
        EventLoopPrivate::assertCorrectThread(m_source);

        sd_event_source_set_enabled(m_source, SD_EVENT_OFF);
        sd_event_source_unref(m_source);
        m_source = nullptr;
    }
}

void TimerPrivate::setSource(sd_event_source *src)
{
    m_source = src;
}

int TimerPrivate::handler(sd_event_source *source, uint64_t usec, void *userData)
{
    int rc;
    int enabled = SD_EVENT_OFF;
    if ((rc = sd_event_source_get_enabled(source, &enabled)) > 0)
    {
        LOG_SYS_WARNING(-rc, "odd, timer disabled or not valid in callback");
        return -1;
    }

    TimerPrivate *self = reinterpret_cast<TimerPrivate*>(userData);

    // debugging sanity check
    if (self->m_source != source)
    {
        LOG_ERROR("odd, source pointers don't match ?");
    }

    // if one shot ensure the timer is now disabled, otherwise reschedule timer
    if (self->m_oneShot)
    {
        if ((rc = sd_event_source_set_enabled(source, SD_EVENT_OFF)) < 0)
        {
            LOG_SYS_ERROR(-rc, "failed to reenable timer");
        }
    }
    else
    {
        if ((rc = sd_event_source_set_time(source, usec + self->m_interval.count())) < 0)
        {
            LOG_SYS_ERROR(-rc, "failed to reschedule timer");
        }
        else if ((rc = sd_event_source_set_enabled(source, SD_EVENT_ON)) < 0)
        {
            LOG_SYS_ERROR(-rc, "failed to re-enable timer");
        }
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

void TimerPrivate::start()
{
    EventLoopPrivate::assertCorrectThread(m_source);

    // calculate the expiry time
    uint64_t expiry;
    sd_event_now(sd_event_source_get_event(m_source), CLOCK_MONOTONIC, &expiry);
    expiry += m_interval.count();

    // set the new expiry
    int rc = sd_event_source_set_time(m_source, expiry);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to set timer time");
    }

    // enable the timer
    rc = sd_event_source_set_enabled(m_source,
                                     m_oneShot ? SD_EVENT_ONESHOT : SD_EVENT_ON);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to enable timer");
    }
}

void TimerPrivate::start(std::chrono::milliseconds &&value)
{
    stop();
    m_interval = std::chrono::duration_cast<std::chrono::microseconds>(value);
    start();
}

void TimerPrivate::stop()
{
    EventLoopPrivate::assertCorrectThread(m_source);

    int rc = sd_event_source_set_enabled(m_source, SD_EVENT_OFF);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to disable timer");
    }
}

std::chrono::milliseconds TimerPrivate::interval() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(m_interval);
}

void TimerPrivate::setInterval(std::chrono::milliseconds &&value)
{
    m_interval = std::chrono::duration_cast<std::chrono::microseconds>(value);
}

bool TimerPrivate::isSingleShot() const
{
    return m_oneShot;
}

void TimerPrivate::setSingleShot(bool singleShot)
{
    m_oneShot = singleShot;
}
