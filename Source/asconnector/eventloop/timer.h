//
//  timer.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef SKY_TIMER_H
#define SKY_TIMER_H

#include <chrono>
#include <memory>

namespace sky
{
    class TimerPrivate;

    class Timer
    {
    public:
        ~Timer();

    public:
        void start();
        void stop();

        template< class Rep, class Period >
        void start(const std::chrono::duration<Rep, Period>& timeout)
        {
            this->startImpl(std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
        }

        std::chrono::milliseconds interval() const;

        template< class Rep, class Period >
        inline void setInterval(const std::chrono::duration<Rep, Period>& timeout)
        {
            this->setIntervalImpl(std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
        }

        bool isSingleShot() const;
        void setSingleShot(bool singleShot);

    private:
        void startImpl(std::chrono::milliseconds &&value);
        void setIntervalImpl(std::chrono::milliseconds &&value);

    private:
        friend class EventLoopPrivate;
        Timer(TimerPrivate *priv);
        TimerPrivate *m_private;
    };

} // namespace sky

#endif // SKY_TIMER_H
