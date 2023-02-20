//
//  asbroadcastmethodcallhelper.h
//
//  Copyright © 2020 Sky UK. All rights reserved.
//

#ifndef AS_BROADCAST_METHOD_CALL_HELPER_H_
#define AS_BROADCAST_METHOD_CALL_HELPER_H_

#include "dbus/dbusconnection.h"
#include "dbus/dbusmessage.h"
#include "sky/log.h"

#include <vector>
#include <jsoncpp/json.h>

class ASBroadcastMethodCallHelper
{

public:

    enum ErrorCode
    {
        Success = 0,
        GenericFailure = 1
    };

    static bool sendDbusBroadcastMethodCall( const DBusConnection &dbus,
                                             const std::string& name,
                                             std::vector<std::string> args,
                                             unsigned& errorCode,
                                             std::string& errorMessage )
    {
        LOG_ENTRY( "%s", __func__ );

        bool success = false;

        if (name.empty())
        {
            LOG_ERROR("%s name can not be empty", __func__);
        }
        else
        {
            const std::string request = buildBroadcastMethodCallJson( name, args );
            success = sendDbusBroadcastMethodCallRequest( dbus, request, errorCode, errorMessage );
        }

        LOG_EXIT( "%s", __func__ );
        return success;
    }

    static bool sendDbusAsyncBroadcastMethodCall( const DBusConnection &dbus,
                                                  const std::string& name, std::vector<std::string> args,
                                                  const std::function<void(DBusMessage&&)> &callback )
    {
        LOG_ENTRY( "%s", __func__ );

        bool success = false;

        if (name.empty())
        {
            LOG_ERROR("%s name can not be empty", __func__);
        }
        else
        {
            const std::string request = buildBroadcastMethodCallJson( name, args );
            success = sendDbusAsyncBroadcastMethodCallRequest( dbus, request, callback );
        }

        LOG_EXIT( "%s", __func__ );
        return success;
    }

private:

    static std::string buildBroadcastMethodCallJson( const std::string& name, std::vector<std::string> args )
    {
        LOG_ENTRY( "%s", __func__ );

        Json::Value message = {};
        message["name"] = name;

        for (auto i=0u; i<args.size(); i++)
        {
            message["args"][i] = args[i];
        }

        Json::FastWriter writer;
        std::string strMessage = writer.write( message );

        LOG_EXIT( "%s", __func__ );
        return strMessage;
    }

    static bool sendDbusBroadcastMethodCallRequest( const DBusConnection &dbus,
                                                    const std::string& message,
                                                    unsigned& errorCode,
                                                    std::string& errorMessage )
    {
        LOG_ENTRY( "%s", __func__ );

        bool success = false;
        DBusMessage dbusMessage = DBusMessage::createMethodCall( "com.sky.as.proxy",
                                                                 "/com/sky/as/service",
                                                                 "com.sky.as.Service1",
                                                                 "DBusBroadcastMethodCall" );
        dbusMessage << message;

        DBusMessage reply = dbus.call( std::move(dbusMessage) );
        if ( reply.isError() )
        {
            LOG_ERROR("%s Failed to send DBusBroadcastMethodCall: %s, errorName: %s, errorMessage: %s",
                      __func__, message.c_str(), reply.errorName().c_str(), reply.errorMessage().c_str() );
        }
        else
        {
            success = true;
            reply >> errorCode;
            reply >> errorMessage;
        }

        LOG_EXIT( "%s", __func__ );
        return success;
    }

    static bool sendDbusAsyncBroadcastMethodCallRequest( const DBusConnection &dbus,
                                                         const std::string& message,
                                                         const std::function<void(DBusMessage&&)> &callback )
    {
        LOG_ENTRY( "%s", __func__ );

        DBusMessage dbusMessage = DBusMessage::createMethodCall( "com.sky.as.proxy",
                                                                 "/com/sky/as/service",
                                                                 "com.sky.as.Service1",
                                                                 "DBusAsyncBroadcastMethodCall" );
        dbusMessage << message;
        bool success = dbus.callWithCallback( std::move(dbusMessage), callback );

        if ( !success )
        {
            LOG_ERROR("%s Failed to send DBusAsyncBroadcastMethodCall: %s", __func__, message.c_str());
        }

        LOG_EXIT( "%s", __func__ );
        return success;
    }
};

#endif /* AS_BROADCAST_METHOD_CALL_HELPER_H_ */
