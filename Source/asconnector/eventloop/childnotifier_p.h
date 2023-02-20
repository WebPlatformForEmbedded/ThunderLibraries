//
//  childnotifier_p.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef CHILDNOTIFIER_P_H
#define CHILDNOTIFIER_P_H

#include <systemd/sd-event.h>

#include <memory>
#include <functional>


namespace sky
{

    class ChildNotifierPrivate
    {
    public:
        ChildNotifierPrivate(pid_t pid, std::function<void()> &&func);
        ~ChildNotifierPrivate();

        void setSource(sd_event_source *src);

    public:
        void setEnabled(bool enable);
        bool isEnabled() const;

        pid_t pid() const;

    public:
        static int handler(sd_event_source *es, const siginfo_t *si, void *userData);

    private:
        const std::shared_ptr<std::function<void()>> m_callback;
        sd_event_source *m_source;
        const pid_t m_pid;
    };

} // namespace sky

#endif // CHILDNOTIFIER_P_H
