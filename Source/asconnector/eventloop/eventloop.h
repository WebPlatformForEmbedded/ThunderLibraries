//
//  eventloop.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef SKY_EVENTLOOP_H
#define SKY_EVENTLOOP_H

#include <atomic>
#include <memory>
#include <functional>
#include <chrono>

typedef struct sd_event sd_event;


namespace sky
{
    class Timer;
    class IONotifier;
    class SignalNotifier;
    class ChildNotifier;

    class EventLoopPrivate;

    class Event
    {
    public:
        explicit Event(int type) : mType(type)
        { }

        virtual ~Event() = default;

        inline int type() const
        {
            return mType;
        }

    private:
        int mType;
    };

    class EventLoop
    {
    public:
        EventLoop();
        EventLoop(const EventLoop &other);
        EventLoop(EventLoop &&other) noexcept;
        ~EventLoop();

        sd_event *handle() const;

        int exec();
        void quit(int exitCode);

        bool onEventLoopThread() const;

        static void enableThreadChecks(bool enable);

        bool waitTillRunning(int timeoutMs);

    public:
        void postEvent(const std::shared_ptr<Event> &event) const;
        void sendEvent(const std::shared_ptr<Event> &event) const;

        template< class Function >
        inline int addEventListener(int eventType, Function&& func) const
        {
            return this->addEventListenerImpl(eventType,
                                             std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline int addEventListener(int eventType, Function &&func, Args&&... args) const
        {
            return this->addEventListenerImpl(eventType,
                                             std::bind(std::forward<Function>(func),
                                                       std::forward<Args>(args)...));
        }

        void removeEventListener(int tag) const;

    public:
        template< class Function >
        inline std::shared_ptr<Timer> createTimer(Function func) const
        {
            return this->createTimerImpl(std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline std::shared_ptr<Timer> createTimer(Function &&func, Args&&... args) const
        {
            return this->createTimerImpl(std::bind(std::forward<Function>(func),
                                                   std::forward<Args>(args)...));
        }

    public:
        template< class Rep, class Period, class Function >
        void singleShotTimer(const std::chrono::duration<Rep, Period> &timeout,
                             Function func) const
        {
            this->singleShotTimerImpl(std::chrono::duration_cast<std::chrono::microseconds>(timeout),
                                      std::forward<Function>(func));
        }

        template< class Rep, class Period, class Function, class... Args >
        void singleShotTimer(const std::chrono::duration<Rep, Period> &timeout,
                             Function &&func, Args&&... args) const
        {
            this->singleShotTimerImpl(std::chrono::duration_cast<std::chrono::microseconds>(timeout),
                                      std::bind(std::forward<Function>(func),
                                                std::forward<Args>(args)...));
        }

    public:
        template< class Function >
        inline std::shared_ptr<IONotifier> createIONotifier(int fd, unsigned events,
                                                            Function func) const
        {
            return this->createIONotifierImpl(fd, events, std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline std::shared_ptr<IONotifier> createIONotifier(int fd, unsigned events,
                                                            Function &&func, Args&&... args) const
        {
            return this->createIONotifierImpl(fd, events, std::bind(std::forward<Function>(func),
                                                                    std::forward<Args>(args)...));
        }

    public:
        template< class Function >
        inline std::shared_ptr<SignalNotifier> createSignalNotifier(int signum,
                                                                    Function func) const
        {
            return this->createSignalNotifierImpl(signum, std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline std::shared_ptr<SignalNotifier> createSignalNotifier(int signum,
                                                                    Function&& func, Args&&... args) const
        {
            return this->createSignalNotifierImpl(signum, std::bind(std::forward<Function>(func),
                                                                    std::forward<Args>(args)...));
        }

    public:
        template< class Function >
        inline std::shared_ptr<ChildNotifier> createChildNotifier(pid_t pid,
                                                                  Function func) const
        {
            return this->createChildNotifierImpl(pid, std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline std::shared_ptr<ChildNotifier> createChildNotifier(pid_t pid,
                                                                  Function&& func, Args&&... args) const
        {
            return this->createChildNotifierImpl(pid, std::bind(std::forward<Function>(func),
                                                                std::forward<Args>(args)...));
        }


    public:
#if defined(ENABLE_OLD_API)
        template< class Rep, class Period, class Function >
        inline int addTimer(const std::chrono::duration<Rep, Period>& timeout,
                            bool oneShot, Function&& func) const
        {
            return this->addTimerImpl(std::chrono::duration_cast<std::chrono::microseconds>(timeout),
                                      oneShot, std::forward<Function>(func));
        }

        template< class Rep, class Period, class Function, class... Args >
        inline int addTimer(const std::chrono::duration<Rep, Period>& timeout,
                            bool oneShot, Function &&func, Args&&... args) const
        {
            return this->addTimerImpl(std::chrono::duration_cast<std::chrono::microseconds>(timeout),
                                      oneShot,
                                      std::bind(std::forward<Function>(func),
                                                std::forward<Args>(args)...));
        }

        template< class Function >
        inline int addTimer(const std::chrono::time_point<std::chrono::steady_clock> &expiry,
                            Function&& func) const
        {
            return this->addTimerImpl(expiry, std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline int addTimer(const std::chrono::time_point<std::chrono::steady_clock> &expiry,
                            Function &&func, Args&&... args) const
        {
            return this->addTimerImpl(expiry, std::bind(std::forward<Function>(func),
                                                        std::forward<Args>(args)...));
        }
#endif

        void removeTimer(int tag) const;

    public:
        enum
        {
            ReadableEvent = 0x1,
            WritableEvent = 0x2,
            ErrorEvent = 0x4,
            UpdateEvent = 0x8
        };

#if defined(ENABLE_OLD_API)
        template< class Function >
        inline int addIoHandler(int fd, unsigned events, Function&& func) const
        {
            return this->addIoHandlerImpl(fd, events,
                                          std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline int addIoHandler(int fd, unsigned events, Function &&func, Args&&... args) const
        {
            return this->addIoHandlerImpl(fd, events,
                                          std::bind(std::forward<Function>(func),
                                                    std::forward<Args>(args)...));
        }
#endif

        void removeIoHandler(int tag) const;

    public:
#if defined(ENABLE_OLD_API)
        template< class Function >
        inline int addSignalHandler(int signal, Function&& func) const
        {
            return this->addSignalHandlerImpl(signal,
                                              std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline int addSignalHandler(int signal, Function &&func, Args&&... args) const
        {
            return this->addSignalHandlerImpl(signal,
                                              std::bind(std::forward<Function>(func),
                                                        std::forward<Args>(args)...));
        }
#endif

        void removeSignalHandler(int tag) const;


    public:
        void flush();

        template< class Function >
        inline bool invokeMethod(Function func) const
        {
            return this->invokeMethodImpl(std::forward<Function>(func));
        }

        template< class Function, class... Args >
        inline bool invokeMethod(Function &&func, Args&&... args) const
        {
            return this->invokeMethodImpl(std::bind(std::forward<Function>(func),
                                                    std::forward<Args>(args)...));
        }

    private:
        bool invokeMethodImpl(std::function<void()> func) const;

        int addTimerImpl(const std::chrono::microseconds &timeout,
                         bool oneShot, std::function<void(int)> &&func) const;
        int addTimerImpl(const std::chrono::time_point<std::chrono::steady_clock> &expiry,
                         std::function<void(int)> &&func) const;
        int addIoHandlerImpl(int fd, unsigned events,
                             std::function<void(unsigned)> &&func) const;
        int addSignalHandlerImpl(int signal, std::function<void()> &&func) const;

        int addEventListenerImpl(int eventType, std::function<void(const std::shared_ptr<Event>&)> func) const;

        std::shared_ptr<Timer> createTimerImpl(std::function<void()> func) const;
        std::shared_ptr<IONotifier> createIONotifierImpl(int fd, unsigned events,
                                                         std::function<void(unsigned)> func) const;
        std::shared_ptr<SignalNotifier> createSignalNotifierImpl(int signum,
                                                                 std::function<void()> func) const;
        std::shared_ptr<ChildNotifier> createChildNotifierImpl(pid_t pid,
                                                               std::function<void()> func) const;

        void singleShotTimerImpl(std::chrono::microseconds timeout, std::function<void()> func) const;

    private:
        std::shared_ptr<EventLoopPrivate> m_private;
    };

} // namespace sky

#endif // SKY_EVENTLOOP_H
