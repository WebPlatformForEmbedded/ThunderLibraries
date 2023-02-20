//
//  asservice_p.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef ASSERVICE_P_H
#define ASSERVICE_P_H

#include "dbus/dbusconnection.h"

#include <systemd/sd-bus.h>

#include <map>
#include <unordered_map>
#include <string>
#include <functional>


class ASService;

class ASServicePrivate
{
public:
    ASServicePrivate(const DBusConnection &dbusConn,
                     const std::string &configJson,
                     ASService *parent);
    ~ASServicePrivate();

public:
    DBusConnection dbusConn() const;

    void updateWebSocket(const std::string &wsUrl, const std::string &wsMessage);
    void updateHttpUrl(const std::string &httpUrl, int64_t tag);
    void updateSystemStatus( const std::map<std::string, ASVariantMap>& systemStatusUpdate );
    int registerForSignal( const std::string& service,
                        const std::string& path,
                        const std::string& interface,
                        const std::string& signalName,
                        const std::string& extraArgs,
                        const ASService::SignalCallback& signalCallback );
    int unregisterForSignal( int tag );

public:
    static int config(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int request(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int GetDiagContexts(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int SetDiagContexts(sd_bus_message *msg, void *userData, sd_bus_error *retError);

    static int systemInfo(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int getSystemSetting(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int setSystemSetting(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int getTestPreference(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int setTestPreference(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int getAsPreference(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int getSystemTime(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int getSystemInputs(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int getSystemEntitlements(sd_bus_message *msg, void *userData, sd_bus_error *retError);

    static int registerWsListener(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int unregisterWsListener(sd_bus_message *msg, void *userData, sd_bus_error *retError);

    static int registerSystemStatusListener(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int unregisterSystemStatusListener(sd_bus_message *msg, void *userData, sd_bus_error *retError);

    static int registerHttpListener(sd_bus_message *msg, void *userData, sd_bus_error *retError);
    static int unregisterHttpListener(sd_bus_message *msg, void *userData, sd_bus_error *retError);

    static int trackerHandler(sd_bus_track *track, void *userdata);
    static int onRuleMatch(sd_bus_message *msg, void *userData, void *retError);

private:
    typedef std::function<void(ASServicePrivate*,
                               const std::string&,
                               const std::string&)> UpdaterFunc;

    int registerListener(sd_bus_message *msg,
                         std::multimap<std::string, std::string> *listenerMap,
                         const UpdaterFunc &updateFunc, const std::string& defaultUrl = std::string());
    int unregisterListener(sd_bus_message *msg,
                           std::multimap<std::string, std::string> *listenerMap, const std::string& defaultUrl = std::string());

private:
    ASService * const m_parent;
    const DBusConnection m_dbusConn;
    const std::string m_configJson;
    const std::string m_objectPath;
    const std::string m_interface;

    sd_bus_slot *m_slot;
    sd_bus_track *m_tracker;

private:
    void sendWsUpdateTo(const std::string &service, const std::string &wsUrl,
                        const std::string &message);
    void sendCachedWsUpdateTo(const std::string &service,
                              const std::string &wsUrl);
    void sendSystemStatusUpdateTo(const std::string &service, const std::map<std::string, ASVariantMap>& systemStatusUpdate);
    void sendCachedSystemStatusUpdateTo(const std::string &service, const std::string &wsUrl);

    void sendHttpUpdateTo(const std::string &service, const std::string &httpUrl,
                          int64_t tag);
    void sendCachedHttpUpdateTo(const std::string &service,
                                const std::string &httpUrl);


    std::map<std::string, std::string> m_wsCacheMessages;
    std::map<std::string, int64_t> m_httpCachedTag;
    std::map<std::string, ASVariantMap> m_SystemStatusCachedMessage;

    std::multimap<std::string, std::string> m_registeredWsClients;
    std::multimap<std::string, std::string> m_registeredUpdatesClients;
    std::multimap<std::string, std::string> m_registeredSystemStatusClients;

    struct MatchRule
    {
        int tag;
        ASService::SignalCallback callback;

        MatchRule(int t, ASService::SignalCallback c)
                : tag(t), callback(std::move(c))
        { }
    };

    int mMatchTagCounter;
    std::unordered_map<sd_bus_slot*, MatchRule> mMatchSlots;
};

#endif // ASSERVICE_P_H
