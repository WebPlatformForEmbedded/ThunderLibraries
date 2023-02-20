//
//  asservice.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef ASSERVICE_H
#define ASSERVICE_H

#include "asvariantmap.h"

#include <string>
#include <memory>


class ASRequest;
class ASServicePrivate;
class DBusConnection;
class DBusMessage;

class ASService
{
public:
    ASService(const DBusConnection &dbusConn,
              const std::string &configJson);
    virtual ~ASService();

public:
    DBusConnection connection() const;

protected:
    virtual void onRequest(const ASRequest &request);

    virtual std::string systemInfo();
    virtual std::string getAsPreference(const std::string &name);

    virtual std::string getSystemSetting(const std::string &name);
    virtual bool setSystemSetting(const std::string &name, const std::string &value);

    virtual std::string getSystemTime();
    virtual std::string getSystemInputs();
    virtual std::string getSystemEntitlements();

    virtual std::string getTestPreference(const std::string &name);
    virtual bool setTestPreference(const std::string &name, const std::string &value, int pin);

public:
    void updateWebSocket(const std::string &wsUrl, const std::string &wsMessage);
    void updateHttpUrl(const std::string &httpUrl, int64_t tag);
    void updateSystemStatus(const std::map<std::string, ASVariantMap>& systemStatusMessage );

    typedef std::function<void(int matchTag, DBusMessage &&message)> SignalCallback;
    int registerForSignal( const std::string& service,
                           const std::string& path,
                           const std::string& interface,
                           const std::string& signalName,
                           const std::string& extraArgs,
                           const ASService::SignalCallback& signalCallback );
    int unregisterForSignal( int tag );

private:
    friend ASServicePrivate;
    std::shared_ptr<ASServicePrivate> m_private;
};


#endif // ASSERVICE_H
