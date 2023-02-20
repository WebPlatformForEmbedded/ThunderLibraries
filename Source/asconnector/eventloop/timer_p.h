//
//  timer_p.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef SKY_TIMER_P_H
#define SKY_TIMER_P_H

#include "timer.h"

#include <systemd/sd-event.h>

#include <memory>
#include <cinttypes>
#include <functional>


namespace sky
{

    class TimerPrivate
    {
    public:
        TimerPrivate(std::function<void()> &&func);
        ~TimerPrivate();

        void setSource(sd_event_source *src);

    public:
        void start();
        void start(std::chrono::milliseconds &&value);
        void stop();

        std::chrono::milliseconds interval() const;
        void setInterval(std::chrono::milliseconds &&value);

        bool isSingleShot() const;
        void setSingleShot(bool singleShot);

    public:
        static int handler(sd_event_source *es, uint64_t usec, void *userData);

    private:
        const std::shared_ptr<std::function<void()>> m_callback;
        sd_event_source *m_source;
        bool m_oneShot;
        std::chrono::microseconds m_interval;
    };

} // namespace sky

#endif // SKY_TIMER_P_H
