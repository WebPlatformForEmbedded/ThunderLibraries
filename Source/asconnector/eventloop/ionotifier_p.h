//
//  ionotifier_p.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef IONOTIFIER_P_H
#define IONOTIFIER_P_H

#include <systemd/sd-event.h>

#include <memory>
#include <functional>


namespace sky
{
    class IONotifierPrivate
    {
    public:
        IONotifierPrivate(int fd, unsigned events, std::function<void(unsigned)> &&func);
        ~IONotifierPrivate();

        void setSource(sd_event_source *src);

    public:
        void setEnabled(bool enable);
        bool isEnabled() const;

        int fd() const;

        void setEvents(unsigned events);
        unsigned getEvents() const;

    public:
        static int handler(sd_event_source *es, int fd, uint32_t revents, void *userData);

    private:
        static unsigned convertToEPollEvents(unsigned events);
        static unsigned convertFromEPollEvents(unsigned events);

    private:
        const std::shared_ptr<std::function<void(unsigned)>> m_callback;
        sd_event_source *m_source;
        const int m_fd;
        unsigned m_events;
    };

} // namespace sky


#endif // IONOTIFIER_P_H
