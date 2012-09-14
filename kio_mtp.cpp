/*
 *  Main implementation for KIO-MTP
 *  Copyright (C) 2012  Philipp Schmidt <philschmidt@gmx.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "kio_mtp.h"
#include "metatypes.h"

#include <kcomponentdata.h>
#include <QCoreApplication>
#include <QThread>
#include <QFileInfo>
#include <QDateTime>
#include <KTemporaryFile>

#include <sys/stat.h>
#include <sys/types.h>

#include <libmtp.h>


extern "C"
int KDE_EXPORT kdemain( int argc, char **argv )
{
    KComponentData instance( "kio_mtp" );

    KGlobal::locale();
    QCoreApplication app( argc, argv );

    if (argc != 4)
    {
        fprintf( stderr, "Usage: kio_mtp protocol domain-socket1 domain-socket2\n");
        exit( -1 );
    }

    MTPSlave slave( argv[2], argv[3] );
    slave.dispatchLoop();
    return 0;
}

MTPSlave::MTPSlave(const QByteArray& pool, const QByteArray& app)
    : SlaveBase( "mtp", pool, app )
{
    kDebug(KIO_MTP) << "Slave started";

    LIBMTP_Init();

    kDebug(KIO_MTP) << "LIBMTP initialized";
}

MTPSlave::~MTPSlave()
{
}

void getUDSEntry(UDSEntry &entry, const LIBMTP_file_t *file)
{
}

QMap<QString, LIBMTP_raw_device_t*> getRawDevices()
{
    kDebug(KIO_MTP) << "getRawDevices()";

    LIBMTP_raw_device_t *rawdevices;
    int numrawdevices;
    LIBMTP_error_number_t err;

    QMap<QString, LIBMTP_raw_device_t*> devices;

    err = LIBMTP_Detect_Raw_Devices(&rawdevices, &numrawdevices);
    switch(err) {
        case LIBMTP_ERROR_CONNECTING:
//             WARNING("There has been an error connecting to the devices");
            break;
        case LIBMTP_ERROR_MEMORY_ALLOCATION:
//             WARNING("Encountered a Memory Allocation Error");
            break;
        case LIBMTP_ERROR_NONE:
            {
                for (int i = 0; i < numrawdevices; i++)
                {
                    LIBMTP_mtpdevice_t *device = LIBMTP_Open_Raw_Device_Uncached( &rawdevices[i] );

                    char *deviceName = LIBMTP_Get_Friendlyname( device );
                    char *deviceModel = LIBMTP_Get_Modelname( device );

                    // prefer friendly devicename over model
                    QString name;
                    if ( !deviceName )
                        name = QString::fromUtf8(deviceModel);
                    else
                        name = QString::fromUtf8(deviceName);

                    devices.insert(name, &rawdevices[i]);

                    LIBMTP_Release_Device(device);
                }
            }
            break;
        case LIBMTP_ERROR_GENERAL:
        default:
//             WARNING("Unknown connection error");
            break;
    }
    return devices;
}

QMap<QString, LIBMTP_devicestorage_t*> getDevicestorages( LIBMTP_mtpdevice_t *device )
{
    kDebug(KIO_MTP) << "getDevicestorages()";

    QMap<QString, LIBMTP_devicestorage_t*> storages;
    if (device != NULL)
    {
        for (LIBMTP_devicestorage_t* storage = device->storage; storage != NULL; storage = storage->next)
        {
            char *storageIdentifier = storage->VolumeIdentifier;
            char *storageDescription = storage->StorageDescription;

            QString storagename;
            if ( !storageIdentifier )
                storagename = QString::fromUtf8( storageDescription );
            else
                storagename = QString::fromUtf8( storageIdentifier );

            kDebug(KIO_MTP) << "found storage" << storagename;

            storages.insert( storagename, storage );
        }
    }

    return storages;
}

QMap<QString, LIBMTP_file_t*> getFiles( LIBMTP_mtpdevice_t *device, LIBMTP_devicestorage_t *storage, uint32_t parent_id = 0xFFFFFFFF )
{
    kDebug(KIO_MTP) << "getFiles() for parent" << parent_id;

    QMap<QString, LIBMTP_file_t*> files;

    LIBMTP_file_t *file = LIBMTP_Get_Files_And_Folders(device, storage->id, parent_id);
    for (; file != NULL; file = file->next)
    {
        files.insert(QString::fromUtf8(file->filename), file);
//         kDebug(KIO_MTP) << "found file" << file->filename;
    }

    return files;
}

/**
 * @brief Get's the correct object from the device.
 * !Important! Release Device after using the returned object
 * @param pathItems A QStringList containing the items of the filepath
 * @return nullptr if the object doesn't exist or for root or, depending on the pathItems size device (1), storage (2) or file (>=3)
 */
QPair<void*, LIBMTP_mtpdevice_t*> getPath ( const QStringList& pathItems )
{
    kDebug(KIO_MTP) << "[ENTER]";

    QPair<void*, LIBMTP_mtpdevice_t*> ret;

    // Don' handle the root directory
    if (pathItems.size() <= 0)
    {
        return ret;
    }

    QMap<QString, LIBMTP_raw_device_t*> devices = getRawDevices();

    if ( devices.contains( pathItems.at(0) ) )
    {
        LIBMTP_mtpdevice_t *device = LIBMTP_Open_Raw_Device_Uncached( devices.value( pathItems.at(0) ) );

        // return specific device
        if ( pathItems.size() == 1 )
        {
            ret.first = device;
            ret.second = device;
        }

        QMap<QString, LIBMTP_devicestorage_t*> storages = getDevicestorages( device );

        if (storages.contains( pathItems.at(1) ) )
        {
            LIBMTP_devicestorage_t *storage = storages.value( pathItems.at(1) );

            if ( pathItems.size() == 2 )
            {
                ret.first = storage;
                ret.second = device;
            }

            int currentLevel = 2, currentParent = 0xFFFFFFFF;

            QMap<QString, LIBMTP_file_t*> files;
            LIBMTP_file_t *file;

            // traverse further while depth not reached
            while ( currentLevel < pathItems.size() )
            {
                files = getFiles( device, storage, currentParent );

                if ( files.contains( pathItems.at(currentLevel) ) )
                {
                    file = files.value( pathItems.at( currentLevel ) );
                    currentParent = file->item_id;
                }
                else
                {
                    return ret;
                }
                currentLevel++;
            }

            ret.first = file;
            ret.second = device;
        }
    }

    return ret;
}

void MTPSlave::listDir( const KUrl& url )
{
    QStringList pathItems = url.path().split('/', QString::SkipEmptyParts);

    kDebug(KIO_MTP) << "listDir()" << url.path() << pathItems.size();

    UDSEntry entry;

    QMap<QString, LIBMTP_raw_device_t*> devices = getRawDevices();

    // list devices
    if (pathItems.size() == 0)
    {
        kDebug(KIO_MTP) << "Root directory, listing devices";

        foreach ( const QString &deviceName, devices.keys() )
        {
            // list device
            entry.insert( UDSEntry::UDS_NAME, deviceName );
            entry.insert( UDSEntry::UDS_ICON_NAME, QString( "multimedia-player" ) );
            entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFDIR );
            entry.insert( UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH );
            entry.insert( UDSEntry::UDS_MIME_TYPE, "inode/directory" );

            listEntry(entry, false);
            entry.clear();
        }

        listEntry(entry, true);
        finished();
    }
    // traverse into device
    else if ( devices.contains( pathItems.at(0) ) )
    {
        LIBMTP_mtpdevice_t *device = LIBMTP_Open_Raw_Device_Uncached( devices.value( pathItems.at(0) ) );

        QMap<QString, LIBMTP_devicestorage_t*> storages = getDevicestorages( device );

        // list storages
        if ( pathItems.size() == 1)
        {
            kDebug(KIO_MTP) << "Listing storages for device " << pathItems.at(0);

            foreach (const QString &storageName, storages.keys())
            {
                kDebug(KIO_MTP) << "Showing " << storageName;

                // list storage
                entry.insert( UDSEntry::UDS_NAME, storageName );
                entry.insert( UDSEntry::UDS_ICON_NAME, QString( "drive-removable-media" ) );
                entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFDIR );
                entry.insert( UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH );
                entry.insert( UDSEntry::UDS_MIME_TYPE, "inode/directory" );

                listEntry(entry, false);
                entry.clear();
            }

            listEntry(entry, true);
            finished();
        }
        // traverse into storage
        else if ( storages.contains( pathItems.at(1) ) )
        {
            LIBMTP_devicestorage_t *storage = storages.value( pathItems.at(1) );

            int currentLevel = 2, currentParent = 0xFFFFFFFF;

            QMap<QString, LIBMTP_file_t*> files = getFiles( device, storage, currentParent );

            // traverse further while depth not reached
            while ( currentLevel < pathItems.size() )
            {
                if ( files.contains( pathItems.at(currentLevel) ) )
                {
                    // set new parent and get filelisting
                    currentParent = files.value( pathItems.at( currentLevel++ ) )->item_id;
                    files = getFiles( device, storage, currentParent );
                }
                else {
                    error(ERR_CANNOT_ENTER_DIRECTORY, url.path());
                    return;
                }
            }

            kDebug(KIO_MTP) << "Showing" << files.size() << "files";

            foreach ( LIBMTP_file_t *file, files.values() ) {
                kDebug(KIO_MTP) << file->filename;

                entry.insert( UDSEntry::UDS_NAME, QString::fromUtf8(file->filename) );
                if (file->filetype == LIBMTP_FILETYPE_FOLDER)
                {
                    entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFDIR );
                    entry.insert( UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IRWXO );
                }
                else
                {
                    entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFREG );
                    entry.insert( UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH );
                    entry.insert( UDSEntry::UDS_SIZE, file->filesize);
                }
                entry.insert( UDSEntry::UDS_INODE, file->item_id);
                entry.insert( UDSEntry::UDS_ACCESS_TIME, file->modificationdate);
                entry.insert( UDSEntry::UDS_MODIFICATION_TIME, file->modificationdate);
                entry.insert( UDSEntry::UDS_CREATION_TIME, file->modificationdate);

                listEntry(entry, false);
                entry.clear();
            }

            listEntry(entry, true);
            finished();
        }
        else
        {
            error(ERR_CANNOT_ENTER_DIRECTORY, url.path());
        }

        LIBMTP_Release_Device( device );
    }
    else
    {
        error(ERR_CANNOT_ENTER_DIRECTORY, url.path());
    }

    kDebug(KIO_MTP) << "[EXIT]";
}

// void MTPSlave::mimetype(const KUrl& url)
// {
// }
//
// void MTPSlave::stat( const KUrl& url )
// {
//     kDebug(KIO_MTP) << "stat()";
//
//     SlaveBase::stat( url );
// }
//
// void MTPSlave::put(const KUrl& url, int permissions, JobFlags flags)
// {
// }
//
// void MTPSlave::get(const KUrl& url)
// {
// }

void MTPSlave::copy(const KUrl& src, const KUrl& dest, int permissions, JobFlags flags)
{
}

void MTPSlave::mkdir(const KUrl& url, int permissions)
{
    kDebug(KIO_MTP) << "[ENTER]";

    QStringList pathItems = url.path().split('/', QString::SkipEmptyParts);

    if ( pathItems.size() > 2 )
    {
        const char* temp = pathItems.takeLast().toStdString().c_str();
        char *dirName = (char*)malloc( strlen( temp ) );
        strcpy( dirName, temp);

        LIBMTP_mtpdevice_t *device;
        LIBMTP_file_t *file;

        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath( pathItems );

        if ( pair.first )
        {
            file = (LIBMTP_file_t*)pair.first;
            device = pair.second;

            if (file && file->filetype == LIBMTP_FILETYPE_FOLDER)
            {
                kDebug(KIO_MTP) << "Found parent" << file->item_id << file->filename;
                kDebug(KIO_MTP) << "Attempting to create folder" << dirName;

                int ret = LIBMTP_Create_Folder(device, dirName, file->item_id, file->storage_id);
                if ( ret == 0 )
                {
                    LIBMTP_Dump_Errorstack(device);
                    LIBMTP_Clear_Errorstack(device);

                    error( ERR_COULD_NOT_MKDIR, url.path() );
                    return;
                }
                else
                {
                    kDebug(KIO_MTP) << "Created folder" << ret;
                }
            }
            else
            {
                error( ERR_COULD_NOT_MKDIR, url.path() );
                return;
            }
        }
        else
        {
            error( ERR_COULD_NOT_MKDIR, url.path() );
            return;
        }
    }
    finished();
}


#include "kio_mtp.moc"
