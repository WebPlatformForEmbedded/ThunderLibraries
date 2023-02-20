//
//  dbusmessage.cpp
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "dbusmessage.h"
#include "dbusmessage_p.h"

#include "sky/log.h"

#include <climits>
#include <cmath>


// -----------------------------------------------------------------------------
/*!
    Creates a new method call message for sending with \a DBusConnection::call(...)
    or \a DBusConnection::callWithCallback(...).


 */
DBusMessage DBusMessage::createMethodCall(const std::string &service,
                                          const std::string &path,
                                          const std::string &interface,
                                          const std::string &method)
{
    return DBusMessage(std::make_unique<DBusMessagePrivate>(DBusMessage::MethodCallMessage,
                                                            service, path,
                                                            interface, method));
}

// -----------------------------------------------------------------------------
/*!
    Creates a new signal message for sending with \a DBusConnection::send(...)
    or \a DBusConnection::sendTo(...).


 */
DBusMessage DBusMessage::createSignal(const std::string &path,
                                      const std::string &interface,
                                      const std::string &name)
{
    return DBusMessage(std::make_unique<DBusMessagePrivate>(DBusMessage::SignalMessage,
                                                            std::string(), path,
                                                            interface, name));
}

// -----------------------------------------------------------------------------
/*!
    Constructs a new DBus message with the given \a path, \a interface and
    \a name, representing a signal emission to a specific destination.

    A DBus signal is emitted from one application and is received only by the
    application owning the destination \a service name.

    The DBusMessage object that is returned can be sent using the
    DBusConnection::send() function.

 */
DBusMessage DBusMessage::createTargetedSignal(const std::string &service,
                                              const std::string &path,
                                              const std::string &interface,
                                              const std::string &name)
{
    return DBusMessage(std::make_unique<DBusMessagePrivate>(DBusMessage::SignalMessage,
                                                            service, path,
                                                            interface, name));
}

DBusMessage DBusMessage::createIncomingSignal( sd_bus_message *reply )
{
    return DBusMessage(std::make_unique<DBusMessagePrivate>(reply));
}


// -----------------------------------------------------------------------------
/*!
    \internal

 */
DBusMessage::DBusMessage(std::unique_ptr<DBusMessagePrivate> &&priv)
    : m_private(std::move(priv))
{
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
DBusMessage::DBusMessage(DBusMessage::ErrorType error, const char *message)
    : m_private(std::make_unique<DBusMessagePrivate>(error, message))
{
}

// -----------------------------------------------------------------------------
/*!
    Constructs an invalid DBusMessage object.

 */
DBusMessage::DBusMessage()
    : m_private(nullptr)
{
}

// -----------------------------------------------------------------------------
/*!
    Move-assigns \a other to this DBusMessage.

 */
DBusMessage::DBusMessage(DBusMessage &&other) noexcept
    : m_private(std::move(other.m_private))
{
}

// -----------------------------------------------------------------------------
/*!

 */
DBusMessage::~DBusMessage()
{
    m_private.reset();
}

// -----------------------------------------------------------------------------
/*!
    Move-assigns \a other to this DBusMessage.

 */
DBusMessage& DBusMessage::operator=(DBusMessage &&rhs) noexcept
{
    m_private = std::move(rhs.m_private);
    return *this;
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if the message is valid.

    \see type()
 */
bool DBusMessage::isValid() const
{
    return !!m_private;
}

// -----------------------------------------------------------------------------
/*!
    Returns the type of the message.

    \see isValid()
 */
DBusMessage::MessageType DBusMessage::type() const
{
    return m_private->m_type;
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if the message type is DBusMessage::ErrorMessage

    \see type() and isValid()
 */
bool DBusMessage::isError() const
{
    return (m_private->m_type == ErrorMessage);
}

std::string DBusMessage::errorMessage() const
{
    return m_private->m_errorMessage;
}

std::string DBusMessage::errorName() const
{
    return m_private->m_errorName;
}

std::string DBusMessage::service() const
{
    return m_private->m_service;
}

std::string DBusMessage::path() const
{
    return m_private->m_path;
}

std::string DBusMessage::interface() const
{
    return m_private->m_interface;
}

std::string DBusMessage::member() const
{
    return m_private->m_name;
}

std::string DBusMessage::signature() const
{
    return m_private->m_signature;
}


template<typename T>
DBusMessage & DBusMessage::operator<<(const T &arg)
{
    if ((m_private->m_type == MethodCallMessage) ||
        (m_private->m_type == SignalMessage))
    {
        DBusMessagePrivate::Argument variant(arg);
        m_private->m_signature.push_back(variant.dbusType());
        m_private->m_args.emplace_back(std::move(variant));
    }
    else
    {
        LOG_WARNING("dbus message is not a method call or signal, cannot add arguments");
    }

    return *this;
}

template DBusMessage & DBusMessage::operator<< <bool>(const bool&);
template DBusMessage & DBusMessage::operator<< <int>(const int&);
template DBusMessage & DBusMessage::operator<< <unsigned>(const unsigned&);
template DBusMessage & DBusMessage::operator<< <double>(const double&);
template DBusMessage & DBusMessage::operator<< <std::string>(const std::string&);
template DBusMessage & DBusMessage::operator<< <DBusFileDescriptor>(const DBusFileDescriptor&);


template<typename T>
DBusMessage & DBusMessage::operator>>(T &arg)
{
    if ((m_private->m_type == ReplyMessage) ||(m_private->m_type == IncomingSignalMessage))
    {
        if (m_private->m_args.empty())
        {
            LOG_WARNING("no more args in dbus message");
        }
        else
        {
            const DBusMessagePrivate::Argument &value = m_private->m_args.front();
            arg = static_cast<T>(value);
            m_private->m_args.pop_front();
        }
    }
    else
    {
        LOG_WARNING("dbus message is not a method reply or a signal, cannot read arguments");
    }

    return *this;
}

template DBusMessage & DBusMessage::operator>> <bool>(bool&);
template DBusMessage & DBusMessage::operator>> <int>(int&);
template DBusMessage & DBusMessage::operator>> <unsigned>(unsigned&);
template DBusMessage & DBusMessage::operator>> <double>(double&);
template DBusMessage & DBusMessage::operator>> <std::string>(std::string&);
template DBusMessage & DBusMessage::operator>> <DBusFileDescriptor>(DBusFileDescriptor&);



const std::map<DBusMessage::ErrorType, std::string> DBusMessagePrivate::m_errorNames =
    {
        {  DBusMessage::NoError,             "" },
        {  DBusMessage::Other,               "" },
        {  DBusMessage::Failed,              "org.freedesktop.DBus.Error.Failed" },
        {  DBusMessage::NoMemory,            "org.freedesktop.DBus.Error.NoMemory" },
        {  DBusMessage::ServiceUnknown,      "org.freedesktop.DBus.Error.ServiceUnknown" },
        {  DBusMessage::NoReply,             "org.freedesktop.DBus.Error.NoReply" },
        {  DBusMessage::BadAddress,          "org.freedesktop.DBus.Error.BadAddress" },
        {  DBusMessage::NotSupported,        "org.freedesktop.DBus.Error.NotSupported" },
        {  DBusMessage::LimitsExceeded,      "org.freedesktop.DBus.Error.LimitsExceeded" },
        {  DBusMessage::AccessDenied,        "org.freedesktop.DBus.Error.AccessDenied" },
        {  DBusMessage::NoServer,            "org.freedesktop.DBus.Error.NoServer" },
        {  DBusMessage::Timeout,             "org.freedesktop.DBus.Error.Timeout" },
        {  DBusMessage::NoNetwork,           "org.freedesktop.DBus.Error.NoNetwork" },
        {  DBusMessage::AddressInUse,        "org.freedesktop.DBus.Error.AddressInUse" },
        {  DBusMessage::Disconnected,        "org.freedesktop.DBus.Error.Disconnected" },
        {  DBusMessage::InvalidArgs,         "org.freedesktop.DBus.Error.InvalidArgs" },
        {  DBusMessage::UnknownMethod,       "org.freedesktop.DBus.Error.UnknownMethod" },
        {  DBusMessage::TimedOut,            "org.freedesktop.DBus.Error.TimedOut" },
        {  DBusMessage::InvalidSignature,    "org.freedesktop.DBus.Error.InvalidSignature" },
        {  DBusMessage::UnknownInterface,    "org.freedesktop.DBus.Error.UnknownInterface" },
        {  DBusMessage::UnknownObject,       "org.freedesktop.DBus.Error.UnknownObject" },
        {  DBusMessage::UnknownProperty,     "org.freedesktop.DBus.Error.UnknownProperty" },
        {  DBusMessage::PropertyReadOnly,    "org.freedesktop.DBus.Error.PropertyReadOnly" },
        {  DBusMessage::InternalError,       "org.qtproject.QtDBus.Error.InternalError" },
        {  DBusMessage::InvalidObjectPath,   "org.qtproject.QtDBus.Error.InvalidObjectPath"  },
        {  DBusMessage::InvalidService,      "org.qtproject.QtDBus.Error.InvalidService"     },
        {  DBusMessage::InvalidMember,       "org.qtproject.QtDBus.Error.InvalidMember" },
        {  DBusMessage::InvalidInterface,    "org.qtproject.QtDBus.Error.InvalidInterface" }

    };


// -----------------------------------------------------------------------------
/*!
    Creates a new message object.

 */
DBusMessagePrivate::DBusMessagePrivate(DBusMessage::MessageType type,
                                       const std::string &service,
                                       const std::string &path,
                                       const std::string &interface,
                                       const std::string &method)
    : m_type(type)
    , m_service(service)
    , m_path(path)
    , m_interface(interface)
    , m_name(method)
{
    // the signature is dynamically updated as args are marshalled into the
    // message object ... anyway reserve some space for the arg signature
    m_signature.reserve(8);
}

// -----------------------------------------------------------------------------
/*!
    Constructs an error message type, for cases when something went wrong
    outside of the sd-bus code.

 */
DBusMessagePrivate::DBusMessagePrivate(DBusMessage::ErrorType error,
                                       const char *message)
    : m_type(DBusMessage::ErrorMessage)
    , m_errorName(m_errorNames.at(error))
    , m_errorMessage(message ? message : "")
{
}

// -----------------------------------------------------------------------------
/*!
    Constructs an error message type, getting the error details from the sd-bus
    \a error object.

 */
DBusMessagePrivate::DBusMessagePrivate(sd_bus_error *error)
    : m_type(DBusMessage::ErrorMessage)
    , m_errorName((error && error->name) ? error->name : "")
    , m_errorMessage((error && error->message) ? error->message : "")
{
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Helper to convert the message type.

 */
static DBusMessage::MessageType getMessageType(sd_bus_message *reply)
{
    uint8_t type;

    int rc = sd_bus_message_get_type(reply, &type);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to get message type");
        return DBusMessage::InvalidMessage;
    }

    switch (type)
    {
        case SD_BUS_MESSAGE_METHOD_RETURN:
            return DBusMessage::ReplyMessage;
        case SD_BUS_MESSAGE_SIGNAL:
            return DBusMessage::IncomingSignalMessage;
        case SD_BUS_MESSAGE_METHOD_ERROR:
            return DBusMessage::ErrorMessage;
        default:
            LOG_ERROR("unexpected message type %hu", type);
            return DBusMessage::InvalidMessage;
    }
}

// -----------------------------------------------------------------------------
/*!
    Creates either a new reply or error message from the sd-bus message
    object.

 */
DBusMessagePrivate::DBusMessagePrivate(sd_bus_message *reply)
    : m_type(getMessageType(reply))
    , m_service(sd_bus_message_get_sender(reply))
{

    // if an error message extract the name and message
    if (m_type == DBusMessage::ErrorMessage)
    {
        const sd_bus_error *error = sd_bus_message_get_error(reply);
        m_errorName = error->name;
        m_errorMessage = error->message;
    }

    // if a reply then try and read any args
    if ((m_type == DBusMessage::ReplyMessage) || (m_type == DBusMessage::IncomingSignalMessage))
    {
        const char *path = sd_bus_message_get_path(reply);
        if (path)
            m_path.assign(path);

        const char *iface = sd_bus_message_get_interface(reply);
        if (iface)
            m_interface.assign(iface);

        const char *sig = sd_bus_message_get_signature(reply, 1);
        if (sig)
            m_signature.assign(sig);

        demarshallArgs(reply);
    }

}

// -----------------------------------------------------------------------------
/*!
    \internal

    Attempts to read the arguments we can from the message object.

 */
bool DBusMessagePrivate::demarshallArgs(sd_bus_message *msg)
{
    // loop through all args
    while (!sd_bus_message_at_end(msg, false))
    {
        // get the argument type
        char type = '\0';
        const char *content = nullptr;
        int rc = sd_bus_message_peek_type(msg, &type, &content);
        if (rc < 0)
        {
            LOG_SYS_WARNING(-rc, "failed to get reply message arg");
            return false;
        }

        // process the argument type
        switch (type)
        {
            case SD_BUS_TYPE_BOOLEAN:
            {
                int32_t value;
                rc = sd_bus_message_read_basic(msg, type, &value);
                if (rc >= 0)
                    m_args.emplace_back(Argument(static_cast<bool>(value)));
                break;
            }
            case SD_BUS_TYPE_INT32:
            {
                int32_t value;
                rc = sd_bus_message_read_basic(msg, type, &value);
                if (rc >= 0)
                    m_args.emplace_back(Argument(static_cast<int>(value)));
                break;
            }
            case SD_BUS_TYPE_UINT32:
            {
                uint32_t value;
                rc = sd_bus_message_read_basic(msg, type, &value);
                if (rc >= 0)
                    m_args.emplace_back(Argument(static_cast<unsigned>(value)));
                break;
            }
            case SD_BUS_TYPE_DOUBLE:
            {
                double value;
                rc = sd_bus_message_read_basic(msg, type, &value);
                if (rc >= 0)
                    m_args.emplace_back(Argument(value));
                break;
            }
            case SD_BUS_TYPE_STRING:
            {
                const char *value = nullptr;
                rc = sd_bus_message_read_basic(msg, type, &value);
                if ((rc >= 0) && value)
                    m_args.emplace_back(Argument(value));
                break;
            }
            case SD_BUS_TYPE_UNIX_FD:
            {
                int value = -1;
                rc = sd_bus_message_read_basic(msg, type, &value);
                if (rc >= 0)
                    m_args.emplace_back(Argument(DBusFileDescriptor(value)));
                break;
            }

            case SD_BUS_TYPE_ARRAY:
            case SD_BUS_TYPE_STRUCT:
            {
                std::string types;
                types += type;
                if (content)
                    types += content;

                LOG_WARNING("received message with unsupported array or struct args, skipping");
                rc = sd_bus_message_skip(msg, types.c_str());
                break;
            }

            default:
            {
                std::string types;
                types += type;

                LOG_WARNING("received message with unsupported args of type '%c' (%s), "
                           "skipping", type, content);
                rc = sd_bus_message_skip(msg, types.c_str());
                break;
            }
        }

        // check for any error skipping or reading args
        if (rc < 0)
        {
            LOG_SYS_WARNING(-rc, "failed to read / skip message arguments");
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    Constructs a sd_bus_message object for either a method call or signal type.

 */
DBusMessagePrivate::sd_bus_message_ptr DBusMessagePrivate::toMessage(sd_bus *bus) const
{
    int rc;

    // create a new method call or signal
    sd_bus_message *msg = nullptr;
    if (m_type == DBusMessage::MethodCallMessage)
    {
        rc = sd_bus_message_new_method_call(bus, &msg, m_service.c_str(),
                                            m_path.c_str(), m_interface.c_str(),
                                            m_name.c_str());
    }
    else if (m_type == DBusMessage::SignalMessage)
    {
        rc = sd_bus_message_new_signal(bus, &msg,
                                       m_path.c_str(), m_interface.c_str(),
                                       m_name.c_str());
    }
    else
    {
        LOG_ERROR("invalid message type");
        return sd_bus_message_ptr(nullptr, sd_bus_message_unref);
    }

    // check we created the message
    if ((rc < 0) || (msg == nullptr))
    {
        LOG_SYS_ERROR(-rc, "failed to create method call message");
        return sd_bus_message_ptr(nullptr, sd_bus_message_unref);
    }

    // then populate with all the args
    for (const Argument &arg : m_args)
    {
        switch (arg.type)
        {
            case Argument::Boolean:
                rc = sd_bus_message_append_basic(msg, SD_BUS_TYPE_BOOLEAN,
                                                 &arg.basic.boolean);
                break;
            case Argument::Integer:
                rc = sd_bus_message_append_basic(msg, SD_BUS_TYPE_INT32,
                                                 &arg.basic.integer);
                break;
            case Argument::UnsignedInteger:
                rc = sd_bus_message_append_basic(msg, SD_BUS_TYPE_UINT32,
                                                 &arg.basic.uinteger);
                break;
            case Argument::Double:
                rc = sd_bus_message_append_basic(msg, SD_BUS_TYPE_DOUBLE,
                                                 &arg.basic.real);
                break;
            case Argument::String:
                rc = sd_bus_message_append_basic(msg, SD_BUS_TYPE_STRING,
                                                 arg.str.c_str());
                break;
            case Argument::FileDescriptor:
            {
                int descriptor = arg.fd.fd();
                rc = sd_bus_message_append_basic(msg, SD_BUS_TYPE_UNIX_FD,
                                                 &descriptor);
            }
                break;
        }

        // check for any errors
        if (rc < 0)
        {
            LOG_SYS_WARNING(-rc, "failed to marshall the method call arguments");
            sd_bus_message_unref(msg);
            return sd_bus_message_ptr(nullptr, sd_bus_message_unref);
        }
    }

    // return the populated message
    return sd_bus_message_ptr(msg, sd_bus_message_unref);
}


// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the dbus character type that matches the stored type.

 */
char DBusMessagePrivate::Argument::dbusType() const
{
    switch (type)
    {
        case Argument::Boolean:
            return 'b';
        case Argument::Integer:
            return 'i';
        case Argument::UnsignedInteger:
            return 'u';
        case Argument::Double:
            return 'd';
        case Argument::String:
            return 's';
        case Argument::FileDescriptor:
            return 'h';
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Cast the variant to the return type.

 */
DBusMessagePrivate::Argument::operator bool() const
{
    if (type != Boolean)
    {
        LOG_WARNING("argument type is not boolean");
        return false;
    }

    return basic.boolean;
}

DBusMessagePrivate::Argument::operator int() const
{
    if (type != Integer)
    {
        LOG_WARNING("argument type is not integer");
        return INT32_MAX;
    }

    return basic.integer;
}

DBusMessagePrivate::Argument::operator unsigned() const
{
    if (type != UnsignedInteger)
    {
        LOG_WARNING("argument type is not unsigned integer");
        return UINT32_MAX;
    }

    return basic.uinteger;
}

DBusMessagePrivate::Argument::operator double() const
{
    if (type != Double)
    {
        LOG_WARNING("argument type is not double");
        return std::nan("");
    }

    return basic.real;
}

DBusMessagePrivate::Argument::operator std::string() const
{
    if (type != String)
    {
        LOG_WARNING("argument type is not string");
        return std::string();
    }

    return str;
}

DBusMessagePrivate::Argument::operator DBusFileDescriptor() const
{
    if (type != FileDescriptor)
    {
        LOG_WARNING("argument type is not file descriptor");
        return DBusFileDescriptor();
    }

    return fd;
}


