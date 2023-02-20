//
//  asvariantmap.cpp
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "asvariantmap.h"


bool ASVariantMap::empty() const
{
    return m_map.empty();
}

void ASVariantMap::clear()
{
    m_map.clear();
}

void ASVariantMap::insert(const std::string &key, bool value)
{
    m_map.emplace(key, value);
}

void ASVariantMap::insert(std::string &&key, bool value)
{
    m_map.emplace(std::move(key), value);
}

void ASVariantMap::insert(const std::string &key, int value)
{
    m_map.emplace(key, value);
}

void ASVariantMap::insert(std::string &&key, int value)
{
    m_map.emplace(std::move(key), value);
}

void ASVariantMap::insert(const std::string &key, const std::string &value)
{
    m_map.emplace(key, value);
}

void ASVariantMap::insert(std::string &&key, std::string &&value)
{
    m_map.emplace(std::move(key), std::move(value));
}

void ASVariantMap::visit(Visitor &&visitor) const
{
    for (const auto &entry : m_map)
    {
        const std::string &key = entry.first;
        const Variant &value = entry.second;

        switch (value.type)
        {
            case Variant::Boolean:
                visitor.operator()(key, value.basic.boolean);
                break;
            case Variant::Integer:
                visitor.operator()(key, value.basic.integer);
                break;
            case Variant::Double:
                visitor.operator()(key, value.basic.real);
                break;
            case Variant::String:
                visitor.operator()(key, value.str);
                break;
        }
    }
}