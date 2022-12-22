/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Metrological B.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#pragma once

#ifndef MODULE_NAME
#define MODULE_NAME Compositor_BufferType
#endif

#include <core/core.h>

#include "IBuffer.h"

#include <sys/eventfd.h>
#include <sys/mman.h>

namespace WPEFramework {

namespace Compositor {

    template <const uint8_t PLANES>
    class BufferType : public ::Compositor::Interfaces::IBuffer, public Core::IResource {
    private:
        using EventFrame = uint8_t;

        class Iterator : public ::Compositor::Interfaces::IBuffer::IIterator {
        private:
            class PlaneImplementation : public ::Compositor::Interfaces::IBuffer::IPlane {
            public:
                PlaneImplementation(PlaneImplementation&&) = delete;
                PlaneImplementation(const PlaneImplementation&) = delete;
                PlaneImplementation& operator=(const PlaneImplementation&) = delete;

                PlaneImplementation()
                    : _parent(nullptr)
                    , _index(-1)
                {
                }
                ~PlaneImplementation() override = default;

            public:
                void Define(BufferType<PLANES>& parent, const uint8_t index)
                {
                    _parent = &parent;
                    _index = index;
                }
                buffer_id Accessor() const override
                { // Access to the actual data.
                    ASSERT(_parent != nullptr);
                    return (_parent->FileDescriptor(_index));
                }
                uint32_t Stride() const override
                { // Bytes per row for a plane [(bit-per-pixel/8) * width]
                    ASSERT(_parent != nullptr);
                    return (_parent->Stride(_index));
                }
                uint32_t Offset() const override
                { // Offset of the plane from where the pixel data starts in the buffer.
                    ASSERT(_parent != nullptr);
                    return (_parent->Offset(_index));
                }

            private:
                BufferType<PLANES>* _parent;
                uint8_t _index;
            };

        public:
            Iterator() = delete;
            Iterator(Iterator&&) = delete;
            Iterator(const Iterator&) = delete;
            Iterator& operator=(const Iterator&) = delete;

            Iterator(BufferType<PLANES>& parent)
                : _parent(parent)
            {
                // Fill our elements
                for (uint8_t index = 0; index < _parent.Planes(); index++) {
                    _planes[index].Define(_parent, index);
                }

                Reset();
            }
            ~Iterator() override = default;

        public:
            bool IsValid() const override
            {
                return ((_position > 0) && (_position <= _parent.Planes()));
            }
            void Reset() override
            {
                _position = 0;
            }
            bool Next() override
            {
                if (_position <= _parent->Planes()) {
                    _position++;
                }
                return (IsValid());
            }
            IPlane* Plane() override
            {
                ASSERT(IsValid() == true);
                return (&(_planes[_position - 1]));
            }

        private:
            BufferType<PLANES>& _parent;
            PlaneImplementation _planes[PLANES];
            uint8_t _position;
        };

    public:
        BufferType() = delete;
        BufferType(const BufferType<PLANES>&) = delete;
        BufferType<PLANES>& operator=(const BufferType<PLANES>&) = delete;

        BufferType(const string& callsign, const uint32_t id, const uint32_t width, const uint32_t height, const uint32_t format, const uint64_t modifier)
            : _id(id)
            , _planeCount(0)
            , _iterator(*this)
            , _virtualFd(-1)
            , _eventFd(-1)
            , _storage(nullptr)
        {
            string definition = _T("NotifiableBuffer") + callsign;
            _virtualFd = ::memfd_create(definition.c_str(), MFD_ALLOW_SEALING | MFD_CLOEXEC);
            if (_virtualFd != -1) {
                int length = sizeof(struct SharedStorage);

                /* Size the file as specified by our struct. */
                if (::ftruncate(_virtualFd, length) != -1) {
                    /* map that file to a memory area we can directly access as a memory mapped file */
                    _storage = reinterpret_cast<struct SharedStorage*>(::mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, _virtualFd, 0));
                    if (_storage != MAP_FAILED) {
                        _eventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
                        if (_eventFd != -1) {
                            ::memset(_storage, 0, length);
                            _storage->_width = width;
                            _storage->_height = height;
                            _storage->_format = format;
                            _storage->_modifier = modifier;
                            _storage->_dirty = ATOMIC_FLAG_INIT;
                            if (::pthread_mutex_init(&(_storage->_mutex), nullptr) != 0) {
                                // That will be the day, if this fails...
                                ASSERT(false);
                            }
                        }
                    }
                }
            }
        }
        BufferType(const uint32_t id, const Core::PrivilegedRequest::Container& descriptors)
            : _id(id)
            , _planeCount(0)
            , _iterator(*this)
            , _virtualFd(descriptors[0])
            , _eventFd(descriptors[1])
            , _storage(nullptr)
        {
            ASSERT(descriptors.size() >= 3);
            ASSERT(_virtualFd != -1);
            ASSERT(_eventFd != -1);

            _storage = reinterpret_cast<struct SharedStorage*>(::mmap(nullptr, sizeof(struct SharedStorage), PROT_READ | PROT_WRITE, MAP_SHARED, _virtualFd, 0));
            if (_storage == MAP_FAILED) {
                ::close(_eventFd);
                _eventFd = -1;
                for (const auto& entry : descriptors) {
                    ::close(entry);
                }
            } else
                for (const auto& entry : descriptors) {
                    _planes[_planeCount] = entry;
                    _planeCount++;
                }
        }
        ~BufferType() override
        {
            if (_eventFd != -1) {
                ::close(_eventFd);
                _eventFd = -1;

                ASSERT(_storage != nullptr);

#ifdef __WINDOWS__
                ::CloseHandle(&(_storage->_mutex));
#else
                ::pthread_mutex_destroy(&(_storage->_mutex));
#endif
            }
            if (_storage != nullptr) {
                ::munmap(_storage, sizeof(struct SharedStorage));
                _storage = nullptr;
            }
            if (_virtualFd != -1) {
                ::close(_virtualFd);
                _virtualFd = -1;
            }

            // Close all the FileDescriptors handedn over to us for the planes.
            for (uint8_t index = 0; index < _planeCount; index++) {
                ::close(_planes[index]);
            }
        }

    public:
        bool IsValid() const
        {
            return (_eventFd != -1);
        }
        Core::PrivilegedRequest::Container Descriptors() const
        {
            ASSERT(IsValid() == true);
            Core::PrivilegedRequest::Container result({ _virtualFd, _eventFd });
            for (uint8_t index = 0; index < _planeCount; index++) {
                result.emplace_back(_planes[index]);
            }
            return (result);
        }

        //
        // Implementation of Core::IResource
        // -----------------------------------------------------------------
        handle Descriptor() const override
        {
            return (_eventFd);
        }
        uint16_t Events() override
        {
            return (POLLIN);
        }
        void Handle(const uint16_t events) override
        {
            EventFrame value;
            if (((events & POLLIN) != 0) && (::read(_eventFd, &value, sizeof(EventFrame)) == sizeof(EventFrame)) && (_storage->test_and_set() == false)) {
                // This is a communication thread, do not wait for more then 10mS to get the lock,
                // If we do not get it, just bail out..
                if (Lock(10) == Core::ERROR_NONE) {
                    Render();
                    Unlock();
                }
            }
        }

        //
        // Implementation of Core::IResource
        // -----------------------------------------------------------------
        // Wait time in milliseconds.
        IIterator* Planes(const uint32_t waitTimeInMs) override
        { // Access to the buffer planes.
            if (Lock(waitTimeInMs) == Core::ERROR_NONE) {
                _iterator.Reset();
                return (&_iterator);
            }
            return (nullptr);
        }
        virtual uint32_t Completed(const bool dirty)
        {
            Unlock();
            if (dirty == true) {
                EventFrame value = 1;
                _storage->_dirty.clear();
                size_t result = ::write(_storage->_eventFd, &value, sizeof(value));
                return (result != sizeof(value) ? Core::ERROR_ILLEGAL_STATE : Core::ERROR_NONE);
            }
            return (Core::ERROR_NONE);
        }
        virtual void Render() = 0;

        uint32_t Identifier() const override
        {
            return (_id);
        }
        uint32_t Width() const override
        { // Width of the allocated buffer in pixels
            return (_storage->_width);
        }
        uint32_t Height() const override
        { // Height of the allocated buffer in pixels
            return (_storage->_height);
        }
        uint32_t Format() const override
        { // Layout of a pixel according the fourcc format
            return (_storage->_format);
        }
        uint64_t Modifier() const override
        { // Pixel arrangement in the buffer, used to optimize for hardware
            return (_storage->_modifier);
        }
        uint8_t Planes() const
        {
            return (_planeCount);
        }

    protected:
        void Add(int fd, const uint32_t stride, const uint32_t offset)
        {
            ASSERT(fd > 0);
            ASSERT((_planeCount + 1) <= PLANES);
            _storage->_planes[_planeCount]._stride = stride;
            _storage->_planes[_planeCount]._offset = offset;
            _planes[_planeCount] = fd;
            _planeCount++;
        }

    private:
        buffer_id Accessor(const uint8_t index) const
        { // Access to the actual data.
            ASSERT(index < _planeCount);
            return (_planes[index]);
        }
        uint32_t Stride(const uint8_t index) const
        { // Bytes per row for a plane [(bit-per-pixel/8) * width]
            ASSERT(index < _planeCount);
            return (_storage->_planes[index]._stride);
        }
        uint32_t Offset(const uint8_t index) const
        { // Offset of the plane from where the pixel data starts in the buffer.
            ASSERT(index < _planeCount);
            return (_storage->_planes[index]._offset);
        }
        uint32_t Lock(uint32_t timeout)
        {
            timespec structTime;

#ifdef __WINDOWS__
            return (::WaitForSingleObjectEx(&(_storage->_mutex), timeout, FALSE) == WAIT_OBJECT_0 ? Core::ERROR_NONE : Core::ERROR_TIMEDOUT);
#else
            clock_gettime(CLOCK_MONOTONIC, &structTime);
            structTime.tv_nsec += ((timeout % 1000) * 1000 * 1000); /* remainder, milliseconds to nanoseconds */
            structTime.tv_sec += (timeout / 1000) + (structTime.tv_nsec / 1000000000); /* milliseconds to seconds */
            structTime.tv_nsec = structTime.tv_nsec % 1000000000;
            int result = pthread_mutex_timedlock(&(_storage->_mutex), &structTime);
            return (result == 0 ? Core::ERROR_NONE : Core::ERROR_TIMEDOUT);
#endif
        }
        uint32_t Unlock() const
        {
#ifdef __WINDOWS__
            ::LeaveCriticalSection(&_storage->_mutex);
#else
            pthread_mutex_unlock(&(_storage->_mutex));
#endif
            return (Core::ERROR_NONE);
        }

    private:
        struct PlaneStorage {
            uint32_t _stride;
            uint32_t _offset;
        };
        // We need some shared space for data to exchange, and to create a lock..
        struct SharedStorage {
            uint32_t _width;
            uint32_t _height;
            uint32_t _format;
            uint64_t _modifier;
            PlaneStorage _planes[PLANES];
#ifdef __WINDOWS__
            CRITICAL_SECTION _mutex;
#else
            pthread_mutex_t _mutex;
#endif
            std::atomic_flag _dirty;
        };

        uint32_t _id;
        uint8_t _planeCount;
        Iterator _iterator;

        // We need a descriptor that is pointing to the virtual memory (shared memory)
        int _virtualFd;

        // We need a descriptor we can wait for and that can be signalled
        int _eventFd;

        // From the virtual memory we can map the shared data to a memory area in "our" process.
        SharedStorage* _storage;

        int _planes[PLANES];
    };

}
}
