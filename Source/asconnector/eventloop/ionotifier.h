//
//  ionotifier.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef IONOTIFIER_H
#define IONOTIFIER_H


namespace sky
{
    class IONotifierPrivate;

    class IONotifier
    {
    public:
        ~IONotifier();

    public:
        enum
        {
            ReadableEvent = 0x1,
            WritableEvent = 0x2,
            ErrorEvent = 0x4,
            UpdateEvent = 0x8
        };

    public:
        void setEnabled(bool enable);
        bool isEnabled() const;

        int fd() const;

        void setEvents(unsigned events);
        unsigned getEvents() const;

    private:
        friend class EventLoopPrivate;
        IONotifier(IONotifierPrivate *priv);
        IONotifierPrivate *m_private;
    };

} // namespace sky


#endif // IONOTIFIER_H
