// File: ASQueue.h
//
// Copyright © 2019 Sky UK. All rights reserved.

#ifndef AS_QUEUE_H
#define AS_QUEUE_H

#include <memory>

class QueuePrivate;

class Queue
{

private:
    QueuePrivate* m_private;

public:
    size_t RecvTimeout( uint32_t *event, void *payload, size_t *payloadSize, int timeout );

    size_t Send( uint32_t event, void *payload, size_t payloadSize, bool prioritySend = false );
    size_t Recv( uint32_t *event, void *payload, size_t *payloadSize );

    size_t TryRecv( uint32_t *event, void *payload, size_t *msgSize );

    explicit Queue( bool prioritisedQueue = false );
    ~Queue();

    Queue( const Queue &other ) = delete;
    Queue& operator=( const Queue &other ) = delete;
};

#endif // #ifndef AS_QUEUE_H
