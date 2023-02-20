//
//  signalnotifier_p.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef SIGNALNOTIFIER_P_H
#define SIGNALNOTIFIER_P_H

#include <systemd/sd-event.h>

#include <memory>
#include <functional>


namespace sky
{

    class SignalNotifierPrivate
    {
    public:
        SignalNotifierPrivate(int signum, std::function<void()> &&func);
        ~SignalNotifierPrivate();

        void setSource(sd_event_source *src);

     public:
        void setEnabled(bool enable);
        bool isEnabled() const;

        int signal() const;

    public:
        static int handler(sd_event_source *es, const struct signalfd_siginfo *si, void *userData);

    private:
        const std::shared_ptr<std::function<void()>> m_callback;
        sd_event_source *m_source;
        const int m_signalNumber;
    };

} // namespace sky



#endif // SIGNALNOTIFIER_P_H
