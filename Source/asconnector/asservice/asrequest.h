//
//  asrequest.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef ASREQUEST_H
#define ASREQUEST_H

#include <map>
#include <string>
#include <memory>
#include <cstring>


class ASRequestPrivate;

class ASRequest
{
public:

    ASRequest(const ASRequest &other);
    ASRequest(ASRequest &&other);
    ~ASRequest() = default;

public:

    enum Method { InvalidMethod, HttpPost, HttpGet };

    Method method() const;

public:

    struct CaselessComp
    {
        bool operator() (const std::string& lhs, const std::string& rhs) const
        {
            return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
        }
    };

    typedef std::multimap<std::string, std::string> QueryStringMap;
    typedef std::multimap<std::string, std::string, CaselessComp> HeaderMap;

    std::string path() const;
    std::string body() const;
    HeaderMap headers() const;
    QueryStringMap queryParams() const;

public:
    bool sendReply(unsigned int code) const;
    bool sendReply(unsigned int code, const std::string &body) const;
    bool sendReply(unsigned int code, const HeaderMap &headers, const std::string &body = std::string()) const;

    enum ErrorType
    {
        None,
        InvalidUrlError,
        InvalidParametersError,
        GenericFailureError,
        NotSupportedError
    };

    bool sendErrorReply(ErrorType type, const std::string &developerMessage = std::string()) const;
    bool sendErrorReply(int httpCode, int errorCode,
                        const std::string &userMessage,
                        const std::string &developerMessage = std::string()) const;

private:
    friend class ASServicePrivate;
    // friend int ASServicePrivate::request(sd_bus_message *msg, void *userData, sd_bus_error *retError);

    ASRequest(std::shared_ptr<ASRequestPrivate> &&priv);

    std::shared_ptr<ASRequestPrivate> m_private;
};


#endif // ASREQUEST_H
