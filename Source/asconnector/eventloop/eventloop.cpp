//
//  eventloop.cpp
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "eventloop.h"
#include "eventloop_p.h"
#include "timer.h"
#include "timer_p.h"
#include "ionotifier.h"
#include "ionotifier_p.h"
#include "signalnotifier.h"
#include "signalnotifier_p.h"
#include "childnotifier.h"
#include "childnotifier_p.h"
#include "safesemaphore_p.h"
#include "sky/log.h"

#include <systemd/sd-event.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/eventfd.h>


using namespace sky;


thread_local EventLoopPrivate * EventLoopPrivate::m_loopRunning = nullptr;


bool EventLoopPrivate::m_enableThreadChecks = false;

/// read / write lock protecting the m_loopThreads map
pthread_rwlock_t EventLoopPrivate::m_globalLoopThreadsRwLock = PTHREAD_RWLOCK_INITIALIZER;

/// map of running sd_event loops to their threads
std::map<sd_event*, std::thread::id> EventLoopPrivate::m_globalLoopThreads;



EventLoop::EventLoop()
    : m_private(std::make_shared<EventLoopPrivate>())
{
}

EventLoop::EventLoop(const EventLoop &other)
    : m_private(other.m_private)
{
}

EventLoop::EventLoop(EventLoop &&other) noexcept
    : m_private(std::move(other.m_private))
{
}

EventLoop::~EventLoop()
{
    m_private.reset();
}

sd_event* EventLoop::handle() const
{
    return m_private->m_loop;
}

int EventLoop::exec()
{
    return m_private->exec();
}

void EventLoop::quit(int exitCode)
{
    return m_private->quit(exitCode);
}

bool EventLoop::invokeMethodImpl(std::function<void()> func) const
{
    return m_private->invokeMethod(std::move(func));
}

void EventLoop::postEvent(const std::shared_ptr<Event> &event) const
{
    m_private->emitEvent(event, true);
}

void EventLoop::sendEvent(const std::shared_ptr<Event> &event) const
{
    m_private->emitEvent(event, false);
}

int EventLoop::addEventListenerImpl(int eventType,
                                    std::function<void(const std::shared_ptr<Event>&)> func) const
{
    return m_private->addEventListener(eventType, std::move(func));
}

void EventLoop::removeEventListener(int tag) const
{
    m_private->removeEventListener(tag);
}

std::shared_ptr<Timer> EventLoop::createTimerImpl(std::function<void()> func) const
{
    return m_private->createTimer(std::move(func));
}

std::shared_ptr<IONotifier> EventLoop::createIONotifierImpl(int fd, unsigned events,
                                                            std::function<void(unsigned)> func) const
{
    return m_private->createIONotifier(fd, events, std::move(func));
}

std::shared_ptr<SignalNotifier> EventLoop::createSignalNotifierImpl(int signum,
                                                                    std::function<void()> func) const
{
    return m_private->createSignalNotifier(signum, std::move(func));
}

std::shared_ptr<ChildNotifier> EventLoop::createChildNotifierImpl(pid_t pid,
                                                                  std::function<void()> func) const
{
    return m_private->createChildNotifier(pid, std::move(func));
}

void EventLoop::singleShotTimerImpl(std::chrono::microseconds timeout, std::function<void()> func) const
{
    return m_private->singleShotTimer(timeout, std::move(func));
}

int EventLoop::addTimerImpl(const std::chrono::microseconds &timeout,
                            bool oneShot, std::function<void(int)> &&func) const
{
    return m_private->addTimer(timeout, oneShot, std::move(func));
}

int EventLoop::addTimerImpl(const std::chrono::time_point<std::chrono::steady_clock> &expiry,
                            std::function<void(int)> &&func) const
{
    return m_private->addTimer(expiry, std::move(func));
}

void EventLoop::removeTimer(int tag) const
{
    m_private->removeTimer(tag);
}

int EventLoop::addIoHandlerImpl(int fd, unsigned events,
                                std::function<void(unsigned)> &&func) const
{
    return m_private->addIoHandler(fd, events, std::move(func));
}

void EventLoop::removeIoHandler(int tag) const
{
    m_private->removeIoHandler(tag);
}

int EventLoop::addSignalHandlerImpl(int signal, std::function<void()> &&func) const
{
    return m_private->addSignalHandler(signal, std::move(func));
}

void EventLoop::removeSignalHandler(int tag) const
{
    return m_private->removeSignalHandler(tag);
}

void EventLoop::flush()
{
    return m_private->flush();
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Should only be called from a thread different from the one running the
    event loop.  This blocks until either the timeout has expired or the event
    loop is confirmed to be up and running.

 */
bool EventLoop::waitTillRunning(int timeoutMs)
{
    return m_private->waitTillRunning(timeoutMs);
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if the calling thread is the same thread that is executing
    the event loop.

    If the event loop hasn't been started this will return \c false.
 */
bool EventLoop::onEventLoopThread() const
{
    return (EventLoopPrivate::m_loopRunning == m_private.get());
}

// -----------------------------------------------------------------------------
/*!
    If \a enable is \c true then the library will perform checks on the
    non-thread safe APIs to ensure they are called from the context of the
    event loop they are executing on.

    This is a global setting that affects all event loops.

    If any check fails - the function is being called from a different thread -
    then an assert is raised.

    There is a small performance hit when enabling this flag as all non-thread
    safe function calls will perform the check, but it's relatively negligible
    and shouldn't be an issue unless the app is very I/O intensive.

    By default this flag is disabled.
 */
void EventLoop::enableThreadChecks(bool enable)
{
    EventLoopPrivate::m_enableThreadChecks = enable;
}

// -----------------------------------------------------------------------------
/*!


 */
EventLoopPrivate::EventLoopPrivate()
    : m_loop(nullptr)
    , m_eventFd(-1)
    , m_stateRwLock(PTHREAD_RWLOCK_INITIALIZER)
    , m_isLoopExecing(false)
    , m_withinEventHandler(0)
    , m_listenersTag(1000 + (rand() % 1000))
    , m_timerTag(1000 + (rand() % 1000))
    , m_ioTag(2000 + (rand() % 1000))
    , m_signalTag(3000 + (rand() % 1000))
{
    int rc = sd_event_new(&m_loop);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to create new event loop");
        return;
    }

    m_eventFd = eventfd(0, EFD_CLOEXEC);
    if (m_eventFd < 0)
    {
        LOG_SYS_ERROR(errno, "failed to create eventfd");
    }
}

// -----------------------------------------------------------------------------
/*!


 */
EventLoopPrivate::~EventLoopPrivate()
{
    // clean-up eventfd
    if ((m_eventFd >= 0) && (close(m_eventFd) != 0))
    {
        LOG_SYS_ERROR(errno, "failed to close eventfd");
    }

    // free all the timers
    for (auto &timer : m_timerMap)
    {
        const std::shared_ptr<TimerSource> &source = timer.second;
        if (source && source->source)
        {
            sd_event_source_unref(source->source);
            source->source = nullptr;
        }
    }

    // free all single shot timers that haven't fired
    for (auto &timer : m_singleShotTimers)
    {
        sd_event_source *source = timer.first;
        if (source)
            sd_event_source_unref(source);
    }

    // free all the io handlers
    for (auto &io : m_ioMap)
    {
        const std::shared_ptr<IOSource> &source = io.second;
        if (source && source->source)
        {
            sd_event_source_unref(source->source);
            source->source = nullptr;
        }
    }

    // free all the signal handlers
    for (auto &sig : m_signalMap)
    {
        const std::shared_ptr<SignalSource> &source = sig.second;
        if (source && source->source)
        {
            sd_event_source_unref(source->source);
            source->source = nullptr;
        }
    }

    // free event loop memory
    if (m_loop)
    {
        sd_event_unref(m_loop);
    }

    pthread_rwlock_destroy(&m_stateRwLock);

    LOG_INFO("destroyed event loop");
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Executes all the methods stored in the m_methods queue.

 */
void EventLoopPrivate::executeAllMethods()
{
    std::unique_lock<std::mutex> locker(m_methodsLock);
    while (!m_methods.empty())
    {
        std::function<void()> fn = std::move(m_methods.front());
        m_methods.pop();

        locker.unlock();

        if (fn)
            fn();

        locker.lock();
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Callback function called when the m_eventFd has a value to read in it.

 */
int EventLoopPrivate::eventHandler(sd_event_source *es, int fd,
                                   uint32_t revents, void *userData)
{
    EventLoopPrivate *self = reinterpret_cast<EventLoopPrivate*>(userData);
    assert(self != nullptr);
    assert(fd == self->m_eventFd);

    // read the eventfd to clear the event
    uint64_t ignore;
    if (TEMP_FAILURE_RETRY(read(self->m_eventFd, &ignore, sizeof(ignore))) != sizeof(ignore))
    {
        LOG_SYS_ERROR(errno, "failed to read from eventfd");
    }

    // process all queue methods
    self->executeAllMethods();

    // done
    return 0;
}

// -----------------------------------------------------------------------------
/*!
    Starts the event loop.  This is a blocking call that won't return until
    EventLoop::quit has been called.

 */
int EventLoopPrivate::exec()
{
    // sanity check
    if (!m_loop)
    {
        LOG_WARNING("no event loop to start");
        return EXIT_FAILURE;
    }

    // install a handler for the quit eventfd
    sd_event_source *eventSource = nullptr;
    int rc = sd_event_add_io(m_loop, &eventSource, m_eventFd,
                             EPOLLIN, eventHandler, this);
    if (rc < 0)
    {
        LOG_SYS_ERROR(errno, "failed to attach quit eventfd source");
        return EXIT_FAILURE;
    }

    // update the state
    pthread_rwlock_wrlock(&m_stateRwLock);
    m_isLoopExecing = true;
    pthread_rwlock_unlock(&m_stateRwLock);

    // update the global map for thread debug checking
    pthread_rwlock_wrlock(&m_globalLoopThreadsRwLock);
    m_globalLoopThreads[m_loop] = std::this_thread::get_id();
    pthread_rwlock_unlock(&m_globalLoopThreadsRwLock);


    // set the thread local pointer back to us
    m_loopRunning = this;

    // run the event loop
    int exitCode = sd_event_loop(m_loop);

    // clear it now the loop has stopped
    m_loopRunning = nullptr;


    // update the global map for thread debug checking
    pthread_rwlock_wrlock(&m_globalLoopThreadsRwLock);
    m_globalLoopThreads.erase(m_loop);
    pthread_rwlock_unlock(&m_globalLoopThreadsRwLock);

    // update the state
    pthread_rwlock_wrlock(&m_stateRwLock);
    m_isLoopExecing = false;
    pthread_rwlock_unlock(&m_stateRwLock);

    // process all left over executors
    executeAllMethods();

    // can now free the event source
    sd_event_source_unref(eventSource);

    return exitCode;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Invokes the supplied method the next time through the event loop.  The
    \a func will always be called from the thread that the event loop is
    executing in.

 */
bool EventLoopPrivate::invokeMethod(std::function<void()> &&func)
{
    // push the function onto the queue
    {
        std::lock_guard<std::mutex> locker(m_methodsLock);
        m_methods.emplace(std::move(func));
    }

    // wake the event loop by writing to the eventfd
    uint64_t wake = 1;
    if (TEMP_FAILURE_RETRY(write(m_eventFd, &wake, sizeof(wake))) != sizeof(wake))
    {
        LOG_SYS_ERROR(errno, "failed to write to eventfd");
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Blocks until all queued methods have been executed

 */
void EventLoopPrivate::flush()
{
    // if running on the event loop thread or the event loop is no longer
    // running, then just execute all the methods until the queue is empty
    pthread_rwlock_rdlock(&m_stateRwLock);
    if (!m_isLoopExecing || (EventLoopPrivate::m_loopRunning == this))
    {
        pthread_rwlock_unlock(&m_stateRwLock);
        executeAllMethods();
        return;
    }
    pthread_rwlock_unlock(&m_stateRwLock);

    // otherwise post a new lambda to the event loop and wait till it's
    // executed
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    std::function<void()> markLambda =
        [&]()
        {
            if (sem_post(&semaphore) != 0)
                LOG_SYS_ERROR(errno, "failed to post semaphore in flush lambda");
        };

    if (!invokeMethod(std::move(markLambda)))
        LOG_ERROR("failed to schedule flush lambda");
    else if (TEMP_FAILURE_RETRY(sem_wait(&semaphore)) != 0)
        LOG_SYS_ERROR(errno, "failed to wait for semaphore in flush routine");

    sem_destroy(&semaphore);
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Queues a message on the event loop then blocks until it's executed or a
    timeout occurs.

 */
bool EventLoopPrivate::waitTillRunning(int timeoutMs)
{
    // if running on the event loop thread or the event loop is no longer
    // running, then just execute all the methods until the queue is empty
    pthread_rwlock_rdlock(&m_stateRwLock);
    if (EventLoopPrivate::m_loopRunning == this)
    {
        pthread_rwlock_unlock(&m_stateRwLock);
        LOG_WARNING("don't call waitTillRunning from the thread running the event loop");
        return true;
    }
    pthread_rwlock_unlock(&m_stateRwLock);


    // use a shared_ptr so it will be deleted by the lambda even if we timed out
    std::shared_ptr<SafeSemaphore> signaller = std::make_shared<SafeSemaphore>();

    std::function<void()> markLambda =
        [signaller]()
        {
            signaller->notify();
        };

    if (!invokeMethod(std::move(markLambda)))
    {
        LOG_ERROR("failed to schedule flush lambda");
        return false;
    }

    return signaller->wait(timeoutMs);
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Quits the event loop which unblocks the EventLoop::exec() function.  It is
    safe to call this function from within the event loop thread or externally.

 */
void EventLoopPrivate::quit(int exitCode)
{
    // lambda to ensure quit is always called from event loop thread
    auto quitLambda =
        [this, exitCode]()
        {
            sd_event_exit(m_loop, exitCode);
        };

    // queue the lambda to be called next time through the event loop
    invokeMethod(quitLambda);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Calls the given func from the context of event loop thread.

    If already running on the event loop thread then the \a func is called
    directly, otherwise it is queued on the event loop and the function blocks
    until the \a func has been executed.

    If the event loop is not yet running then the function is called directly
    however a lock is taken to ensure only one instance of this function is
    called.

 */
bool EventLoopPrivate::callOnEventLoopThread(std::function<void()> &&func)
{
    // check if the loop is running
    pthread_rwlock_rdlock(&m_stateRwLock);
    if (!m_isLoopExecing)
    {
        // re-take as a write lock so the event loop isn't started while
        // executing the function
        pthread_rwlock_unlock(&m_stateRwLock);
        pthread_rwlock_wrlock(&m_stateRwLock);

        // need to re-check the loop is still not running and if not executing
        // call the func
        if (!m_isLoopExecing)
        {
            func();
            pthread_rwlock_unlock(&m_stateRwLock);
            return true;
        }
    }
    pthread_rwlock_unlock(&m_stateRwLock);


    // if running on the eventloop thread then just call the function
    if (EventLoopPrivate::m_loopRunning == this)
    {
        func();
        return true;
    }

    // otherwise wrap the function in a lambda that is invoked on the event
    // loop thread, the semaphore is used to wake the caller when the func
    // has finished executing
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    std::function<void()> wrapperLambda =
        [&semaphore, fn = std::move(func)]()
        {
            // call the function
            fn();

            // wake the caller thread
            if (sem_post(&semaphore) != 0)
                LOG_SYS_ERROR(errno, "failed to post semaphore in flush lambda");
        };

    bool success = false;

    if (!invokeMethod(std::move(wrapperLambda)))
        LOG_ERROR("failed to schedule lambda");
    else if (TEMP_FAILURE_RETRY(sem_wait(&semaphore)) != 0)
        LOG_SYS_ERROR(errno, "failed to wait for semaphore");
    else
        success = true;

    sem_destroy(&semaphore);

    return success;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Static callback for all timer events.

 */
int EventLoopPrivate::timerHandler(sd_event_source *es, uint64_t usec,
                                   void *userdata)
{
    EventLoopPrivate *self = reinterpret_cast<EventLoopPrivate*>(userdata);

    // find the matching event source
    auto it = self->m_timerMap.begin();
    for (; it != self->m_timerMap.end(); ++it)
    {
        if (it->second->source == es)
            break;
    }

    if (it == self->m_timerMap.end())
    {
        LOG_ERROR("failed to find callback for timer with source %p", es);
        return -1;
    }

    // get the tag
    const int tag = it->first;

    LOG_INFO("timer handler called for tag %d", tag);

    // get the handler
    std::shared_ptr<TimerSource> timer = it->second;
    if (timer->source != es)
    {
        LOG_ERROR("odd, timer source doesn't match");
        return -1;
    }

    // and call it
    if (timer->callback)
    {
        timer->callback(tag);
    }

    // if the timer is one-shot then remove it now, otherwise reset the time
    int enabled = SD_EVENT_OFF;
    int rc = sd_event_source_get_enabled(es, &enabled);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to get timer state");
        return rc;
    }

    if (enabled == SD_EVENT_ON)
    {
        // calculate the expiry time
        uint64_t expiry;
        sd_event_now(sd_event_source_get_event(es), CLOCK_MONOTONIC, &expiry);
        expiry += timer->interval.count();

        // update the timer
        sd_event_source_set_time(es, expiry);
    }
    else
    {
        // erase the entry, not we can't use the iterator from above as the
        // client may have removed the timer in it's callback
        self->m_timerMap.erase(tag);
    }

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Adds a timer to the event loop.  The handler will be called from the
    context of the event loop thread.

 */
int EventLoopPrivate::addTimer(const std::chrono::microseconds &timeout,
                               bool oneShot, std::function<void(int)> &&func)
{
    // tag of created timer
    int tag = -1;

    // create a lambda to install the timer
    std::function<void()> installTimerLambda =
        [&]()
        {
            // calculate the expiry time
            uint64_t expiry;
            sd_event_now(m_loop, CLOCK_MONOTONIC, &expiry);
            expiry += timeout.count();

            // create the event source
            sd_event_source *source = nullptr;
            int rc = sd_event_add_time(m_loop, &source, CLOCK_MONOTONIC, expiry, 0,
                                       &EventLoopPrivate::timerHandler, this);
            if ((rc < 0) || (source == nullptr))
            {
                LOG_SYS_ERROR(-rc, "failed to install timer");
            }
            else
            {
                // assign a new tag
                tag = m_timerTag++;

                // set (or un-set) one shot timer
                if (oneShot)
                    sd_event_source_set_enabled(source, SD_EVENT_ONESHOT);
                else
                    sd_event_source_set_enabled(source, SD_EVENT_ON);

                LOG_INFO("installed timer with tag %d - this = %p", tag, this);

                // finally add the timer to the map
                m_timerMap.emplace(tag, std::make_shared<TimerSource>(source, std::move(func), timeout));
            }
        };

    // if running on the eventloop thread then just call the lambda
    callOnEventLoopThread(std::move(installTimerLambda));

    // return the tag number set in the lambda
    return tag;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Adds a timer to expire at the given

 */
int EventLoopPrivate::addTimer(const std::chrono::time_point<std::chrono::steady_clock> &expiry,
                               std::function<void(int)> &&func)
{
    // tag of created timer
    int tag = -1;

    // create a lambda to install the timer
    std::function<void()> installTimerLambda =
        [&]()
        {
            // calculate the expiry time relative to the current monotonic clock
            std::chrono::microseconds diff =
                std::chrono::duration_cast<std::chrono::microseconds>(expiry - std::chrono::steady_clock::now());

            uint64_t usecs;
            sd_event_now(m_loop, CLOCK_MONOTONIC, &usecs);
            usecs += diff.count();

            // create the event source
            sd_event_source *source = nullptr;
            int rc = sd_event_add_time(m_loop, &source, CLOCK_MONOTONIC, usecs, 0,
                                       &EventLoopPrivate::timerHandler, this);
            if ((rc < 0) || (source == nullptr))
            {
                LOG_SYS_ERROR(-rc, "failed to install timer");
            }
            else
            {
                // assign a new tag
                tag = m_timerTag++;

                // all fixed point timers are one-shot
                sd_event_source_set_enabled(source, SD_EVENT_ONESHOT);

                LOG_INFO("installed timer with tag %d - this = %p", tag, this);

                // finally add the timer to the map
                m_timerMap.emplace(tag, std::make_shared<TimerSource>(source, std::move(func)));
            }
        };

    // if running on the eventloop thread then just call the lambda
    callOnEventLoopThread(std::move(installTimerLambda));

    // return the tag number set in the lambda
    return tag;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Removes a timer previous installed with addTimer(...).  It is safe to call
    this from any thread or from the timer callback itself.

 */
void EventLoopPrivate::removeTimer(int tag)
{
    // create the lambda to do the remove
    std::function<void()> removeTimerLambda =
        [&]()
        {
            // find the timer
            auto it = m_timerMap.find(tag);
            if (it == m_timerMap.end())
            {
                // don't log this as an error because for oneshot timers the
                // timer will be removed automatically after firing
                LOG_INFO("no timer found with tag %d", tag);
                return;
            }

            std::shared_ptr<TimerSource> timer = it->second;

            // set the event source to off
            sd_event_source_set_enabled(timer->source, SD_EVENT_OFF);

            // remove from the map
            m_timerMap.erase(it);
        };

    // if running on the event loop thread then just call the lambda
    callOnEventLoopThread(std::move(removeTimerLambda));
}

// -----------------------------------------------------------------------------
/*!
    Creates a new timer - initially stopped - which will call the given func
    when started and expires.

    \warning This function is not thread safe, it should be called from the
    event loop thread and the returned object should also only be accessed from
    the event loop thread.

 */
std::shared_ptr<Timer> EventLoopPrivate::createTimer(std::function<void()> &&func)
{
    EventLoopPrivate::assertCorrectThread(m_loop);

    // allocate a new timer object
    TimerPrivate *timer = new TimerPrivate(std::move(func));

    // create the event source
    sd_event_source *source = nullptr;
    int rc = sd_event_add_time(m_loop, &source, CLOCK_MONOTONIC, UINT64_MAX, 1000,
                               &TimerPrivate::handler, timer);
    if ((rc < 0) || (source == nullptr))
    {
        LOG_SYS_ERROR(-rc, "failed to install timer");
        delete timer;
        return nullptr;
    }

    // disable the timer
    rc = sd_event_source_set_enabled(source, SD_EVENT_OFF);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to disable timer");
        sd_event_source_unref(source);
        delete timer;
        return nullptr;
    }

    // gift the source to the timer object
    timer->setSource(source);

    // return the wrapped result
    return std::shared_ptr<Timer>(new Timer(timer));
}

// -----------------------------------------------------------------------------
/*!
    Creates a new io listener attached to the \a fd which will call \a func
    when one of the \a events in the bitmask occurs.

    \warning This function is not thread safe, it should be called from the
    event loop thread and the returned object should also only be accessed from
    the event loop thread.

 */
std::shared_ptr<IONotifier> EventLoopPrivate::createIONotifier(int fd, unsigned events,
                                                               std::function<void(unsigned)> &&func)
{
    EventLoopPrivate::assertCorrectThread(m_loop);

    // allocate a new timer object
    IONotifierPrivate *io = new IONotifierPrivate(fd, events, std::move(func));

    // create the event source
    sd_event_source *source = nullptr;
    int rc = sd_event_add_io(m_loop, &source, fd, events,
                             &IONotifierPrivate::handler, io);
    if ((rc < 0) || (source == nullptr))
    {
        LOG_SYS_ERROR(-rc, "failed to create io listener");
        delete io;
        return nullptr;
    }

    // gift the source to the timer object
    io->setSource(source);

    // return the wrapped result
    return std::shared_ptr<IONotifier>(new IONotifier(io));
}

// -----------------------------------------------------------------------------
/*!
    Creates a new unix signal listener attached for \a signum which will call
    \a func when the signal is raised.

    \warning This function is not thread safe, it should be called from the
    event loop thread and the returned object should also only be accessed from
    the event loop thread.

 */
std::shared_ptr<SignalNotifier> EventLoopPrivate::createSignalNotifier(int signum,
                                                                       std::function<void()> &&func)
{
    EventLoopPrivate::assertCorrectThread(m_loop);

    // allocate a new listener object
    SignalNotifierPrivate *sig = new SignalNotifierPrivate(signum, std::move(func));

    // create the event source
    sd_event_source *source = nullptr;
    int rc = sd_event_add_signal(m_loop, &source, signum,
                                 &SignalNotifierPrivate::handler, sig);
    if ((rc < 0) || (source == nullptr))
    {
        LOG_SYS_ERROR(-rc, "failed to create signal listener");
        delete sig;
        return nullptr;
    }

    // gift the source to the listener object
    sig->setSource(source);

    // return the wrapped result
    return std::shared_ptr<SignalNotifier>(new SignalNotifier(sig));
}

// -----------------------------------------------------------------------------
/*!
    Creates a new child process listener attached to the process with \a pid.

    \warning This function is not thread safe, it should be called from the
    event loop thread and the returned object should also only be accessed from
    the event loop thread.

 */
std::shared_ptr<ChildNotifier> EventLoopPrivate::createChildNotifier(pid_t pid,
                                                                     std::function<void()> &&func)
{
    EventLoopPrivate::assertCorrectThread(m_loop);

    // allocate a new listener object
    ChildNotifierPrivate *child = new ChildNotifierPrivate(pid, std::move(func));

    // create the event source
    sd_event_source *source = nullptr;
    int rc = sd_event_add_child(m_loop, &source, pid, WEXITED,
                                &ChildNotifierPrivate::handler, child);
    if ((rc < 0) || (source == nullptr))
    {
        LOG_SYS_ERROR(-rc, "failed to create child listener");
        delete child;
        return nullptr;
    }

    // gift the source to the listener object
    child->setSource(source);

    // return the wrapped result
    return std::shared_ptr<ChildNotifier>(new ChildNotifier(child));

}

// -----------------------------------------------------------------------------
/*!
    \internal

    Static callback for all timer events.

 */
int EventLoopPrivate::ioHandler(sd_event_source *es, int fd,
                                uint32_t revents, void *userdata)
{
    EventLoopPrivate *self = reinterpret_cast<EventLoopPrivate*>(userdata);

    LOG_INFO("io handler called for fd %d", fd);

    // find the matching event source
    auto it = self->m_ioMap.begin();
    for (; it != self->m_ioMap.end(); ++it)
    {
        if (it->second->source == es)
            break;
    }

    if (it == self->m_ioMap.end())
    {
        LOG_ERROR("failed to find callback for io with source %p", es);
        return -1;
    }

    // get the handler
    std::shared_ptr<IOSource> io = it->second;
    if (io->source != es)
    {
        LOG_ERROR("odd, io source doesn't match");
        return -1;
    }

    // convert the events
    unsigned events = 0;
    if (revents & EPOLLIN)
        events |= EventLoop::ReadableEvent;
    if (revents & EPOLLOUT)
        events |= EventLoop::WritableEvent;
    if (revents & EPOLLERR)
        events |= EventLoop::ErrorEvent;
    if (revents & EPOLLHUP)
        events |= EventLoop::UpdateEvent;

    // and call it
    if (io->callback)
    {
        io->callback(events);
    }

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Adds a file descriptor handler to the event loop.  The \a func will be
    called when any of the events in the \a events argument are detected.

 */
int EventLoopPrivate::addIoHandler(int fd, unsigned events,
                                   std::function<void(unsigned)> &&func)
{
    // tag of created timer
    int tag = -1;

    // create a lambda to install the timer
    std::function<void()> installIOLambda =
        [&]()
        {
            // convert the events flags
            uint32_t revents = 0;
            if (events & EventLoop::ReadableEvent)
                revents |= EPOLLIN;
            if (events & EventLoop::WritableEvent)
                revents |= EPOLLOUT;

            // create the event source
            sd_event_source *source = nullptr;
            int rc = sd_event_add_io(m_loop, &source, fd, revents,
                                     &EventLoopPrivate::ioHandler, this);
            if ((rc < 0) || (source == nullptr))
            {
                LOG_SYS_ERROR(-rc, "failed to install io handler");
            }
            else
            {
                // assign a new tag
                tag = m_ioTag++;

                LOG_INFO("added io handler for fd %d and events 0x%04x", fd, revents);

                // finally add the handler to the map
                m_ioMap.emplace(tag, std::make_shared<IOSource>(source, std::move(func)));
            }
        };

    // if running on the eventloop thread then just call the lambda
    callOnEventLoopThread(std::move(installIOLambda));

    // return the tag number set in the lambda
    return tag;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Removes a io handler previous installed with addIoHandler(...).  It is
    safe to call this from any thread or from the io callback itself.

 */
void EventLoopPrivate::removeIoHandler(int tag)
{
    // create the lambda to do the remove
    std::function<void()> removeIOLambda =
        [&]()
        {
            // find the timer
            auto it = m_ioMap.find(tag);
            if (it == m_ioMap.end())
            {
                LOG_WARNING("no io handler found with tag %d", tag);
                return;
            }

            std::shared_ptr<IOSource> io = it->second;

            // set the event source to off
            sd_event_source_set_enabled(io->source, SD_EVENT_OFF);

            // remove from the map
            m_ioMap.erase(it);
        };

    // if running on the eventloop thread then just call the lambda
    callOnEventLoopThread(std::move(removeIOLambda));
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Callback from the event loop when a monitored signal is detected.

 */
int EventLoopPrivate::signalHandler(sd_event_source *es,
                                    const struct signalfd_siginfo *si,
                                    void *userdata)
{
    EventLoopPrivate *self = reinterpret_cast<EventLoopPrivate*>(userdata);

    LOG_INFO("signal handler called for signal %u", si->ssi_signo);

    // find the matching event source
    auto it = self->m_signalMap.begin();
    for (; it != self->m_signalMap.end(); ++it)
    {
        if (it->second->source == es)
            break;
    }

    if (it == self->m_signalMap.end())
    {
        LOG_ERROR("failed to find callback for signal with source %p", es);
        return -1;
    }

    // get the handler
    std::shared_ptr<SignalSource> sig = it->second;
    if (sig->signalNum != (int)si->ssi_signo)
    {
        LOG_ERROR("odd, signal number in source doesn't match (%d vs %u)",
                 sig->signalNum, si->ssi_signo);
        return -1;
    }

    // call the handler
    if (sig->callback)
    {
        sig->callback();
    }

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Adds a file descriptor handler to the event loop.  The \a func will be
    called when any of the events in the \a events argument are detected.

 */
int EventLoopPrivate::addSignalHandler(int signal, std::function<void()> &&func)
{
    // tag of created signal handler
    int tag = -1;

    // create a lambda to install the timer
    std::function<void()> installSignalLambda =
        [&]()
        {
            // create the event source
            sd_event_source *source = nullptr;
            int rc = sd_event_add_signal(m_loop, &source, signal,
                                         &EventLoopPrivate::signalHandler, this);
            if ((rc < 0) || (source == nullptr))
            {
                LOG_SYS_ERROR(-rc, "failed to install signal handler");
            }
            else
            {
                // assign a new tag
                tag = m_signalTag++;

                LOG_INFO("added signal handler for signal %d", signal);

                // finally add the handler to the map
                m_signalMap.emplace(tag, std::make_shared<SignalSource>(source, std::move(func), signal));
            }
        };

    // if running on the eventloop thread then just call the lambda
    callOnEventLoopThread(std::move(installSignalLambda));

    // return the tag number set in the lambda
    return tag;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Removes a io handler previous installed with addIoHandler(...).  It is
    safe to call this from any thread or from the io callback itself.

 */
void EventLoopPrivate::removeSignalHandler(int tag)
{
    // create the lambda to do the remove
    std::function<void()> removeSignalLambda =
        [&]()
        {
            // find the timer
            auto it = m_signalMap.find(tag);
            if (it == m_signalMap.end())
            {
                LOG_WARNING("no signal handler found with tag %d", tag);
                return;
            }

            std::shared_ptr<SignalSource> sig = it->second;

            // set the event source to off
            sd_event_source_set_enabled(sig->source, SD_EVENT_OFF);

            // remove from the map
            m_signalMap.erase(it);
        };

    // if running on the eventloop thread then just call the lambda
    callOnEventLoopThread(std::move(removeSignalLambda));
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    This adds the event into the event loop and all register listeners for the
    event will be called the next time through the event loop.

    If \a post is \c true then the function returns immediately without waiting
    for the listeners to be called.  It \a post is \c false then this call will
    block until all listeners are called.  In both cases the listeners will be
    invoked from the event loop thread.

 */
void EventLoopPrivate::emitEvent(const std::shared_ptr<Event> &event, bool post)
{
    // create the lambda to invoke all listeners
    std::function<void()> callListenersLambda =
        [this, event]()
        {
            std::lock_guard<std::recursive_mutex> locker(m_listenersLock);
            m_withinEventHandler++;

            // iterate through the listeners...
            auto it = m_listenersMap.begin();
            while (it != m_listenersMap.end())
            {
                // check if the listener has been removed
                if (m_removedListenersSet.count(it->first) > 0)
                {
                    it = m_listenersMap.erase(it);
                }
                else
                {
                    // not removed so invoke is an event match
                    const EventListener &listener = it->second;
                    if ((listener.eventType < 0) ||
                        (listener.eventType == event->type()))
                    {
                        listener.callback(event);
                    }

                    // move onto next listener
                    ++it;
                }
            }

            m_withinEventHandler--;

            // if there are still some listeners to be removed then remove
            // then now
            if (!m_removedListenersSet.empty())
            {
                for (int removedTag : m_removedListenersSet)
                    m_listenersMap.erase(removedTag);

                m_removedListenersSet.clear();
            }
        };

    // put the lambda on the queue to process next idle time
    if (post)
        invokeMethod(std::move(callListenersLambda));
    else
        callOnEventLoopThread(std::move(callListenersLambda));
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Adds a listener for an event or type \a eventType, or if eventType less than
    zero then all events.

 */
int EventLoopPrivate::addEventListener(int eventType,
                                       std::function<void(const std::shared_ptr<Event>&)> &&func)
{
    std::lock_guard<std::recursive_mutex> locker(m_listenersLock);

    const int tag = m_listenersTag++;
    m_listenersMap.emplace(tag, EventListener(eventType, std::move(func)));

    return tag;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Adds a listener for an event or type \a eventType, or if eventType less than
    zero then all events.

 */
void EventLoopPrivate::removeEventListener(int tag)
{
    std::lock_guard<std::recursive_mutex> locker(m_listenersLock);

    // check if currently in an event listener callback and if so we can't
    // remove from the map as that would invalidate the map iterators in the
    // listener, so instead add the listener to a remove set.  Then when the
    // listener is finished invoking the listeners it will remove the listener
    // for us
    if (m_withinEventHandler > 0)
    {
        m_removedListenersSet.insert(tag);
    }
    else
    {
        // otherwise we can just remove the entry from the map
        m_listenersMap.erase(tag);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called when a single shot timer fires.  This uses the source pointer to
    find the function to call when this happens.

 */
int EventLoopPrivate::singleShotTimerHandler(sd_event_source *es, uint64_t usec,
                                             void *userdata)
{
    (void) usec;

    // find the callback function
    EventLoopPrivate *self = reinterpret_cast<EventLoopPrivate*>(userdata);
    auto it = self->m_singleShotTimers.find(es);
    if (it == self->m_singleShotTimers.end())
    {
        LOG_ERROR("failed to find single shot timer callback for source %p", es);
    }
    else
    {
        // call the callback
        if (it->second)
            it->second();

        // remove from the map
        self->m_singleShotTimers.erase(it);
    }

    // free the timer source now it's fired
    sd_event_source_unref(es);
    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Adds a single shot timer to the event loop.  This timer can't be cancelled
    or restarted.

 */
void EventLoopPrivate::singleShotTimer(const std::chrono::microseconds &timeout,
                                       std::function<void()> &&func)
{
    // create the lambda to invoke all listeners
    std::function<void()> installTimerLambda =
        [&]()
        {
            // calculate the expiry time
            uint64_t expiry;
            sd_event_now(m_loop, CLOCK_MONOTONIC, &expiry);
            expiry += timeout.count();

            // create and add the timer
            sd_event_source *timer = nullptr;
            int rc = sd_event_add_time(m_loop, &timer, CLOCK_MONOTONIC,
                                       expiry, 10000,
                                       EventLoopPrivate::singleShotTimerHandler, this);
            if ((rc < 0) || (timer == nullptr))
            {
                LOG_SYS_ERROR(-rc, "failed to install timer handler");
                return;
            }

            // store the callback against the event source
            m_singleShotTimers.emplace(timer, std::move(func));
        };

    // call the lambda on the event loop thread if running - if not running then
    // the lambda is called immediately
    callOnEventLoopThread(std::move(installTimerLambda));
}
