//
//  safesemaphore_p.h
//  ASServiceLib
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef SAFESEMAPHORE_P_H
#define SAFESEMAPHORE_P_H

#include <pthread.h>


// -----------------------------------------------------------------------------
/*!
    \class SafeSemaphore
    \brief Simply object that creates a semaphore object whose timed waits
    uses the monotonic clock.

    This is basically just the code described here
    https://stackoverflow.com/questions/40698438/sem-timedwait-with-clock-monotonic-raw-clock-monotonic

 */


namespace sky
{

    class SafeSemaphore
    {
    public:
        explicit SafeSemaphore(int count = 0)
            : m_mutex(PTHREAD_MUTEX_INITIALIZER)
            , m_cond(PTHREAD_COND_INITIALIZER)
            , m_count(count)
        {
            pthread_condattr_t attr;
            pthread_condattr_init(&attr);
            pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
            pthread_cond_init(&m_cond, &attr);
            pthread_condattr_destroy(&attr);
        }

        ~SafeSemaphore()
        {
            pthread_mutex_destroy(&m_mutex);
            pthread_cond_destroy(&m_cond);
        }

        void notify()
        {
            pthread_mutex_lock(&m_mutex);
            m_count++;
            pthread_cond_signal(&m_cond);
            pthread_mutex_unlock(&m_mutex);
        }

        bool wait(int timeoutMs)
        {
            if (timeoutMs < 0)
            {
                pthread_mutex_lock(&m_mutex);
                while (!m_count)
                {
                    pthread_cond_wait(&m_cond, &m_mutex);
                }
            }
            else
            {
                struct timespec tsTimeout;
                clock_gettime(CLOCK_MONOTONIC, &tsTimeout);
                tsTimeout.tv_sec += (timeoutMs / 1000);
                tsTimeout.tv_nsec += (timeoutMs % 1000) * 1000000;
                if (tsTimeout.tv_nsec > 1000000000)
                {
                    tsTimeout.tv_sec += 1;
                    tsTimeout.tv_nsec -= 1000000000;
                }

                pthread_mutex_lock(&m_mutex);
                while (!m_count)
                {
                    if (pthread_cond_timedwait(&m_cond, &m_mutex, &tsTimeout) == ETIMEDOUT)
                        break;
                }
            }

            const bool result = (m_count > 0);
            if (m_count > 0)
                m_count--;

            pthread_mutex_unlock(&m_mutex);

            return result;
        }

    private:
        pthread_mutex_t m_mutex;
        pthread_cond_t m_cond;
        int m_count;

    };

}

#endif // SAFESEMAPHORE_P_H
