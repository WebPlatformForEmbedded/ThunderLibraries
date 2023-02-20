//
//  dbusfiledescriptor.h
//  VoiceSearchDaemon
//
//  Copyright © 2019 Sky UK. All rights reserved.
//

#ifndef DBUSFILEDESCRIPTOR_H
#define DBUSFILEDESCRIPTOR_H


class DBusFileDescriptor
{
public:
    DBusFileDescriptor();
    explicit DBusFileDescriptor(int fd);
    DBusFileDescriptor(const DBusFileDescriptor &other);
    DBusFileDescriptor &operator=(DBusFileDescriptor &&other);
    DBusFileDescriptor &operator=(const DBusFileDescriptor &other);
    ~DBusFileDescriptor();

public:
    bool isValid() const;
    int fd() const;

    void reset();
    void clear();

private:
    int m_fd;
};

#endif // DBUSFILEDESCRIPTOR_H
