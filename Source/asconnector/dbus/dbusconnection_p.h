//
//  dbusconnection_p.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef DBUSCONNECTION_P_H
#define DBUSCONNECTION_P_H


#include "dbusmessage.h"
#include "eventloop/eventloop.h"

#include <systemd/sd-bus.h>

#include <map>
#include <string>

using namespace sky;

class DBusConnectionPrivate
{
public:
    DBusConnectionPrivate(const EventLoop &eventLoop, sd_bus *m_bus);
    ~DBusConnectionPrivate();

public:
    bool callWithCallback(DBusMessage &&message,
                          const std::function<void(DBusMessage&&)> &callback,
                          int msTimeout);

    bool send(DBusMessage &&message) const;


private:
    static int methodCallCallback(sd_bus_message *msg, void *userData,
                                  sd_bus_error *retError);

private:
    friend class DBusConnection;

    EventLoop m_eventLoop;
    sd_bus *m_bus;

    std::map<uint64_t, std::function<void(DBusMessage&&)>> m_callbacks;
};

#endif // DBUSCONNECTION_P_H
