//
//  asservice.cpp
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "asservice.h"
#include "asservice_p.h"
#include "asrequest.h"
#include "asrequest_p.h"
#include "sky/log.h"
#include "dbus/dbusconnection.h"
#include "eventloop/eventloop.h"
#include "System/settings/SystemSettings/SystemSettingsStorage.h"

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#include <cassert>
#include <dbus/dbusmessage_p.h>

using namespace sky;

ASService::ASService(const DBusConnection &dbusConn,
                     const std::string &configJson)
    : m_private(std::make_shared<ASServicePrivate>(dbusConn, configJson, this))
{
}

ASService::~ASService()
{
    m_private.reset();
}

// -----------------------------------------------------------------------------
/*!
    Returns the dbus connection object that the service is attached to.  This
    is the same as connection object supplied in the constructor.


 */
DBusConnection ASService::connection() const
{
    return m_private->dbusConn();
}

// -----------------------------------------------------------------------------
/*!
    Expected to be overridden, it's called when a request from the remote
    client is received.

 */
void ASService::onRequest(const ASRequest &request)
{
    (void)request;
}

// -----------------------------------------------------------------------------
/*!


 */
std::string ASService::systemInfo()
{
    return std::string();
}

// -----------------------------------------------------------------------------
/*!


 */
std::string ASService::getSystemSetting(const std::string &name)
{
    LOG_ENTRY("> %s", __func__);

    std::string setting{};

    if (SystemSettingsStorage::getInstance()->GetByteArrayValue(name, setting) == 0)
    {
        LOG_INFO("getting {%s, %s} is done successfully from persistant storage",
           name.c_str(), setting.c_str());
    }
    else
    {
        LOG_ERROR("failed to get setting {%s} from persistant storage", name.c_str());
    }

    LOG_EXIT("< %s", __func__);

    return setting;
}

// -----------------------------------------------------------------------------
/*!


 */
bool ASService::setSystemSetting(const std::string &name, const std::string &value)
{
    LOG_ENTRY("> %s", __func__);

    if ("state" == name)
    {
        LOG_MIL("setting {%s, %s} is done successfully",
            name.c_str(), value.c_str());

        std::map<std::string, ASVariantMap> systemStatus{};
        ASVariantMap servicelistState{};

        servicelistState.insert("state", value);

        if ("unavailable" == value)
          servicelistState.insert("reason", std::string("DTT scan required"));

        systemStatus.emplace("servicelist", servicelistState);

        updateSystemStatus(systemStatus);
    }
    else
    {
        LOG_ERROR("setting {%s} not supported", name.c_str());
    }

    LOG_EXIT("< %s", __func__);

    return true;
}

// -----------------------------------------------------------------------------
/*!


 */
std::string ASService::getSystemTime()
{
    return std::string();
}

// -----------------------------------------------------------------------------
/*!


 */
std::string ASService::getSystemInputs()
{
    return std::string();
}


// -----------------------------------------------------------------------------
/*!


 */
std::string ASService::getSystemEntitlements()
{
    return std::string();
}

// -----------------------------------------------------------------------------
/*!


 */
std::string ASService::getTestPreference(const std::string &name)
{
    (void)name;
    return std::string();
}

// -----------------------------------------------------------------------------
/*!


 */
std::string ASService::getAsPreference(const std::string &name)
{
    (void)name;
    return std::string();
}

// -----------------------------------------------------------------------------
/*!


 */
bool ASService::setTestPreference(const std::string &name, const std::string &value, int pin)
{
    (void)name;
    (void)value;
    (void)pin;

    return false;
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Updates the contents of the websocket.  If any listeners are registered
    they will be sent the new content.

    This function is thread safe and can be called from any thread.

 */
void ASService::updateWebSocket(const std::string &wsUrl, const std::string &wsMessage)
{
    m_private->updateWebSocket(wsUrl, wsMessage);
}

void ASService::updateSystemStatus(const std::map<std::string, ASVariantMap>& systemStatusMessage )
{
    m_private->updateSystemStatus( systemStatusMessage );
}
// -----------------------------------------------------------------------------
/*!
    \threadsafe


 */
void ASService::updateHttpUrl(const std::string &httpUrl, int64_t tag)
{
    m_private->updateHttpUrl(httpUrl, tag);
}

int ASService::registerForSignal(const std::string& service,
                                        const std::string& path,
                                        const std::string& interface,
                                        const std::string& signalName,
                                        const std::string& extraArgs,
                                        const ASService::SignalCallback& signalCallback)
{
    m_private->registerForSignal(service, path, interface, signalName, extraArgs, signalCallback);
}

int ASService::unregisterForSignal( int tag )
{
    m_private->unregisterForSignal( tag );
}


// -----------------------------------------------------------------------------
/*!
    The following are re-worked macros from the sd-bus-vtable.h header file
    to make them c++ compatible.

    \see https://github.com/systemd/systemd/issues/4858

 */
#undef SD_BUS_VTABLE_START
#define SD_BUS_VTABLE_START(_flags)                                     \
        {                                                               \
                .type = _SD_BUS_VTABLE_START,                           \
                .flags = _flags,                                        \
                .x = {                                                  \
                    .start = {                                          \
                        .element_size = sizeof(sd_bus_vtable)           \
                    },                                                  \
                },                                                      \
        }

#undef SD_BUS_METHOD_WITH_OFFSET
#define SD_BUS_METHOD_WITH_OFFSET(_member, _signature, _result, _handler, _offset, _flags)   \
        {                                                               \
                .type = _SD_BUS_VTABLE_METHOD,                          \
                .flags = _flags,                                        \
                .x = {                                                  \
                    .method = {                                         \
                        .member = _member,                              \
                        .signature = _signature,                        \
                        .result = _result,                              \
                        .handler = _handler,                            \
                        .offset = _offset,                              \
                    },                                                  \
                },                                                      \
        }

#undef SD_BUS_SIGNAL
#define SD_BUS_SIGNAL(_member, _signature, _flags)                      \
        {                                                               \
                .type = _SD_BUS_VTABLE_SIGNAL,                          \
                .flags = _flags,                                        \
                .x = {                                                  \
                    .signal = {                                         \
                        .member = _member,                              \
                        .signature = _signature,                        \
                    },                                                  \
                },                                                      \
        }

// -----------------------------------------------------------------------------
/*!
    \internal

    Table that describes the callbacks to call for the com.sky.as.Service1
    methods.

 */
static const sd_bus_vtable g_asServiceVTable[] =
{
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("Config",                      nullptr,         "s",         &ASServicePrivate::config,                SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Request",                     "usa{ss}a{ss}s", "(ua{ss}s)", &ASServicePrivate::request,               SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetSystemInfo",               nullptr,         "s",         &ASServicePrivate::systemInfo,            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetSystemSetting",            "s",             "s",         &ASServicePrivate::getSystemSetting,      SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetSystemSetting",            "ss",            nullptr,     &ASServicePrivate::setSystemSetting,      SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetTestPreference",           "s",             "s",         &ASServicePrivate::getTestPreference,     SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetTestPreference",           "ssi",           nullptr,     &ASServicePrivate::setTestPreference,     SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("GetAsPreference",             "s",             "s",         &ASServicePrivate::getAsPreference,       SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetSystemTime",               nullptr,         "s",         &ASServicePrivate::getSystemTime,         SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("GetDiagContexts",             nullptr,         "s",         &ASServicePrivate::GetDiagContexts,       SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetDiagContexts",             "s",             nullptr,     &ASServicePrivate::SetDiagContexts,       SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_METHOD("RegisterWebSocketListener",   "s",             nullptr,     &ASServicePrivate::registerWsListener,    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("UnregisterWebSocketListener", "s",             nullptr,     &ASServicePrivate::unregisterWsListener,  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("WebSocketUpdate",             "ss",                                                                   0),

    SD_BUS_METHOD("RegisterSysStatusListener",   nullptr,         nullptr,     &ASServicePrivate::registerSystemStatusListener,    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("UnregisterSysStatusListener", nullptr,         nullptr,     &ASServicePrivate::unregisterSystemStatusListener,  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("SysStatusUpdate",             "a(sa{sv})",                                                            0),

    SD_BUS_METHOD("RegisterUpdatesListener",     "s",             nullptr,     &ASServicePrivate::registerHttpListener,  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("UnregisterUpdatesListener",   "s",             nullptr,     &ASServicePrivate::unregisterHttpListener,SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetSystemInputs",             nullptr,         "s",         &ASServicePrivate::getSystemInputs,       SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("SystemInputsUpdate",          "s",                                                                    0),
    SD_BUS_SIGNAL("HttpUpdate",                  "sx",                                                                   0),
    SD_BUS_SIGNAL("PowerLEDStateUpdate",         "s",                                                                    0),
    SD_BUS_SIGNAL("BouquetUpdate",               "s",                                                                    0),
    SD_BUS_METHOD("GetSystemEntitlements",       nullptr,         "s",         &ASServicePrivate::getSystemEntitlements, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("SystemEntitlementsUpdate",    "s",                                                                    0),

    SD_BUS_VTABLE_END
};


// -----------------------------------------------------------------------------
/*!


 */
ASServicePrivate::ASServicePrivate(const DBusConnection &dbusConn,
                                   const std::string &configJson,
                                   ASService *parent)
    : m_parent(parent)
    , m_dbusConn(dbusConn)
    , m_configJson(configJson)
    , m_objectPath("/com/sky/as/service")
    , m_interface("com.sky.as.Service1")
    , m_slot(nullptr)
    , m_tracker(nullptr)
    , mMatchTagCounter(0)
{
    // installs the object and handlers for the methods
    int rc = sd_bus_add_object_vtable(m_dbusConn.handle(),
                                      &m_slot,
                                      m_objectPath.c_str(),
                                      m_interface.c_str(),
                                      g_asServiceVTable,
                                      this);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to add ASService object");
    }

    // create a tracker object to keep track of registered clients, if they
    // disappear then we automatically remove their listeners
    rc = sd_bus_track_new(m_dbusConn.handle(),
                          &m_tracker,
                          &ASServicePrivate::trackerHandler,
                          this);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to create bus tracker");
    }

#if 0
    // set the tracker as recursive as a remote client listener may register
    // for multiple urls
    rc = sd_bus_track_set_recursive(m_tracker, true);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to enable recursive mode on tracker");
    }
#endif

}

// -----------------------------------------------------------------------------
/*!


 */
ASServicePrivate::~ASServicePrivate()
{
    // frees the vtable object registered on the dbus
    sd_bus_slot_unref(m_slot);

    // free the tracker
    sd_bus_track_unref(m_tracker);
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
DBusConnection ASServicePrivate::dbusConn() const
{
    return m_dbusConn;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when there is a tracked service drops off the
    bus.

    Annoyingly the callback doesn't tell us which service has dropped off the
    bus, so we have to iterate through all the callbacks and check if still
    being tracked, if not it means they've gone bye bye.

 */
int ASServicePrivate::trackerHandler(sd_bus_track *track, void *userData)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    // lambda to walk the listener maps(s) and remove services that are no
    // longer being tracked
    auto removeUntrackedListeners =
        [&track](std::multimap<std::string, std::string> &listeners)
        {
            auto it = listeners.begin();
            while (it != listeners.end())
            {
                const std::string &service = it->second;
                if (!sd_bus_track_contains(track, service.c_str()))
                {
                    LOG_INFO("removing listener '%s' for url '%s' as service dropped off bus",
                            service.c_str(), it->first.c_str());

                    it = listeners.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        };

    // run the lambda to remove all defunct listener services
    removeUntrackedListeners(self->m_registeredWsClients);
    removeUntrackedListeners(self->m_registeredUpdatesClients);

    return 1;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.Config' method
    call arrives.

 */
int ASServicePrivate::config(sd_bus_message *msg, void *userData,
                             sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    return sd_bus_reply_method_return(msg, "s", self->m_configJson.c_str());
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.Request' method
    call arrives.

 */
int ASServicePrivate::request(sd_bus_message *msg, void *userData,
                              sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // create the request object
    std::shared_ptr<ASRequestPrivate> request =
        std::make_shared<ASRequestPrivate>(self->m_dbusConn.eventLoop(), msg);

    // check the message was paresed correctly
    if (!request || !request->isValid())
    {
        LOG_WARNING("invalid Request method call");
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_ARGS,
                                          "Failed to parse request args");
    }

    // call the client api
    self->m_parent->onRequest(ASRequest(std::move(request)));

    // done (the reply will be created / queued by the client api)
    return 1;
}

int ASServicePrivate::GetDiagContexts(sd_bus_message *msg, void *userData, sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // call the client api
    const std::string value = sky::getFilter();

    // marshall the reply
    return sd_bus_reply_method_return(msg, "s", value.c_str());
}


int ASServicePrivate::SetDiagContexts(sd_bus_message *msg, void *userData, sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // de-marshall the arguments
    const char *filterJson = nullptr;
    int rc = sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &filterJson);
    if ((rc < 0) || (filterJson == nullptr))
    {
        LOG_SYS_WARNING(rc, "failed to parse method call messag");
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                          "Invalid argument types");
    }

    // call the client api
    sky::setFilter(filterJson);

    // marshall the reply
    return sd_bus_reply_method_return(msg, "");
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.GetSystemInfo'
    method call arrives.

 */
int ASServicePrivate::systemInfo(sd_bus_message *msg, void *userData,
                                 sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // create the reply message
    return sd_bus_reply_method_return(msg, "s", self->m_parent->systemInfo().c_str());
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.GetSystemSetting'
    method call arrives.

 */
int ASServicePrivate::getSystemSetting(sd_bus_message *msg, void *userData,
                                       sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // de-marshall the arguments
    const char *name = nullptr;
    int rc = sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &name);
    if ((rc < 0) || (name == nullptr))
    {
        LOG_SYS_WARNING(-rc, "failed to parse method call messag");
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                          "Invalid argument types");
    }

    // call the client api
    const std::string value = self->m_parent->getSystemSetting(name);

    // marshall the reply
    return sd_bus_reply_method_return(msg, "s", value.c_str());
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.SetTestPreference'
    method call arrives.

 */
int ASServicePrivate::setSystemSetting(sd_bus_message *msg, void *userData,
                                       sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // de-marshall the arguments
    const char *key = nullptr, *value = nullptr;
    int rc = sd_bus_message_read(msg, "ss", &key, &value);
    if ((rc < 0) || !key || !value)
    {
        LOG_SYS_WARNING(-rc, "failed to parse method call messag");
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                          "Invalid argument types");
    }

    // call the client api
    if (self->m_parent->setSystemSetting(key, value))
    {
        return sd_bus_reply_method_return(msg, "");
    }
    else
    {
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_ARGS,
                                          "Failed to set test preference '%s'",
                                          key);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.GetTestPreference'
    method call arrives.

 */
int ASServicePrivate::getTestPreference(sd_bus_message *msg, void *userData,
                                        sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // de-marshall the arguments
    const char *name = nullptr;
    int rc = sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &name);
    if ((rc < 0) || (name == nullptr))
    {
        LOG_SYS_WARNING(-rc, "failed to parse method call messag");
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                          "Invalid argument types");
    }

    // call the client api
    const std::string value = self->m_parent->getTestPreference(name);

    // marshall the reply
    return sd_bus_reply_method_return(msg, "s", value.c_str());
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.GetAsPreference'
    method call arrives.

 */
int ASServicePrivate::getAsPreference(sd_bus_message *msg, void *userData, sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // de-marshall the arguments
    const char *name = nullptr;
    int rc = sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &name);
    if ((rc < 0) || (name == nullptr))
    {
        LOG_SYS_WARNING(-rc, "failed to parse method call messag");
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                          "Invalid argument types");
    }

    // call the client api
    const std::string value = self->m_parent->getAsPreference(name);

    // marshall the reply
    return sd_bus_reply_method_return(msg, "s", value.c_str());
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.GetSystemTime'
    method call arrives.

 */
int ASServicePrivate::getSystemTime(sd_bus_message *msg, void *userData, sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    return sd_bus_reply_method_return(msg, "s", self->m_parent->getSystemTime().c_str());
}



// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.SetTestPreference'
    method call arrives.

 */
int ASServicePrivate::setTestPreference(sd_bus_message *msg, void *userData,
                                        sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    // de-marshall the arguments
    int pin;
    const char *key = nullptr, *value = nullptr;
    int rc = sd_bus_message_read(msg, "ssi", &key, &value, &pin);
    if ((rc < 0) || !key || !value)
    {
        LOG_SYS_WARNING(-rc, "failed to parse method call messag");
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                          "Invalid argument types");
    }

    // call the client api
    if (self->m_parent->setTestPreference(key, value, pin))
    {
        return sd_bus_reply_method_return(msg, "");
    }
    else
    {
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_ARGS,
                                          "Failed to set test preference '%s'",
                                          key);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Code shared by both websocket and http update listener.  It registers the
    service for updates from a given url.

 */
int ASServicePrivate::registerListener(sd_bus_message *msg,
                                       std::multimap<std::string, std::string> *listenerMap,
                                       const UpdaterFunc &updateFunc, const std::string& defaultUrl){
    std::string urlValue;
    int rc;

    if ( defaultUrl.empty() ) {
        // de-marshall the arguments
        const char *urlArg = nullptr;
        rc = sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &urlArg);
        if ((rc < 0) || !urlArg) {
            LOG_SYS_WARNING(-rc, "failed to parse method call messag");
            return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                              "Invalid argument types");
        }
        urlValue = urlArg;
    } else{
        urlValue = defaultUrl;
    }

    // get the caller and check we don't already have a listener registered
    const char *caller = sd_bus_message_get_sender(msg);
    LOG_INFO("registering listener '%s' for url '%s'", caller, urlValue.c_str());

    auto range = listenerMap->equal_range(urlValue);
    for (auto it = range.first; it != range.second; ++it)
    {
        const std::string &service = it->second;
        if (service == caller)
        {
            LOG_WARNING("already have listener registered for '%s' url", urlValue.c_str());
            return sd_bus_reply_method_errorf(msg, "com.sky.as.Error.AlreadyRegistered",
                                              "Listener for url already registered");
        }
    }

    // new unique caller / url pair so add to the map
    listenerMap->emplace(urlValue, caller);

    // enable tracking of the caller so if they disappear the listener is removed
    rc = sd_bus_track_add_name(m_tracker, caller);
    if (rc < 0)
    {
        LOG_SYS_WARNING(-rc, "failed to setup tracker for service");
    }

    // next queue up an immediate ws update for the given client
    EventLoop loop = m_dbusConn.eventLoop();
    loop.invokeMethod(updateFunc, this, std::string(caller), urlValue);

    // send back empty reply
    return sd_bus_reply_method_return(msg, "");
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Code shared by both websocket and http update listener.  It unregisters the
    service for updates from a given url in the \a listenerMap.

 */
int ASServicePrivate::unregisterListener(sd_bus_message *msg,
                                         std::multimap<std::string, std::string> *listenerMap, const std::string& defaultUrl){
    std::string urlValue;
    int rc;

    if ( defaultUrl.empty() ) {
        // de-marshall the arguments
        const char *urlArg = nullptr;
        rc = sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &urlArg);
        if ((rc < 0) || !urlArg) {
            LOG_SYS_WARNING(-rc, "failed to parse method call message");
            return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_INVALID_SIGNATURE,
                                              "Invalid argument types");
        }
        urlValue = urlArg;
    } else{
        urlValue = defaultUrl;
    }

    // get the caller
    const char *caller = sd_bus_message_get_sender(msg);

    // and remove from the registered clients map
    bool found = false;
    auto range = listenerMap->equal_range(urlValue);
    for (auto it = range.first; it != range.second; )
    {
        const std::string &service = it->second;
        if (service == caller)
        {
            it = listenerMap->erase(it);
            found = true;
        }
        else
        {
            ++it;
        }
    }

    // log an error if didn't find a registered l
    if (!found)
    {
        LOG_WARNING("failed to find registered listener '%s' for url '%s'",
                   caller, urlValue.c_str());
        return sd_bus_reply_method_errorf(msg, SD_BUS_ERROR_SERVICE_UNKNOWN,
                                          "Service not registered ws listener");
    }

    // next check if there are no more listeners registered from the caller and
    // if so remove the tracker on the caller
    bool callerStillRegistered = false;
    for (const auto &listener : *listenerMap)
    {
        const std::string &service = listener.second;
        if (service == caller)
        {
            callerStillRegistered = true;
            break;
        }
    }

    // remove from the tracker if caller no longer registered
    if (!callerStillRegistered)
    {
        rc = sd_bus_track_remove_name(m_tracker, caller);
        if (rc < 0)
        {
            LOG_SYS_WARNING(-rc, "failed to remove tracker for service");
        }
    }

    // send back empty reply
    return sd_bus_reply_method_return(msg, "");
}


// -----------------------------------------------------------------------------
/*!
    \internal

    Called by remote clients to register for updates to a given websocket.

 */
int ASServicePrivate::registerWsListener(sd_bus_message *msg, void *userData,
                                         sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    return self->registerListener(msg, &self->m_registeredWsClients,
                                  std::mem_fn(&ASServicePrivate::sendCachedWsUpdateTo));
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by remote clients to unregister for updates to a given websocket.

 */
int ASServicePrivate::unregisterWsListener(sd_bus_message *msg, void *userData,
                                           sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    return self->unregisterListener(msg, &self->m_registeredWsClients);
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
int ASServicePrivate::registerHttpListener(sd_bus_message *msg, void *userData,
                                           sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    return self->registerListener(msg, &self->m_registeredUpdatesClients,
                                  std::mem_fn(&ASServicePrivate::sendCachedHttpUpdateTo));
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
int ASServicePrivate::unregisterHttpListener(sd_bus_message *msg, void *userData,
                                             sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    return self->unregisterListener(msg, &self->m_registeredUpdatesClients);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.getSystemInputs'
    method call arrives.

 */
int ASServicePrivate::getSystemInputs(sd_bus_message *msg, void *userData, sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    return sd_bus_reply_method_return(msg, "s", self->m_parent->getSystemInputs().c_str());
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by the sd-bus library when a 'com.sky.as.Service1.getSystemEntitlements'
    method call arrives.

 */
int ASServicePrivate::getSystemEntitlements(sd_bus_message *msg, void *userData, sd_bus_error *retError)
{
    auto *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);
    assert(self->m_parent != nullptr);

    return sd_bus_reply_method_return(msg, "s", self->m_parent->getSystemEntitlements().c_str());
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */

int ASServicePrivate::registerSystemStatusListener(sd_bus_message *msg, void *userData, sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    return self->registerListener(msg, &self->m_registeredSystemStatusClients,
                                  std::mem_fn(&ASServicePrivate::sendCachedSystemStatusUpdateTo), "/as/system/status");
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
int ASServicePrivate::unregisterSystemStatusListener(sd_bus_message *msg, void *userData,
                                                     sd_bus_error *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    return self->unregisterListener(msg, &self->m_registeredSystemStatusClients, "/as/system/status");
}


// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Updates the message in the websocket with the given url.  If any remote
    listeners a registered with the given url then a the new message is sent
    to them.

    The value is cached, such that when a new client registers for notifications
    it will immediately be sent the last message.

 */
void ASServicePrivate::updateWebSocket(const std::string &wsUrl,
                                       const std::string &wsMessage)
{
    // update the ws in a lambda so can post it to an event queue if being
    // called from outside the event loop thread
    std::function<void()> lambda =
        [this, wsUrl, wsMessage]()
        {
            LOG_INFO("caching message '%s' for ws url '%s'",
                     wsUrl.c_str(), wsMessage.c_str());

            // update the cached value
            m_wsCacheMessages[wsUrl] = wsMessage;

            // then send signals to any listeners
            auto range = m_registeredWsClients.equal_range(wsUrl);
            for (auto it = range.first; it != range.second; ++it)
            {
                sendWsUpdateTo(it->second, wsUrl, wsMessage);
            }
        };

    // update and send the signal from the event loop thread
    EventLoop eventLoop = m_dbusConn.eventLoop();
    if (eventLoop.onEventLoopThread())
        lambda();
    else
        eventLoop.invokeMethod(std::move(lambda));
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ASServicePrivate::sendWsUpdateTo(const std::string &service,
                                      const std::string &wsUrl,
                                      const std::string &message)
{
    LOG_INFO("sending message '%s' for ws url '%s' to '%s'\n",
             message.c_str(), wsUrl.c_str(), service.c_str());

    // create the signal
    sd_bus_message *msg;
    int rc = sd_bus_message_new_signal(m_dbusConn.handle(),
                                       &msg,
                                       m_objectPath.c_str(),
                                       m_interface.c_str(),
                                       "WebSocketUpdate");
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to create new signal message");
        return;
    }

    // add the arguments
    rc = sd_bus_message_append(msg, "ss", wsUrl.c_str(), message.c_str());
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to append args to signal");
    }
    else
    {
        // and send the signal
        rc = sd_bus_send_to(m_dbusConn.handle(), msg, service.c_str(), nullptr);
        if (rc < 0)
        {
            LOG_SYS_ERROR(-rc, "failed to send signal");
        }
    }

    sd_bus_message_unref(msg);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called when a new client has registered a listener and we therefore need to
    send it the cached value (if one).


 */
void ASServicePrivate::sendCachedWsUpdateTo(const std::string &service,
                                            const std::string &wsUrl)
{
    // check if we have a cached value
    auto it = m_wsCacheMessages.find(wsUrl);
    if (it == m_wsCacheMessages.end())
    {
        LOG_INFO("no cached ws message for url '%s'", wsUrl.c_str());
        return;
    }

    // send off the cached value
    sendWsUpdateTo(service, wsUrl, it->second);
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Public API called by the service code to update the 64-bit tag value
    associated with an http url in the /as/updates websocket.

 */
void ASServicePrivate::updateHttpUrl(const std::string &httpUrl, int64_t tag)
{
    // update the ws in a lambda so can post it to an event queue if being
    // called from outside the event loop thread
    std::function<void()> lambda =
        [this, httpUrl, tag]()
        {
            LOG_INFO("caching tag %" PRId64 " for http url '%s'",
                     tag, httpUrl.c_str());

            // update the cached value
            m_httpCachedTag[httpUrl] = tag;

            // then send signals to any listeners
            auto range = m_registeredUpdatesClients.equal_range(httpUrl);
            for (auto it = range.first; it != range.second; ++it)
            {
                sendHttpUpdateTo(it->second, httpUrl, tag);
            }
        };

    // update and send the signal from the event loop thread
    EventLoop eventLoop = m_dbusConn.eventLoop();
    if (eventLoop.onEventLoopThread())
        lambda();
    else
        eventLoop.invokeMethod(std::move(lambda));
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
void ASServicePrivate::sendHttpUpdateTo(const std::string &service,
                                        const std::string &httpUrl,
                                        int64_t tag)
{
    LOG_INFO("sending tag %" PRId64 " for http url '%s' to '%s'",
            tag, httpUrl.c_str(), service.c_str());

    // create the signal
    sd_bus_message *msg;
    int rc = sd_bus_message_new_signal(m_dbusConn.handle(),
                                       &msg,
                                       m_objectPath.c_str(),
                                       m_interface.c_str(),
                                       "HttpUpdate");
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to create new signal message");
        return;
    }

    // add the arguments
    rc = sd_bus_message_append(msg, "sx", httpUrl.c_str(), tag);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to append args to signal");
    }
    else
    {
        // and send the signal
        rc = sd_bus_send_to(m_dbusConn.handle(), msg, service.c_str(), nullptr);
        if (rc < 0)
        {
            LOG_SYS_ERROR(-rc, "failed to send signal");
        }
    }

    sd_bus_message_unref(msg);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called when a new client has registered a listener for updates to an http
    domain and we therefore need to send it the cached value (if one).


 */
void ASServicePrivate::sendCachedHttpUpdateTo(const std::string &service,
                                              const std::string &httpUrl)
{
    LOG_INFO("Sending cached tag to service '%s', url '%s'", service.c_str(), httpUrl.c_str());

    // check if we have a cached value
    auto it = m_httpCachedTag.find(httpUrl);
    if (it == m_httpCachedTag.end())
    {
        LOG_INFO("no cached tag for url '%s'", httpUrl.c_str());
        return;
    }

    // send off the cached value
    sendHttpUpdateTo(service, httpUrl, it->second);
}

void ASServicePrivate::sendSystemStatusUpdateTo(const std::string &service, const std::map<std::string, ASVariantMap>& systemStatusUpdate)
{
    LOG_INFO("sending system status update to '%s'\n", service.c_str());

    // create the signal
    sd_bus_message *msg;
    int rc = sd_bus_message_new_signal(m_dbusConn.handle(),
                                       &msg,
                                       m_objectPath.c_str(),
                                       m_interface.c_str(),
                                       "SysStatusUpdate");
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to create new signal message");
        return;
    }

    // Vistor object that populates the reply dictionary
    struct SystemStatusVisitor : ASVariantMap::Visitor
    {
        sd_bus_message * const reply;

        SystemStatusVisitor(sd_bus_message *msg)
                : reply(msg)
        { }

        void appendDictEntry(const std::string& key, const char *type, const void *value)
        {
            sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv");
            sd_bus_message_append_basic(reply, 's', key.c_str());

            sd_bus_message_open_container(reply, SD_BUS_TYPE_VARIANT, type);
            sd_bus_message_append_basic(reply, *type, value);
            sd_bus_message_close_container(reply);

            sd_bus_message_close_container(reply);
        }

        void operator()(const std::string& k, bool v) override
        {
            int boolValue = v;
            appendDictEntry(k, "b", &boolValue);
        }
        void operator()(const std::string& k, int v) override
        {
            appendDictEntry(k, "i", &v);
        }
        void operator()(const std::string& k, double v) override
        {
            appendDictEntry(k, "d", &v);
        }
        void operator()(const std::string& k, const std::string& v) override
        {
            appendDictEntry(k, "s", v.c_str());
        }
    };

    // open the dictionary container
    rc = sd_bus_message_open_container(msg, SD_BUS_TYPE_ARRAY, "(sa{sv})");
    if (rc < 0) {
        LOG_SYS_WARNING(-rc, "failed to create dictionary for signal");
        sd_bus_message_unref(msg);
        return;
    }

    for ( auto statusUpdate : systemStatusUpdate ) {

        rc = sd_bus_message_open_container(msg, SD_BUS_TYPE_STRUCT, "sa{sv}");
        if (rc < 0) {
            LOG_SYS_ERROR(-rc, "failed to open dbus message container");
            sd_bus_message_ref(msg);
            return;
        }

        // add the arguments
        rc = sd_bus_message_append(msg, "s", statusUpdate.first.c_str());
        if (rc < 0) {
            LOG_SYS_ERROR(-rc, "failed to append args to signal");
            sd_bus_message_ref(msg);
            return;
        }

        // open the dictionary container
        rc = sd_bus_message_open_container(msg, SD_BUS_TYPE_ARRAY, "{sv}");
        if (rc < 0) {
            LOG_SYS_WARNING(-rc, "failed to create dictionary for signal");
            sd_bus_message_unref(msg);
            return;
        }

        // Apply the visitor to all entries in the map, this will populate the
        // dictionary in the reply message
        statusUpdate.second.visit(SystemStatusVisitor(msg));

        // Close the dictionary container
        rc = sd_bus_message_close_container(msg);
        if (rc < 0) {
            LOG_SYS_WARNING(-rc, "failed to close dictionary for signal");
            sd_bus_message_unref(msg);
            return;
        }

        // Close the dictionary container
        rc = sd_bus_message_close_container(msg);
        if (rc < 0) {
            LOG_SYS_WARNING(-rc, "failed to close dictionary for signal");
            sd_bus_message_unref(msg);
            return;
        }
    }

    // Close the dictionary container
    rc = sd_bus_message_close_container(msg);
    if (rc < 0) {
        LOG_SYS_WARNING(-rc, "failed to close dictionary for signal");
        sd_bus_message_unref(msg);
        return;
    }

    // Send the signal
    rc = sd_bus_send_to(m_dbusConn.handle(), msg, service.c_str(), nullptr);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to send signal");
    }
    else
    {
        LOG_INFO("Successfully sent the system status update signal" );
    }

    sd_bus_message_unref(msg);
}

void ASServicePrivate::sendCachedSystemStatusUpdateTo(const std::string &service, const std::string &wsUrl)
{
    // Check if we have a cached system status updates
    if ( m_SystemStatusCachedMessage.empty() )
    {
        LOG_INFO("No cached system status messages" );
        return;
    }

    // send off the cached value
    sendSystemStatusUpdateTo(service, m_SystemStatusCachedMessage);
}

void ASServicePrivate::updateSystemStatus( const std::map<std::string, ASVariantMap>& systemStatusUpdate )
{
    // update the ws in a lambda so can post it to an event queue if being
    // called from outside the event loop thread
    std::function<void()> lambda =
            [this, systemStatusUpdate]() {

                for (auto statusUpdate : systemStatusUpdate)
                {
                    LOG_INFO("caching System status entity '%s'",
                            statusUpdate.first.c_str());

                    // update the cached value
                    m_SystemStatusCachedMessage.erase(statusUpdate.first);
                    m_SystemStatusCachedMessage.emplace(statusUpdate.first, statusUpdate.second);
                }

                // then send signals to any listeners
                for (auto it = m_registeredSystemStatusClients.begin(); it != m_registeredSystemStatusClients.end(); ++it)
                {
                    sendSystemStatusUpdateTo(it->second, systemStatusUpdate);
                }
            };

    // update and send the signal from the event loop thread
    EventLoop eventLoop = m_dbusConn.eventLoop();
    if (eventLoop.onEventLoopThread())
        lambda();
    else
        eventLoop.invokeMethod(std::move(lambda));
}

int ASServicePrivate::registerForSignal(const std::string& service,
                                        const std::string& path,
                                        const std::string& interface,
                                        const std::string& signalName,
                                        const std::string& extraArgs,
                                        const ASService::SignalCallback& signalCallback)
{
    int tag = ++mMatchTagCounter;

    // create a lambda to be invoked within the context of the event loop thread
    std::function<void()> lambda =
    [this, tag, service, path, interface, signalName, extraArgs, signalCallback]()
    {
        std::string matchRule = "type='signal'";
        if ( !service.empty() )
        {
            matchRule += ( ",sender='" + service + "'" );
        }
        if ( !path.empty() )
        {
            matchRule += ( ",path='" + path + "'" );
        }
        if ( !interface.empty() )
        {
            matchRule += ( ",interface='" + interface + "'" );
        }
        if ( !signalName.empty() )
        {
            matchRule += ( ",member='" + signalName + "'" );
        }
        if ( !extraArgs.empty() )
        {
            matchRule += ( "," + extraArgs );
        }

        // attempt to add the rule match
        sd_bus_slot *slot = nullptr;
        int rc = sd_bus_add_match(m_dbusConn.handle(), &slot, matchRule.c_str(),
                                  sd_bus_message_handler_t(&onRuleMatch),
                                  this);
        if ((rc < 0) || (!slot))
        {
            LOG_SYS_ERROR(-rc, "failed to register for signal");
        }
        else
        {
            // rule added so insert into our local map
            mMatchSlots.emplace(slot, MatchRule(tag, signalCallback));
        }
    };

    // update and send the signal from the event loop thread
    EventLoop eventLoop = m_dbusConn.eventLoop();
    if (eventLoop.onEventLoopThread())
        lambda();
    else
        eventLoop.invokeMethod(std::move(lambda));

    return tag;
}

int ASServicePrivate::unregisterForSignal( int tag )
{
    // create a lambda to be invoked within the context of the event loop thread
    std::function<void()> lambda =
    [this, tag]()
    {
        // try and find the match tag
        auto it = mMatchSlots.begin();
        for ( ; it != mMatchSlots.end(); ++it)
        {
            const MatchRule &rule = it->second;
            if (rule.tag == tag)
                break;
        }

        // check we found the tag
        if (it == mMatchSlots.end())
        {
            LOG_WARNING("failed to find match rule with tag %d to remove",
                           tag);
            return;
        }

        // By unreferencing the slot, it should remove the match rule
        sd_bus_slot *slot = it->first;
        sd_bus_slot_unref(slot);

        // now we can remove from our map
        mMatchSlots.erase(it);
    };

    // update and send the signal from the event loop thread
    EventLoop eventLoop = m_dbusConn.eventLoop();
    if (eventLoop.onEventLoopThread())
        lambda();
    else
        eventLoop.invokeMethod(std::move(lambda));

    return 0;
}

int ASServicePrivate::onRuleMatch(sd_bus_message *msg, void *userData, void *retError)
{
    ASServicePrivate *self = reinterpret_cast<ASServicePrivate*>(userData);
    assert(self != nullptr);

    // Get the current slot and use it to find the match rule that hit
    sd_bus_slot *slot = sd_bus_get_current_slot(self->m_dbusConn.handle());
    if (!slot)
    {
        LOG_WARNING("Match callback called without valid slot");
        return -1;
    }

    // lookup the match rule
    auto it = self->mMatchSlots.find(slot);
    if (it == self->mMatchSlots.end())
    {
        LOG_WARNING("failed to find match callback for slot %p", slot);
        return -1;
    }

    // call the registered callback
    const MatchRule &rule = it->second;
    if (rule.callback)
    {
        // wrap the message in c++ fluff and call
        rule.callback(rule.tag, std::move(DBusMessage::createIncomingSignal(msg)));
    }

    return 0;
}
