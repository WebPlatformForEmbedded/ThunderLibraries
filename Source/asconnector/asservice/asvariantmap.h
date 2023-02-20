//
//  asvariantmap.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef ASVARIANTMAP_H
#define ASVARIANTMAP_H

#include <map>
#include <string>


class ASVariantMap
{
public:
    ASVariantMap() = default;
    ASVariantMap(const ASVariantMap &other) = default;
    ASVariantMap(ASVariantMap &&other) = default;

public:
    void insert(const std::string &key, bool value);
    void insert(std::string &&key, bool value);

    void insert(const std::string &key, int value);
    void insert(std::string &&key, int value);

    void insert(const std::string &key, const std::string &value);
    void insert(std::string &&key, std::string &&value);

    void clear();

    bool empty() const;

public:
    struct Visitor
    {
        virtual void operator()(const std::string& k, bool v)               { }
        virtual void operator()(const std::string& k, int v)                { }
        virtual void operator()(const std::string& k, double v)             { }
        virtual void operator()(const std::string& k, const std::string& v) { }
    };

    void visit(Visitor &&visitor) const;

private:
    struct Variant
    {
        enum Type { Boolean, Integer, Double, String } type;
        union BasicType
        {
            bool boolean;
            int integer;
            double real;
            BasicType() = default;
            explicit BasicType(bool b)   : boolean(b) { }
            explicit BasicType(int i)    : integer(i) { }
            explicit BasicType(double d) : real(d)    { }
        } basic;
        std::string str;

        explicit Variant(bool b)                  : type(Boolean), basic(b) { }
        explicit Variant(int i)                   : type(Integer), basic(i) { }
        explicit Variant(double d)                : type(Double),  basic(d) { }

        explicit Variant(const char *str_)        : type(String),  str(str_) { }
        explicit Variant(const std::string &str_) : type(String),  str(str_) { }
        explicit Variant(std::string &&str_)      : type(String),  str(std::move(str_)) { }
    };

    std::map<std::string, Variant> m_map;
};

#endif // ASVARIANTMAP_H
