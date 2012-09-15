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
#include <QFileInfo>
#include <QDateTime>

#include <sys/stat.h>
#include <sys/types.h>

#include <libmtp.h>

#include "kio_mtp_helpers.cpp"

//////////////////////////////////////////////////////////////////////////////
///////////////////////////// Slave Implementation ///////////////////////////
//////////////////////////////////////////////////////////////////////////////

extern "C"
int KDE_EXPORT kdemain( int argc, char **argv )
{
    KComponentData instance( "kio_mtp" );

    KGlobal::locale();

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
}

MTPSlave::~MTPSlave()
{
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
//                 kDebug(KIO_MTP) << "Showing " << storageName;

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
//                 kDebug(KIO_MTP) << file->filename;

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

void MTPSlave::copy(const KUrl& src, const KUrl& dest, int, JobFlags)
{
    kDebug(KIO_MTP) << "[ENTER]";

    // mtp:/// to mtp:///
    if (src.protocol() == "mtp" && dest.protocol() == "mtp")
    {
        kDebug(KIO_MTP) << "Copy on device";
        // MTP doesn't support moving files directly on the device, so we have to download and then upload...

        error(ERR_UNSUPPORTED_ACTION, "Cannot copy/move files on the device itself");
    }
    // file:/// tp mtp:///
    if (src.protocol() == "file" && dest.protocol() == "mtp")
    {
        QStringList destItems = dest.path().split('/', QString::SkipEmptyParts);

        // Can't copy to root or device, needs storage
        if (destItems.size() < 2)
        {
            error(ERR_UNSUPPORTED_ACTION, dest.path());
            return;
        }

        kDebug(KIO_MTP) << "Copy file " << src.fileName() << "from filesystem to device" << src.directory(KUrl::AppendTrailingSlash) << dest.directory(KUrl::AppendTrailingSlash);

//         destItems.takeLast();

        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath( destItems );

        if (!pair.first)
        {
            error(ERR_DOES_NOT_EXIST, dest.path());
            return;
        }

        LIBMTP_mtpdevice_t *device = pair.second;
        LIBMTP_file_t *parent = (LIBMTP_file_t*)pair.first;
        if (parent->filetype != LIBMTP_FILETYPE_FOLDER)
        {
            error(ERR_IS_FILE, dest.directory());
            return;
        }

        QFileInfo info(src.path());

        LIBMTP_file_t *file = LIBMTP_new_file_t();
        file->parent_id = parent->item_id;
        file->filename = strdup(src.fileName().toStdString().c_str());
        file->filetype = getFiletype(src.fileName());
        file->filesize = info.size();
        file->modificationdate = info.lastModified().toTime_t();
        file->storage_id = parent->storage_id;

        kDebug(KIO_MTP) << "Sending file" << file->filename;

        totalSize(info.size());

        int ret = LIBMTP_Send_File_From_File(device, src.path().toStdString().c_str(), file, (LIBMTP_progressfunc_t) &dataProgress, this);
        if (ret != 0)
        {
            error(KIO::ERR_COULD_NOT_WRITE, dest.fileName());
            LIBMTP_Dump_Errorstack(device);
            LIBMTP_Clear_Errorstack(device);
            return;
        }
    }
    // mtp:/// to file:///
    if (src.protocol() == "mtp" && dest.protocol() == "file")
    {
        kDebug(KIO_MTP) << "Copy file " << src.fileName() << "from device to filesystem" << src.directory(KUrl::AppendTrailingSlash) << dest.directory(KUrl::AppendTrailingSlash);

        QStringList srcItems = src.path().split('/', QString::SkipEmptyParts);

        // Can't copy to root or device, needs storage
        if (srcItems.size() < 2)
        {
            error(ERR_UNSUPPORTED_ACTION, src.path());
            return;
        }

        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath( srcItems );

        LIBMTP_mtpdevice_t *device = pair.second;
        LIBMTP_file_t *source = (LIBMTP_file_t*)pair.first;
        if (source->filetype == LIBMTP_FILETYPE_FOLDER)
        {
            error(ERR_IS_DIRECTORY, src.directory());
            return;
        }

        kDebug(KIO_MTP) << "Getting file" << source->filename << dest.fileName() << source->filesize;

        totalSize(source->filesize);

        int ret = LIBMTP_Get_File_To_File(device, source->item_id, dest.path().toStdString().c_str(), (LIBMTP_progressfunc_t) &dataProgress, this);
        if (ret != 0)
        {
            error(KIO::ERR_COULD_NOT_WRITE, dest.fileName());
            LIBMTP_Dump_Errorstack(device);
            LIBMTP_Clear_Errorstack(device);
            return;
        }

        kDebug(KIO_MTP) << "Sent file";

    }
    finished();
}

void MTPSlave::mkdir(const KUrl& url, int)
{
    kDebug(KIO_MTP) << "[ENTER]" << url.path();

    QStringList pathItems = url.path().split('/', QString::SkipEmptyParts);

    if ( pathItems.size() > 2 && !getPath( pathItems ).first )
    {
        char *dirName = strdup(pathItems.takeLast().toStdString().c_str());

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
    else
    {
        error( ERR_DIR_ALREADY_EXIST, url.path() );
        return;
    }
    finished();
}

void MTPSlave::del(const KUrl& url, bool)
{
    QStringList pathItems = url.path().split('/', QString::SkipEmptyParts);

    if (pathItems.size() < 2)
    {
        error(ERR_CANNOT_DELETE, url.path());
        return;
    }

    QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath( pathItems );

    LIBMTP_file_t *file = (LIBMTP_file_t*)pair.first;

    // TODO Maybe need to check for children? Or does KIO delete recursively?

    int ret = LIBMTP_Delete_Object(pair.second, file->item_id);

    LIBMTP_destroy_file_t(file);
    LIBMTP_Release_Device(pair.second);

    if (ret != 0)
    {
        error(ERR_CANNOT_DELETE, url.path());
        return;
    }
    finished();
}

void MTPSlave::rename(const KUrl& src, const KUrl& dest, JobFlags)
{
    QStringList srcItems = src.path().split('/', QString::SkipEmptyParts);

    if (srcItems.size() < 2)
    {
        error(ERR_CANNOT_RENAME, src.path());
        return;
    }

    QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath( srcItems );

    LIBMTP_file_t *file = (LIBMTP_file_t*)pair.first;

    int ret = LIBMTP_Set_File_Name(pair.second, file, dest.fileName().toStdString().c_str());

    LIBMTP_destroy_file_t(file);
    LIBMTP_Release_Device(pair.second);

    if (ret != 0)
    {
        error(ERR_CANNOT_RENAME, src.path());
        return;
    }
    finished();
}


#include "kio_mtp.moc"
