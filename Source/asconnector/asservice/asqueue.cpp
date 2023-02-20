// File: AS_Queue.cpp
//
// Copyright © 2019 Sky UK. All rights reserved.

#include "asservice/asqueue.h"

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <cstring>

class QueuePrivate
{

private:

    // -----------------------------------------------------------------------------
    /*!
        \internal

        Structure prefixed onto the payload before storing in the queue

     */
    struct MessageHeader
    {
        uint32_t event;
        size_t payloadSize;
        uint8_t payload[];
    };


    const bool m_prioritisedQueue;

    std::queue< std::unique_ptr<uint8_t[]> > m_lowPriorityQueue;
    std::queue< std::unique_ptr<uint8_t[]> > m_highPriorityQueue;

    std::mutex m_lock;
    std::condition_variable m_cond;



public:

    explicit QueuePrivate( bool prioritisedQueue )
        : m_prioritisedQueue( prioritisedQueue )
    {
    }

    // -----------------------------------------------------------------------------
    /*!
        \internal

        Attempts to pull a message of the queue.

     */
    size_t recvTimeout( uint32_t *event, void *payload, size_t *payloadSize, int timeout )
    {
        std::unique_lock<std::mutex> locker(m_lock);

        // wait for something if a timeout was specified
        if ( timeout > 0 )
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
            while ( m_lowPriorityQueue.empty() && m_highPriorityQueue.empty() &&
                    (std::chrono::steady_clock::now() < deadline) )
            {
                m_cond.wait_until( locker, deadline );
            }
        }
        else if ( timeout < 0 )
        {
            while ( m_lowPriorityQueue.empty() && m_highPriorityQueue.empty() )
            {
                m_cond.wait( locker );
            }
        }

        // take the message off the queue
        std::unique_ptr<uint8_t[]> message;
        if ( !m_highPriorityQueue.empty() )
        {
            message = std::move( m_highPriorityQueue.front() );
            m_highPriorityQueue.pop();
        }
        else if ( !m_lowPriorityQueue.empty() )
        {
            message = std::move( m_lowPriorityQueue.front() );
            m_lowPriorityQueue.pop();
        }

        // can now drop the lock
        locker.unlock();


        // check if anything was in the queue, if not we're done
        if ( !message )
        {
            return 0;
        }

        size_t copied = 0;

        auto header = reinterpret_cast<MessageHeader*>( message.get() );
        if ( header->payloadSize > *payloadSize )
        {
            // according to the fusion ITC documentation 'If the message is longer than
            // the specified length it will be discarded' and an error is returned
            *payloadSize = header->payloadSize;
        }
        else
        {
            copied = header->payloadSize;
            *event = header->event;
            *payloadSize = header->payloadSize;
            if ( payload ) {
                memcpy(payload, header->payload, copied);
            }
        }

        return copied;
    }

    // -----------------------------------------------------------------------------
    /*!
        \internal

        Adds an item to the queue.

     */
    size_t send( uint32_t event, void *payload, size_t payloadSize, bool prioritySend )
    {
        // sanity check
        if (payloadSize > (1 * 1024 * 1024))
        {
            return 0;
        }

        // allocate memory for the message details and payload
        auto message = std::make_unique<uint8_t[]>(sizeof(MessageHeader) + payloadSize);

        // copy in the data
        auto *header = reinterpret_cast<MessageHeader*>(message.get());
        header->event = event;
        header->payloadSize = payloadSize;
        if ( payloadSize > 0 ) {
            memcpy(header->payload, payload, payloadSize);
        }


        std::unique_lock<std::mutex> locker(m_lock);

        // add to the queue
        if (m_prioritisedQueue && prioritySend)
            m_highPriorityQueue.emplace(std::move(message));
        else
            m_lowPriorityQueue.emplace(std::move(message));

        // wake up any threads listening for a message
        m_cond.notify_all();

        return payloadSize;
    }

};


Queue::Queue( bool prioritisedQueue )
    : m_private( new QueuePrivate( prioritisedQueue ) )
{
}

Queue::~Queue()
{
    delete m_private;
}

size_t Queue::Send( uint32_t event, void *payload, size_t payloadSize, bool prioritySend )
{
    return m_private->send( event, payload, payloadSize, prioritySend );
}

size_t Queue::RecvTimeout( uint32_t *event, void *payload, size_t *payloadSize, int timeout )
{
    return m_private->recvTimeout( event, payload, payloadSize, timeout );
}

size_t Queue::Recv( uint32_t *event, void *payload, size_t *payloadSize )
{
    return m_private->recvTimeout( event, payload, payloadSize, -1 );
}

size_t Queue::TryRecv( uint32_t *event, void *payload, size_t *payloadSize )
{
    return m_private->recvTimeout( event, payload, payloadSize, 0 );
}

