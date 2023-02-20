//
//  dbusconnection.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef DBUSCONNECTION_H
#define DBUSCONNECTION_H

#include "eventloop/eventloop.h"

#include <string>


typedef struct sd_bus sd_bus;


class DBusMessage;
class DBusConnectionPrivate;

using namespace sky;

class DBusConnection
{
public:
    DBusConnection(const DBusConnection &other);
    DBusConnection(DBusConnection &&other);
    ~DBusConnection() = default;

    // DBusConnection & operator=(DBusConnection &&other);
    // DBusConnection & operator=(const DBusConnection &other);

    static DBusConnection systemBus(const EventLoop &eventLoop);
    static DBusConnection sessionBus(const EventLoop &eventLoop);
    static DBusConnection connectToBus(const EventLoop &eventLoop,
                                       const std::string &address);

public:
    sd_bus *handle() const;
    EventLoop eventLoop() const;

    bool isConnected() const;

    bool registerName(const std::string &name);


public:
    DBusMessage call(DBusMessage &&message, int msTimeout = -1) const;
    bool callWithCallback(DBusMessage &&message,
                          const std::function<void(DBusMessage&&)> &callback,
                          int msTimeout = -1) const;

    bool send(DBusMessage &&message) const;


private:
    DBusConnection();
    DBusConnection(std::shared_ptr<DBusConnectionPrivate> &&priv);
    std::shared_ptr<DBusConnectionPrivate> m_private;

};

#endif // DBUSCONNECTION_H
