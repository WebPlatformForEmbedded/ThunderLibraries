//
//  asrequest_p.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef ASREQUEST_P_H
#define ASREQUEST_P_H

#include "asrequest.h"
#include "eventloop/eventloop.h"

#include <systemd/sd-bus.h>

#include <mutex>
#include <string>

using namespace sky;

class ASRequestPrivate
{
public:
    ASRequestPrivate(const EventLoop &eventLoop, sd_bus_message *requestMsg);
    ~ASRequestPrivate();

    bool isValid() const;

    bool sendReply(unsigned int code,
                   const ASRequest::HeaderMap &headers,
                   const std::string &body);

private:
    bool parseRequest(sd_bus_message *msg);

    bool doSendReply(unsigned int code,
                     const ASRequest::HeaderMap &headers,
                     const std::string &body);

    static int marshallReply(sd_bus_message *reply,
                             uint32_t code,
                             const ASRequest::HeaderMap &headers,
                             const std::string &body);

    static bool marshallAndSendReply(sd_bus_message *reply,
                                     unsigned int code,
                                     const ASRequest::HeaderMap &headers,
                                     const std::string &body);

private:
    friend class ASRequest;

    const EventLoop m_eventLoop;

    std::mutex m_lock;

    sd_bus_message *m_reply;
    bool m_sentReply;

    ASRequest::Method m_method;
    std::string m_urlPath;
    std::string m_body;
    ASRequest::HeaderMap m_headers;
    ASRequest::QueryStringMap m_queryParams;
};

#endif // ASREQUEST_P_H
