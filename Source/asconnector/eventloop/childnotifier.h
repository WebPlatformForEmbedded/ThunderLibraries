//
//  childnotifier.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef CHILDNOTIFIER_H
#define CHILDNOTIFIER_H

#include <sys/types.h>

namespace sky
{
    class ChildNotifierPrivate;

    class ChildNotifier
    {
    public:
        ~ChildNotifier();

    public:
        void setEnabled(bool enable);
        bool isEnabled() const;

        pid_t pid() const;

    private:
        friend class EventLoopPrivate;
        ChildNotifier(ChildNotifierPrivate *priv);
        ChildNotifierPrivate *m_private;
    };

} // namespace sky

#endif // CHILDNOTIFIER_H
