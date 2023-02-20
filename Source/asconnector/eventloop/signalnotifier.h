//
//  signalnotifier.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef SIGNALNOTIFIER_H
#define SIGNALNOTIFIER_H

namespace sky
{
    class SignalNotifierPrivate;

    class SignalNotifier
    {
    public:
        ~SignalNotifier();

    public:
        void setEnabled(bool enable);
        bool isEnabled() const;

        int signal() const;

    private:
        friend class EventLoopPrivate;
        SignalNotifier(SignalNotifierPrivate *priv);
        SignalNotifierPrivate *m_private;
    };

} // namespace sky

#endif // SIGNALNOTIFIER_H
