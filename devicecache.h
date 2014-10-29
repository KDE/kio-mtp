/*
    Cache for recently used devices.
    Copyright (C) 2012  Philipp Schmidt <philschmidt@gmx.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef DEVICECACHE_H
#define DEVICECACHE_H

#include <QPair>
#include <QHash>
#include <QTimer>
#include <QThread>
#include <QEventLoop>

#include <Solid/DeviceNotifier>
#include <Solid/Device>

#include <libmtp.h>


class CachedDevice : public QObject
{
    Q_OBJECT

private:
    qint32 timeout;
    QTimer *timer;
    LIBMTP_mtpdevice_t* mtpdevice;
    LIBMTP_raw_device_t rawdevice;

    QString name;
    QString udi;

public:
    explicit CachedDevice(LIBMTP_mtpdevice_t* device, LIBMTP_raw_device_t* rawdevice, const QString udi, qint32 timeout);
    virtual ~CachedDevice();

    LIBMTP_mtpdevice_t* getDevice();
    const QString getName();
    const QString getUdi();
};


class DeviceCache : public QEventLoop
{
    Q_OBJECT

private:
    /**
     * Fields in order: Devicename (QString), expiration Timer, pointer to device
     */
    QHash< QString, CachedDevice* > nameCache, udiCache;
    
    Solid::DeviceNotifier *notifier;

    qint32 timeout;

public:
    DeviceCache( qint32 timeout, QObject* parent = 0 );
    virtual ~DeviceCache();

    /*
     * Functions for accessing the device
     */
private:
    void checkDevice ( Solid::Device solidDevice );
    
private slots:

    void deviceAdded( const QString &udi );
    void deviceRemoved( const QString &udi );

public:
    QHash< QString, CachedDevice* > getAll();
    CachedDevice* get ( const QString& string, bool isUdi = false );
    bool contains(QString string, bool isUdi = false);
    int size();
};

#endif // DEVICECACHE_H
