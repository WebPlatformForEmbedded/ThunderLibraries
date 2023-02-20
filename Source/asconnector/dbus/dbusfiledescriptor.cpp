//
//  dbusfiledescriptor.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#include "dbusfiledescriptor.h"
#include "sky/log.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>



// -----------------------------------------------------------------------------
/*!
    \class DBusFileDescriptor
    \brief Light wrapper around a file descriptor so it can be used safely with
    dbus.

    Why do we need this?  Because we want to safely pass a file descriptor
    around using the DBusMessage class.

    Why not just use an integer? Because although it's obviously fine to pass
    an integer around, the life time of the file descriptor can get lost.  This
    class uses \c dup(2) to ensure that if the object was created with a valid
    file descriptor in the first place then it and all copy constructed objects
    will have a valid file descriptor.

 */


// -----------------------------------------------------------------------------
/*!
    Constructs a DBusFileDescriptor without a wrapped file descriptor. This is
    equivalent to constructing the object with an invalid file descriptor
    (like -1).

    \see fd() and isValid()
 */
DBusFileDescriptor::DBusFileDescriptor()
    : m_fd(-1)
{
}

// -----------------------------------------------------------------------------
/*!
    Constructs a DBusFileDescriptor object by copying the fileDescriptor parameter.
    The original file descriptor is not touched and must be closed by the user.

    Note that the value returned by fd() will be different from the \a fd
    parameter passed.

    If the \a fd parameter is not valid, isValid() will return\c false and fd()
    will return \c -1.

    \see fd().
 */
DBusFileDescriptor::DBusFileDescriptor(int fd)
    : m_fd(-1)
{
    if (fd >= 0) {
        m_fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if (m_fd < 0)
            LOG_SYS_ERROR(errno, "failed to dup supplied fd");
    }
}

// -----------------------------------------------------------------------------
/*!
    Constructs a DBusFileDescriptor object by copying \a other.

 */
DBusFileDescriptor::DBusFileDescriptor(const DBusFileDescriptor &other)
    : m_fd(-1)
{
    if (other.m_fd >= 0) {
        m_fd = fcntl(other.m_fd, F_DUPFD_CLOEXEC, 3);
        if (m_fd < 0)
            LOG_SYS_ERROR(errno, "failed to dup supplied fd");
    }
}

// -----------------------------------------------------------------------------
/*!
    Move-assigns \a other to this DBusFileDescriptor.

 */
DBusFileDescriptor &DBusFileDescriptor::operator=(DBusFileDescriptor &&other)
{
    if ((m_fd >= 0) && (::close(m_fd) != 0))
        LOG_SYS_ERROR(errno, "failed to close file descriptor");

    m_fd = other.m_fd;
    other.m_fd = -1;

    return *this;
}

// -----------------------------------------------------------------------------
/*!
    Copies the Unix file descriptor from the \a other DBusFileDescriptor object.
    If the current object contained a file descriptor, it will be properly
    disposed of beforehand.

 */
DBusFileDescriptor &DBusFileDescriptor::operator=(const DBusFileDescriptor &other)
{
    if ((m_fd >= 0) && (::close(m_fd) != 0))
        LOG_SYS_ERROR(errno, "failed to close file descriptor");

    m_fd = -1;

    if (other.m_fd >= 0) {
        m_fd = fcntl(other.m_fd, F_DUPFD_CLOEXEC, 3);
        if (m_fd < 0)
            LOG_SYS_ERROR(errno, "failed to dup supplied fd");
    }

    return *this;
}

// -----------------------------------------------------------------------------
/*!
    Destroys this DBusFileDescriptor object and disposes of the Unix file
    descriptor that it contained.

    \see reset() and clear()
 */
DBusFileDescriptor::~DBusFileDescriptor()
{
    reset();
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if this Unix file descriptor is valid. A valid Unix file
    descriptor is greater than or equal to 0.

    \see fd()
 */
bool DBusFileDescriptor::isValid() const
{
    return (m_fd >= 0);
}

// -----------------------------------------------------------------------------
/*!
    Returns the Unix file descriptor contained by this DBusFileDescriptor object.
    An invalid file descriptor is represented by the value -1.

    Note that the file descriptor returned by this function is owned by the
    DBusFileDescriptor object and must not be stored past the lifetime of this
    object. It is ok to use it while this object is valid, but if one wants to
    store it for longer use, the file descriptor should be cloned using the
    Unix dup(2), dup2(2) or dup3(2) functions.

    \see isValid()
 */
int DBusFileDescriptor::fd() const
{
    return m_fd;
}

// -----------------------------------------------------------------------------
/*!
    Closes the file descriptor contained by this DBusFileDescriptor object.

    After this call isValid() will return \c false and fd() will return \c -1.

    \see clear()
 */
void DBusFileDescriptor::reset()
{
    if ((m_fd >= 0) && (::close(m_fd) != 0))
        LOG_SYS_ERROR(errno, "failed to close file descriptor");

    m_fd = -1;
}

// -----------------------------------------------------------------------------
/*!
    Same as reset, added to match c++11 naming convention.

    \see reset()
 */
void DBusFileDescriptor::clear()
{
    reset();
}
