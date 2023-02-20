//
//  eventloop_p.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef SKY_EVENTLOOP_P_H
#define SKY_EVENTLOOP_P_H

#include <systemd/sd-event.h>

#include <set>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <cassert>
#include <functional>

#include <pthread.h>


namespace sky
{
    class Event;
    class Timer;
    class IONotifier;
    class SignalNotifier;
    class ChildNotifier;

    class EventLoopPrivate
    {
    public:
        EventLoopPrivate();
        ~EventLoopPrivate();

        int exec();
        void quit(int exitCode);

    public:
        void flush();
        bool waitTillRunning(int timeoutMs);
        bool invokeMethod(std::function<void()> &&func);

        int addTimer(const std::chrono::microseconds &timeout,
                     bool oneShot, std::function<void(int)> &&func);
        int addTimer(const std::chrono::time_point<std::chrono::steady_clock> &expiry,
                     std::function<void(int)> &&func);
        void removeTimer(int tag);

        int addIoHandler(int fd, unsigned events,
                         std::function<void(unsigned)> &&func);
        void removeIoHandler(int tag);

        int addSignalHandler(int signal,
                             std::function<void()> &&func);
        void removeSignalHandler(int tag);

    public:
        void emitEvent(const std::shared_ptr<Event> &event, bool post);

        int addEventListener(int eventType,
                             std::function<void(const std::shared_ptr<Event>&)> &&func);
        void removeEventListener(int tag);

    public:
        std::shared_ptr<Timer> createTimer(std::function<void()> &&func);

        std::shared_ptr<IONotifier> createIONotifier(int fd, unsigned events,
                                                     std::function<void(unsigned)> &&func);

        std::shared_ptr<SignalNotifier> createSignalNotifier(int signum,
                                                             std::function<void()> &&func);

        std::shared_ptr<ChildNotifier> createChildNotifier(pid_t pid,
                                                           std::function<void()> &&func);

    public:
        void singleShotTimer(const std::chrono::microseconds &timeout,
                             std::function<void()> &&func);

    private:
        static int eventHandler(sd_event_source *es, int fd,
                                uint32_t revents, void *userdata);

        static int timerHandler(sd_event_source *es, uint64_t usec,
                                void *userdata);

        static int ioHandler(sd_event_source *es, int fd,
                             uint32_t revents, void *userdata);

        static int signalHandler(sd_event_source *es,
                                 const struct signalfd_siginfo *si,
                                 void *userdata);

        static int singleShotTimerHandler(sd_event_source *es, uint64_t usec,
                                          void *userdata);

    private:
        void executeAllMethods();

        bool callOnEventLoopThread(std::function<void()> &&func);

    private:
        friend class EventLoop;

        sd_event *m_loop;
        int m_eventFd;

        pthread_rwlock_t m_stateRwLock;
        bool m_isLoopExecing;

        mutable std::mutex m_methodsLock;
        std::queue< std::function<void()> > m_methods;

        static thread_local EventLoopPrivate *m_loopRunning;

        struct EventListener
        {
            int eventType;
            std::function<void(const std::shared_ptr<Event>&)> callback;

            EventListener(int type,
                          std::function<void(const std::shared_ptr<Event>&)> &&cb)
                : eventType(type)
                , callback(std::move(cb))
            { }
        };

        mutable std::recursive_mutex m_listenersLock;
        ssize_t m_withinEventHandler;
        std::set<int> m_removedListenersSet;
        int m_listenersTag;
        std::map<int, EventListener> m_listenersMap;



        struct TimerSource
        {
            sd_event_source *source;
            std::function<void(int)> callback;
            std::chrono::microseconds interval;

            TimerSource(sd_event_source *src, std::function<void(int)> &&cb,
                        const std::chrono::microseconds &t)
                : source(src), callback(std::move(cb)), interval(t)
            { }

            TimerSource(sd_event_source *src, std::function<void(int)> &&cb)
                : source(src), callback(std::move(cb))
            { }

            ~TimerSource()
            {
                if (source)
                {
                    sd_event_source_unref(source);
                    source = nullptr;
                }
            }
        };

        int m_timerTag;
        std::map<int, std::shared_ptr<TimerSource>> m_timerMap;

        struct IOSource
        {
            sd_event_source *source;
            std::function<void(unsigned)> callback;

            IOSource(sd_event_source *src, std::function<void(unsigned)> &&cb)
                : source(src), callback(std::move(cb))
            { }

            ~IOSource()
            {
                if (source)
                {
                    sd_event_source_unref(source);
                    source = nullptr;
                }
            }
        };

        int m_ioTag;
        std::map<int, std::shared_ptr<IOSource>> m_ioMap;

        struct SignalSource
        {
            sd_event_source *source;
            std::function<void()> callback;
            int signalNum;

            SignalSource(sd_event_source *src, std::function<void()> &&cb, int signo)
                : source(src), callback(std::move(cb)), signalNum(signo)
            { }

            ~SignalSource()
            {
                if (source)
                {
                    sd_event_source_unref(source);
                    source = nullptr;
                }
            }
        };

        int m_signalTag;
        std::map<int, std::shared_ptr<SignalSource>> m_signalMap;


        std::map<sd_event_source*, std::function<void()>> m_singleShotTimers;


    public:
        // ---------------------------------------------------------------------
        /*!
            \internal

            Performs a check that the calling thread is the same thread that is
            running the given event \a loop.  If not an assert is tripped.

            This test is only enabled if the EventLoop::enableThreadChecks(...)
            was passed \c true, it defaults to \c false.

         */
        static inline void assertCorrectThread(sd_event *loop)
        {
            if (__builtin_expect(!m_enableThreadChecks, 1))
                return;

            pthread_rwlock_rdlock(&m_globalLoopThreadsRwLock);
            auto it = m_globalLoopThreads.find(loop);
            assert((it == m_globalLoopThreads.end()) ||
                   (it->second == std::this_thread::get_id()));
            pthread_rwlock_unlock(&m_globalLoopThreadsRwLock);
        }

        static inline void assertCorrectThread(sd_event_source *source)
        {
            assertCorrectThread(sd_event_source_get_event(source));
        }


    private:
        static bool m_enableThreadChecks;
        static pthread_rwlock_t m_globalLoopThreadsRwLock;
        static std::map<sd_event*, std::thread::id> m_globalLoopThreads;

    };

} // namespace sky


#endif // SKY_EVENTLOOP_P_H
