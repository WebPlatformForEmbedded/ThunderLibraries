//
//  asrequest.cpp
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "asrequest.h"
#include "asrequest_p.h"
#include "sky/log.h"

using namespace sky;

ASRequest::ASRequest(const ASRequest &other)
    : m_private(other.m_private)
{
}

ASRequest::ASRequest(ASRequest &&other)
    : m_private(std::move(other.m_private))
{
}

ASRequest::ASRequest(std::shared_ptr<ASRequestPrivate> &&priv)
    : m_private(std::move(priv))
{
}

ASRequest::Method ASRequest::method() const
{
    return m_private->m_method;
}

std::string ASRequest::path() const
{
    return m_private->m_urlPath;
}

std::string ASRequest::body() const
{
    return m_private->m_body;
}

ASRequest::HeaderMap ASRequest::headers() const
{
    return m_private->m_headers;
}

ASRequest::QueryStringMap ASRequest::queryParams() const
{
    return m_private->m_queryParams;
}

bool ASRequest::sendReply(uint code) const
{
    return m_private->sendReply(code, HeaderMap(), std::string());
}

bool ASRequest::sendReply(uint code, const std::string &body) const
{
    return m_private->sendReply(code, HeaderMap(), body);
}

bool ASRequest::sendReply(uint code, const HeaderMap &headers, const std::string &body) const
{
    return m_private->sendReply(code, headers, body);
}

bool ASRequest::sendErrorReply(int httpCode, int errorCode,
                               const std::string &userMessage,
                               const std::string &developerMessage) const
{
    char body[128];
    if (developerMessage.empty())
    {
        snprintf(body, sizeof(body),
                 R"JSON({ "errorCode": %d, "userMessage": "%s" })JSON",
                 errorCode, userMessage.c_str());
    }
    else
    {
        snprintf(body, sizeof(body),
                 R"JSON({ "errorCode": %d, "userMessage": "%s", "developerMessage": "%s" })JSON",
                 errorCode, userMessage.c_str(), developerMessage.c_str());
    }

    return m_private->sendReply(httpCode, HeaderMap(), body);
}

bool ASRequest::sendErrorReply(ErrorType type, const std::string &developerMessage) const
{
    struct CannedError
    {
        int httpCode;
        int errorCode;
        std::string userMessage;
    };

    static const std::map<ErrorType, CannedError> cannedErrors =
    {
        {   InvalidUrlError,        {  404, 101, "Invalid URL"                  }   },
        {   InvalidParametersError, {  400, 102, "Invalid Parameters"           }   },
        {   GenericFailureError,    {  500, 103, "Generic failure"              }   },
        {   NotSupportedError,      {  404, 104, "Not supported on this device" }   },
    };

    const CannedError &error = cannedErrors.at(type);
    return sendErrorReply(error.httpCode, error.errorCode, error.userMessage, developerMessage );
}



// -----------------------------------------------------------------------------
/*!
    \internal

    Helper to determine the request method from the flags.

 */
static ASRequest::Method methodFromFlags(uint requestFlags)
{
    switch (requestFlags & 0xf)
    {
        case 0x1:
            return ASRequest::HttpGet;
        case 0x2:
            return ASRequest::HttpPost;
        default:
            return ASRequest::InvalidMethod;
    }
}



// -----------------------------------------------------------------------------
/*!
    \internal



 */
ASRequestPrivate::ASRequestPrivate(const EventLoop &eventLoop,
                                   sd_bus_message *requestMsg)
    : m_eventLoop(eventLoop)
    , m_reply(nullptr)
    , m_sentReply(false)
{
    // we need to de-marshall all the args from the request
    if (!parseRequest(requestMsg))
    {
        LOG_WARNING("failed to parse request message");
        return;
    }

    // create the reply message, it'll only be populated and sent when sendReply
    // is called
    int rc = sd_bus_message_new_method_return(requestMsg, &m_reply);
    if ((rc < 0) || (m_reply == nullptr))
    {
        LOG_SYS_WARNING(-rc, "failed to create reply message");
        m_reply = nullptr;
        return;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Destructor, checks if a response has been sent and if not logs a warning
    and sends a default error response.

 */
ASRequestPrivate::~ASRequestPrivate()
{
    std::lock_guard<std::mutex> locker(m_lock);

    // if haven't yet sent a reply do it now
    if (!m_sentReply && m_reply)
    {
        LOG_WARNING("as request object destroyed without sending a reply,"
                   " sending default reply");

        // create a generic http error reply
        char body[128];
        snprintf(body, sizeof(body),
                 R"JSON( { "errorCode": "105", "userMessage": "%s", "developerMessage": "%s" } )JSON",
                 "Service failure",
                 "Service failed to send response to request");

        // send off the reply
        doSendReply(500, ASRequest::HeaderMap(), body);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Attempts to extract all the fields from the request message.

    \note The signature for request method calls is "usa{ss}a{ss}s".
 */
bool ASRequestPrivate::parseRequest(sd_bus_message *msg)
{
    // sanity check the signature of the request
    if (!sd_bus_message_has_signature(msg, "usa{ss}a{ss}s"))
        return false;


    // read the flags and url path
    uint32_t flags;
    const char *urlPath = nullptr;
    int rc = sd_bus_message_read(msg, "us", &flags, &urlPath);
    if (rc < 0)
        return false;

    if (urlPath)
        m_urlPath.assign(urlPath);

    m_method = methodFromFlags(flags);


    //  read the http headers
    rc = sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, "{ss}");
    if (rc < 0)
        return false;

    while ((rc = sd_bus_message_enter_container(msg, SD_BUS_TYPE_DICT_ENTRY, "ss")) > 0)
    {
        const char *key = nullptr, *value = nullptr;

        rc = sd_bus_message_read(msg, "ss", &key, &value);
        if (rc < 0)
            return false;

        if (key && value)
            m_headers.emplace(key, value);

        rc = sd_bus_message_exit_container(msg);
        if (rc < 0)
            return rc;
    }

    rc = sd_bus_message_exit_container(msg);
    if (rc < 0)
        return false;


    // read the query parameters
    rc = sd_bus_message_enter_container(msg, SD_BUS_TYPE_ARRAY, "{ss}");
    if (rc < 0)
        return false;

    while ((rc = sd_bus_message_enter_container(msg, SD_BUS_TYPE_DICT_ENTRY, "ss")) > 0)
    {
        const char *key = nullptr, *value = nullptr;

        rc = sd_bus_message_read(msg, "ss", &key, &value);
        if (rc < 0)
            return false;

        if (key && value)
            m_queryParams.emplace(key, value);

        rc = sd_bus_message_exit_container(msg);
        if (rc < 0)
            return rc;
    }

    rc = sd_bus_message_exit_container(msg);
    if (rc < 0)
        return false;


    // finally read the request body
    const char *body = nullptr;
    rc = sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &body);
    if (rc < 0)
        return false;

    if (body)
        m_body.assign(body);


    // parsed all the fields
    return true;
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if the request was parsed successifully and a reply message
    was created (but not sent).

 */
bool ASRequestPrivate::isValid() const
{
    return (m_reply != nullptr);
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Sends back a reply with the given values.  This method call is thread safe,
    it can be called from any thread.  If called from outside the event loop
    thread then the reply is queued on the event loop and the reply object
    invalidated.

 */
bool ASRequestPrivate::sendReply(unsigned int code,
                                 const ASRequest::HeaderMap &headers,
                                 const std::string &body)
{
    std::lock_guard<std::mutex> locker(m_lock);

    // check to make sure we haven't already sent a reply
    if (m_sentReply)
    {
        LOG_WARNING("already sent a reply, can't send again");
        return false;
    }

    // and then check we have a reply object
    if (!m_reply)
    {
        LOG_ERROR("missing reply message");
        return false;
    }

    // do the actual send
    return doSendReply(code, headers, body);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Does the actual sending, we've first checked that everything is ok and
    the m_lock is held.

 */
bool ASRequestPrivate::doSendReply(unsigned int code,
                                   const ASRequest::HeaderMap &headers,
                                   const std::string &body)
{
    // indicate we've sent the reply
    m_sentReply = true;

    // detach the reply and give to the marshalling / send code, it will free
    // it when sent
    sd_bus_message *reply = m_reply;
    m_reply = nullptr;

    // if running in the event loop thread we can just send the reply
    if (m_eventLoop.onEventLoopThread())
    {
        return marshallAndSendReply(reply, code, headers, body);
    }

    // otherwise running on a different thread so need to invoke from the
    // event loop thread
    return m_eventLoop.invokeMethod(&ASRequestPrivate::marshallAndSendReply,
                                    reply, code, headers, body);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Copies all the arguments into the reply message.

    \note The signature of the return type is "(ua{ss}s)".
 */
int ASRequestPrivate::marshallReply(sd_bus_message *reply,
                                    uint32_t code,
                                    const ASRequest::HeaderMap &headers,
                                    const std::string &body)
{
    int rc = sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "ua{ss}s");
    if (rc < 0)
        return rc;


    // add the return code
    rc = sd_bus_message_append_basic(reply, SD_BUS_TYPE_UINT32, &code);
    if (rc < 0)
        return rc;


    // add all the header key value pairs
    rc = sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "{ss}");
    if (rc < 0)
        return rc;

    for (const auto &header : headers)
    {
        rc = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "ss");
        if (rc < 0)
            return rc;

        rc = sd_bus_message_append(reply, "ss",
                                   header.first.c_str(),
                                   header.second.c_str());
        if (rc < 0)
            return rc;

        rc = sd_bus_message_close_container(reply);
        if (rc < 0)
            return rc;
    }

    rc = sd_bus_message_close_container(reply);
    if (rc < 0)
        return rc;


    // add the reply body
    rc = sd_bus_message_append_basic(reply, SD_BUS_TYPE_STRING, body.c_str());
    if (rc < 0)
        return rc;


    // close the struct
    rc = sd_bus_message_close_container(reply);
    if (rc < 0)
        return rc;

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Populates the reply message and sends it over the bus.

    \warning This function must be called from the thread context of the
    event loop the dbus connection is attached to.

 */
bool ASRequestPrivate::marshallAndSendReply(sd_bus_message *reply,
                                            unsigned int code,
                                            const ASRequest::HeaderMap &headers,
                                            const std::string &body)
{
    // copy all the args into the reply message
    int rc = marshallReply(reply, code, headers, body);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to marshall all args into reply message");
        sd_bus_message_unref(reply);
        return false;
    }

    // and finally send the reply
    rc = sd_bus_send(nullptr, reply, nullptr);
    if (rc < 0)
    {
        LOG_SYS_ERROR(-rc, "failed to send reply messag");
        return false;
    }

    return true;
}

