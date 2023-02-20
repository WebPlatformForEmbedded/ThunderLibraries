//
//  dbusconnection.cpp
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "dbusconnection.h"
#include "dbusconnection_p.h"
#include "dbusmessage.h"
#include "dbusmessage_p.h"
#include "eventloop/eventloop.h"
#include "sky/log.h"

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#include <cassert>
#include <cerrno>

#include <semaphore.h>


#define DBUS_DEFAULT_TIMEOUT_USEC    (25 * 1000 * 1000)


// -----------------------------------------------------------------------------
/*!
    \internal

 */
DBusConnection::DBusConnection()
{
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
DBusConnection::DBusConnection(std::shared_ptr<DBusConnectionPrivate> &&priv)
    : m_private(std::move(priv))
{
}

// -----------------------------------------------------------------------------
/*!


 */
DBusConnection::DBusConnection(const DBusConnection &other)
    : m_private(other.m_private)
{
}

// -----------------------------------------------------------------------------
/*!


 */
DBusConnection::DBusConnection(DBusConnection &&other)
    : m_private(std::move(other.m_private))
{
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
DBusConnectionPrivate::DBusConnectionPrivate(const EventLoop &eventLoop, sd_bus *bus)
    : m_eventLoop(eventLoop)
    , m_bus(bus)
{
}

// -----------------------------------------------------------------------------
/*!


 */
DBusConnectionPrivate::~DBusConnectionPrivate()
{
    // flush out any queued executors in the event loop
    m_eventLoop.flush();

    // free the bus object
    sd_bus_flush_close_unref(m_bus);
    m_bus = nullptr;
}

// -----------------------------------------------------------------------------
/*!


 */
DBusConnection DBusConnection::systemBus(const EventLoop &eventLoop)
{
    sd_bus *bus = nullptr;

    int rc = sd_bus_open_system(&bus);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to connect to system bus");
        return DBusConnection(nullptr);
    }

    rc = sd_bus_attach_event(bus, eventLoop.handle(), SD_EVENT_PRIORITY_NORMAL);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to attach bus to event loop");
        sd_bus_unref(bus);
        return DBusConnection(nullptr);
    }

    return DBusConnection(std::make_shared<DBusConnectionPrivate>(eventLoop, bus));
}

// -----------------------------------------------------------------------------
/*!


 */
DBusConnection DBusConnection::sessionBus(const EventLoop &eventLoop)
{
    sd_bus *bus = nullptr;

    int rc = sd_bus_open_user(&bus);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to connect to session bus");
        return DBusConnection(nullptr);
    }

    rc = sd_bus_attach_event(bus, eventLoop.handle(), SD_EVENT_PRIORITY_NORMAL);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to attach bus to event loop");
        sd_bus_unref(bus);
        return DBusConnection(nullptr);
    }

    return DBusConnection(std::make_shared<DBusConnectionPrivate>(eventLoop, bus));
}

// -----------------------------------------------------------------------------
/*!


 */
DBusConnection DBusConnection::connectToBus(const EventLoop &eventLoop,
                                            const std::string &address)
{
    (void)eventLoop;
    (void)address;

    // TODO:
    return DBusConnection(nullptr);
}

// -----------------------------------------------------------------------------
/*!


 */
sd_bus* DBusConnection::handle() const
{
    if (!m_private)
        return nullptr;
    else
        return m_private->m_bus;
}

// -----------------------------------------------------------------------------
/*!


 */
bool DBusConnection::isConnected() const
{
    return m_private && m_private->m_bus;
}

// -----------------------------------------------------------------------------
/*!


 */
EventLoop DBusConnection::eventLoop() const
{
    if (!m_private)
        return EventLoop();
    else
        return m_private->m_eventLoop;
}

// -----------------------------------------------------------------------------
/*!


 */
bool DBusConnection::registerName(const std::string &name)
{
    if (!m_private || !m_private->m_bus)
    {
        return false;
    }

    // request the given name
    int rc = sd_bus_request_name(m_private->m_bus, name.c_str(), 0);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to acquire service name");
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Sends the message over this connection and blocks, waiting for a reply,
    for at most \a msTimeout milliseconds. This function is suitable for method
    calls only. It returns the reply message as its return value, which will be
    either of type DBusMessage::ReplyMessage or DBusMessage::ErrorMessage.

    If no reply is received within \a msTimeout milliseconds, an automatic error
    will be delivered indicating the expiration of the call. The default timeout
    is \c -1, which will be replaced with an implementation-defined value that
    is suitable for inter-process communications (generally, 25 seconds).

    It is safe to call this function from any thread.  If called from the event
    loop thread it will block the event loop until complete, otherwise it
    will post a message to the event loop and send the dbus message within
    that.

 */
DBusMessage DBusConnection::call(DBusMessage &&message, int msTimeout) const
{
    // sanity check the message
    if (message.type() != DBusMessage::MethodCallMessage)
    {
        LOG_WARNING("trying to call with non-method call message");
        return DBusMessage(DBusMessage::Failed);
    }

    // get the dets
    DBusConnectionPrivate *priv = m_private.get();
    if (!priv || !priv->m_bus)
    {
        LOG_WARNING("dbus not connected");
        return DBusMessage(DBusMessage::NoNetwork);
    }

    // check if being called from the event loop thread
    if (priv->m_eventLoop.onEventLoopThread())
    {
        // convert the timeout to sd-bus usec format
        uint64_t timeout;
        if (msTimeout >= 0)
            timeout = msTimeout * 1000;
        else
            timeout = DBUS_DEFAULT_TIMEOUT_USEC;

        // construct the request
        auto msg = message.m_private->toMessage(priv->m_bus);
        if (!msg)
            return DBusMessage(DBusMessage::Failed);

        // will store error details on return
        sd_bus_error error;
        bzero(&error, sizeof(error));

        // make the call
        sd_bus_message *reply = nullptr;
        int rc = sd_bus_call(priv->m_bus, msg.get(), timeout, &error, &reply);
        if ((rc < 0) || !reply)
        {
            // LOG_SYS_WARNING(-rc, "dbus call failed due to '%s' - '%s'",
            //               error.name, error.message);

            DBusMessage errorMsg(std::make_unique<DBusMessagePrivate>(&error));
            sd_bus_error_free(&error);
            return errorMsg;
        }

        // construct the reply
        DBusMessage replyMsg(std::make_unique<DBusMessagePrivate>(reply));
        sd_bus_message_unref(reply);
        sd_bus_error_free(&error);

        // return the result
        return replyMsg;
    }


    // otherwise we're not running on the dbus / event loop thread so use the
    // async version and block on the callback

    DBusMessage replyMsg;

    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    std::function<void(DBusMessage)> lambda =
        [&](DBusMessage &&reply)
        {
            assert(priv->m_eventLoop.onEventLoopThread());

            // move the reply to our local copy
            replyMsg = std::move(reply);

            // wake the blocked thread
            if (sem_post(&semaphore) != 0)
                LOG_SYS_FATAL(errno, "failed to release semaphore");
        };

    // make the dbus call
    if (!m_private->callWithCallback(std::move(message), lambda, msTimeout))
    {
        replyMsg = DBusMessage(DBusMessage::Failed);
    }

    // wait for the callback to be called
    else if (sem_wait(&semaphore) != 0)
    {
        LOG_SYS_FATAL(errno, "failed to wait on semaphore");
        replyMsg = DBusMessage(DBusMessage::Failed);
    }

    sem_destroy(&semaphore);

    return replyMsg;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Sends the \a message over this connection and returns immediately. When the
    reply is received, the method returnMethod is called in the receiver object.

    If no reply is received within \a msTimeout milliseconds, an automatic error
    will be delivered indicating the expiration of the call. The default
    timeout is -1, which will be replaced with an implementation-defined value
    that is suitable for inter-process communications (generally, 25 seconds).

    This function is suitable for method calls only. It is guaranteed that the
    \a callback will be called exactly once with the reply, or if an error
    occurs an error message.

    Returns \c true if the message was sent, or \c false if the message could
    not be sent (in which case the callback won't be called).

 */
bool DBusConnection::callWithCallback(DBusMessage &&message,
                                      const std::function<void(DBusMessage&&)> &callback,
                                      int msTimeout) const
{
    // sanity check the message type
    if (message.type() != DBusMessage::MethodCallMessage)
    {
        LOG_WARNING("trying to call with non-method call message");
        return false;
    }

    // then check we're connected
    DBusConnectionPrivate *priv = m_private.get();
    if (!priv || !priv->m_bus)
    {
        LOG_WARNING("not connected to bus");
        return false;
    }

    return priv->callWithCallback(std::move(message), callback, msTimeout);
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
int DBusConnectionPrivate::methodCallCallback(sd_bus_message *reply,
                                              void *userData,
                                              sd_bus_error *retError)
{
    DBusConnectionPrivate *self = reinterpret_cast<DBusConnectionPrivate*>(userData);
    assert(self->m_eventLoop.onEventLoopThread());

    // get the cookie to find the correct callback
    uint64_t cookie;
    int rc = sd_bus_message_get_reply_cookie(reply, &cookie);
    if (rc < 0)
    {
        LOG_SYS_FATAL(-rc, "failed to get cookie of reply message");
        return 0;
    }

    // nb: we don't need to take any locking for this as we know we'll only
    // be called from the dbus event loop thread
    auto it = self->m_callbacks.find(cookie);
    if (it == self->m_callbacks.end())
    {
        LOG_FATAL("failed to find callback for cookie %" PRIu64, cookie);
        return 0;
    }

    // get a copy of the callback and erase from the map
    std::function<void(DBusMessage&&)> fn = it->second;
    self->m_callbacks.erase(it);

    // call the callback
    if (fn)
        fn(DBusMessage(std::make_unique<DBusMessagePrivate>(reply)));

    // done
    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
bool DBusConnectionPrivate::callWithCallback(DBusMessage &&message,
                                             const std::function<void(DBusMessage&&)> &callback,
                                             int msTimeout)
{
    // convert the timeout to sd-bus usec format
    uint64_t timeout;
    if (msTimeout >= 0)
        timeout = msTimeout * 1000;
    else
        timeout = DBUS_DEFAULT_TIMEOUT_USEC;


    // if not running on the event loop thread then any errors that occur before
    // sending the message are also sent to the callback
    std::function<void(DBusMessage&&)> errorCallback;
    if (!m_eventLoop.onEventLoopThread())
        errorCallback = callback;


    // take the message data
    std::shared_ptr<DBusMessagePrivate> messageData = message.m_private;
    message.m_private.reset();


    // create a lambda to make the call
    auto execCall =
        [this, messageData, callback, errorCallback, timeout]()
        {
            assert(m_eventLoop.onEventLoopThread());

            // construct the request
            auto msg = messageData->toMessage(m_bus);
            if (!msg)
            {
                if (errorCallback)
                    errorCallback(DBusMessage(DBusMessage::Failed));
                return false;
            }

            // make the call
            int rc = sd_bus_call_async(m_bus, nullptr, msg.get(),
                                       &DBusConnectionPrivate::methodCallCallback,
                                       this, timeout);
            if (rc < 0)
            {
                LOG_SYS_WARNING(-rc, "dbus call failed");
                if (errorCallback)
                    errorCallback(DBusMessage(DBusMessage::Failed));
                return  false;
            }

            // get the cookie
            uint64_t cookie = 0;
            rc = sd_bus_message_get_cookie(msg.get(), &cookie);
            if (rc < 0)
            {
                LOG_SYS_ERROR(-rc, "failed to get request message cookie");
                if (errorCallback)
                    errorCallback(DBusMessage(DBusMessage::Failed));
                return  false;
            }

            // add the callback to the map against the cookie
            m_callbacks.emplace(cookie, callback);

            // return the result
            return  true;
        };

    // if running on the main event loop thread then just make the call,
    // otherwise the schedule the call on the event loop thread
    if (m_eventLoop.onEventLoopThread())
    {
        return execCall();
    }
    else
    {
        return m_eventLoop.invokeMethod(std::move(execCall));
    }
}

// -----------------------------------------------------------------------------
/*!
    Sends the \a message over this connection, without waiting for a reply.
    This is suitable only for signals and method calls whose return values are
    not necessary.

    Returns \c true if the message was queued successfully, \c false otherwise.

 */
bool DBusConnection::send(DBusMessage &&message) const
{
    // sanity check the message type
    if ((message.type() != DBusMessage::MethodCallMessage) &&
        (message.type() != DBusMessage::SignalMessage))
    {
        LOG_WARNING("trying to call with non-method call or signal message");
        return false;
    }

    // then check we're connected
    DBusConnectionPrivate *priv = m_private.get();
    if (!priv || !priv->m_bus)
    {
        LOG_WARNING("dbus not connected");
        return false;
    }

    return priv->send(std::move(message));
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
bool DBusConnectionPrivate::send(DBusMessage &&message) const
{
    // take the message data
    std::shared_ptr<DBusMessagePrivate> messageData = message.m_private;
    message.m_private.reset();

    // create a lambda to do the actual sending of the message
    auto sendMessageLambda =
        [this, messageData]() mutable
        {
            assert(m_eventLoop.onEventLoopThread());

            // construct the request
            auto msg = messageData->toMessage(m_bus);
            if (!msg)
                return false;

            // make the call
            int rc;
            if (messageData->m_type == DBusMessage::SignalMessage)
            {
                if (messageData->m_service.empty())
                    rc = sd_bus_send(m_bus, msg.get(), nullptr);
                else
                    rc = sd_bus_send_to(m_bus, msg.get(), messageData->m_service.c_str(), nullptr);
            }
            else if (messageData->m_type == DBusMessage::MethodCallMessage)
            {
                sd_bus_message_set_expect_reply(msg.get(), false);
                rc = sd_bus_send(m_bus, msg.get(), nullptr);
            }
            else
            {
                return false;
            }

            if (rc < 0)
            {
                LOG_SYS_WARNING(-rc, "dbus call failed");
                return false;
            }

            // return the result
            return true;
        };

    if (m_eventLoop.onEventLoopThread())
    {
        // being called from the event loop thread so just execute the lambda
        return sendMessageLambda();
    }
    else
    {
        // otherwise queue on the event loop thread
        return m_eventLoop.invokeMethod(std::move(sendMessageLambda));
    }
}
