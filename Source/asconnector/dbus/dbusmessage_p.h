//
//  dbusmessage_p.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef DBUSMESSAGE_P_H
#define DBUSMESSAGE_P_H

#include "dbusmessage.h"
#include "dbusfiledescriptor.h"

#include <systemd/sd-bus.h>

#include <map>
#include <list>
#include <string>
#include <memory>


class DBusMessagePrivate
{

public:
    DBusMessagePrivate(DBusMessage::MessageType type,
                       const std::string &service,
                       const std::string &path,
                       const std::string &interface,
                       const std::string &method);

    DBusMessagePrivate(DBusMessage::ErrorType error,
                       const char *message);

    explicit DBusMessagePrivate(sd_bus_message *reply);
    explicit DBusMessagePrivate(sd_bus_error *error);

    ~DBusMessagePrivate() = default;

public:
    struct sd_bus_message_deleter {
        void operator()(sd_bus_message* msg) const {
            sd_bus_message_unref(msg);
        }
    };

    using sd_bus_message_ptr = std::unique_ptr<sd_bus_message, sd_bus_message*(*)(sd_bus_message*)>;
    sd_bus_message_ptr toMessage(sd_bus *bus) const;

    const DBusMessage::MessageType m_type;
    const std::string m_service;

private:
    bool demarshallArgs(sd_bus_message *reply);

private:
    friend DBusMessage;

    std::string m_path;
    std::string m_interface;
    std::string m_name;

    std::string m_signature;

    std::string m_errorName;
    std::string m_errorMessage;

private:
    struct Argument
    {
        enum Type { Boolean, Integer, UnsignedInteger, Double, FileDescriptor, String } type;
        union BasicType
        {
            bool boolean;
            int integer;
            unsigned uinteger;
            double real;
            BasicType()                    : real(0.0f)  { }
            explicit BasicType(bool b)     : boolean(b)  { }
            explicit BasicType(int i)      : integer(i)  { }
            explicit BasicType(unsigned u) : uinteger(u) { }
            explicit BasicType(double d)   : real(d)     { }
        } basic;
        std::string str;
        DBusFileDescriptor fd;

        explicit Argument(bool b)                  : type(Boolean),         basic(b) { }
        explicit Argument(int i)                   : type(Integer),         basic(i) { }
        explicit Argument(unsigned u)              : type(UnsignedInteger), basic(u) { }
        explicit Argument(double d)                : type(Double),          basic(d) { }

        explicit Argument(const char *str_)        : type(String),  str(str_) { }
        explicit Argument(const std::string &str_) : type(String),  str(str_) { }
        explicit Argument(std::string &&str_)      : type(String),  str(std::move(str_)) { }

        explicit Argument(DBusFileDescriptor fd)   : type(FileDescriptor),  fd(fd) { }

        explicit operator bool() const;
        explicit operator int() const;
        explicit operator unsigned() const;
        explicit operator double() const;
        explicit operator std::string() const;
        explicit operator DBusFileDescriptor() const;

        char dbusType() const;
    };

    std::list<Argument> m_args;

private:
    static const std::map<DBusMessage::ErrorType, std::string> m_errorNames;
};


#endif // DBUSMESSAGE_P_H
