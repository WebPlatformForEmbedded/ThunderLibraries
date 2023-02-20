//
//  dbusmessage.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef DBUSMESSAGE_H
#define DBUSMESSAGE_H

#include "dbusfiledescriptor.h"

#include <systemd/sd-bus.h>

#include <string>
#include <memory>

typedef sd_bus_message sd_bus_message;

class DBusMessagePrivate;

class DBusMessage
{

public:
    static DBusMessage createMethodCall(const std::string &service,
                                        const std::string &path,
                                        const std::string &interface,
                                        const std::string &method);

    static DBusMessage createSignal(const std::string &path,
                                    const std::string &interface,
                                    const std::string &name);

    static DBusMessage createTargetedSignal(const std::string &service,
                                            const std::string &path,
                                            const std::string &interface,
                                            const std::string &name);

    static DBusMessage createIncomingSignal( sd_bus_message *reply );

public:
    DBusMessage();
    DBusMessage(const DBusMessage &other) = delete;
    DBusMessage(DBusMessage &&other) noexcept;
    ~DBusMessage();

    DBusMessage& operator= (const DBusMessage &rhs) = delete;
    DBusMessage& operator= (DBusMessage &&rhs) noexcept;


public:
    enum MessageType
    {
        MethodCallMessage,
        SignalMessage,
        ReplyMessage,
        IncomingSignalMessage,
        ErrorMessage,
        InvalidMessage
    };

    enum ErrorType
    {
        NoError,
        Other,
        Failed,
        NoMemory,
        ServiceUnknown,
        NoReply,
        BadAddress,
        NotSupported,
        LimitsExceeded,
        AccessDenied,
        NoServer,
        Timeout,
        NoNetwork,
        AddressInUse,
        Disconnected,
        InvalidArgs,
        UnknownMethod,
        TimedOut,
        InvalidSignature,
        UnknownInterface,
        UnknownObject,
        UnknownProperty,
        PropertyReadOnly,
        InternalError,
        InvalidObjectPath,
        InvalidService,
        InvalidMember,
        InvalidInterface,
    };

    bool isValid() const;

public:
    MessageType type() const;

    bool isError() const;
    std::string errorMessage() const;
    std::string errorName() const;

    std::string service() const;
    std::string path() const;
    std::string interface() const;
    std::string member() const;
    std::string signature() const;

public:
    template<typename T>
    DBusMessage & operator<<(const T &arg);

    template<typename T>
    DBusMessage & operator>>(T &arg);

private:
    friend class DBusConnection;
    friend class DBusConnectionPrivate;
    explicit DBusMessage(std::unique_ptr<DBusMessagePrivate> &&priv);
    explicit DBusMessage(DBusMessage::ErrorType error, const char *message = nullptr);

    std::shared_ptr<DBusMessagePrivate> m_private;
};


#endif // DBUSMESSAGE_H
