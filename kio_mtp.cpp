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

#include <KComponentData>
#include <KTemporaryFile>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>
#include <QTimer>

#include <sys/stat.h>
#include <sys/types.h>

#include <solid/device.h>
#include <solid/genericinterface.h>

#include "kio_mtp_helpers.cpp"

//////////////////////////////////////////////////////////////////////////////
///////////////////////////// Slave Implementation ///////////////////////////
//////////////////////////////////////////////////////////////////////////////

extern "C"
int KDE_EXPORT kdemain ( int argc, char **argv )
{
    KComponentData instance ( "kio_mtp" );

    KGlobal::locale();

    QCoreApplication app( argc, argv );

    if ( argc != 4 )
    {
        fprintf ( stderr, "Usage: kio_mtp protocol domain-socket1 domain-socket2\n" );
        exit ( -1 );
    }

    MTPSlave slave ( argv[2], argv[3] );
    slave.dispatchLoop();
    return 0;
}

MTPSlave::MTPSlave ( const QByteArray& pool, const QByteArray& app )
    : SlaveBase ( "mtp", pool, app )
{
    LIBMTP_Init();

    fileCache = new FileCache ( this );
    fileCache->start();

    kDebug ( KIO_MTP ) << "Slave started";
}

MTPSlave::~MTPSlave()
{
    fileCache->exit();
}

/**
 * @brief Get's the correct object from the device.
 * !Important! Release Device after using the returned object
 * @param pathItems A QStringList containing the items of the filepath
 * @return QPair with the object and its device. pair.fist is a nullpointer if the object doesn't exist or for root or, depending on the pathItems size device (1), storage (2) or file (>=3)
 */
QPair<void*, LIBMTP_mtpdevice_t*> MTPSlave::getPath ( const QString& path )
{
    QStringList pathItems = path.split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

    kDebug ( KIO_MTP ) << path << pathItems.size();

    QPair<void*, LIBMTP_mtpdevice_t*> ret;

    // Don' handle the root directory
    if ( pathItems.size() <= 0 )
    {
        return ret;
    }

    QMap<QString, LIBMTP_raw_device_t*> devices = getRawDevices();

    if ( devices.contains ( pathItems.at ( 0 ) ) )
    {
        LIBMTP_mtpdevice_t *device = LIBMTP_Open_Raw_Device_Uncached ( devices.value ( pathItems.at ( 0 ) ) );

        // return specific device
        if ( pathItems.size() == 1 )
        {
            ret.first = device;
            ret.second = device;
        }

        if ( pathItems.size() > 2 )
        {
            // Query Cache after we have the device
            uint32_t c_fileID = fileCache->queryPath ( path );
            if ( c_fileID != 0 )
            {
                kDebug() << "Match found in cache, checking device";

                LIBMTP_file_t* file = LIBMTP_Get_Filemetadata ( device, c_fileID );
                if ( file )
                {
                    kDebug ( KIO_MTP ) << "Found file in cache";
                    ret.first = file;
                    ret.second = device;

                    return ret;
                }
            }
            // Query cache for parent
            else if ( pathItems.size() > 3 )
            {
                QString parentPath = convertToPath ( pathItems, pathItems.size() - 1 );
                uint32_t c_parentID = fileCache->queryPath ( parentPath );

                kDebug() << "Match for parent found in cache, checking device";

                LIBMTP_file_t* parent = LIBMTP_Get_Filemetadata ( device, c_parentID );
                if ( parent )
                {
                    kDebug ( KIO_MTP ) << "Found parent in cache";
                    fileCache->addPath( parentPath, c_parentID );

                    QMap<QString, LIBMTP_file_t*> files = getFiles ( device, parent->storage_id, c_parentID );

                    if ( files.contains ( pathItems.last() ) )
                    {
                        LIBMTP_file_t* file = files.value( pathItems.last() );

                        ret.first = file;
                        ret.second = device;

                        fileCache->addPath( path, file->item_id );
                    }

                    return ret;
                }
            }
        }

        QMap<QString, LIBMTP_devicestorage_t*> storages = getDevicestorages ( device );

        if ( pathItems.size() > 1 && storages.contains ( pathItems.at ( 1 ) ) )
        {
            kDebug(KIO_MTP) << "storages!!!!!!!!!!!!!!!!!!!";

            LIBMTP_devicestorage_t *storage = storages.value ( pathItems.at ( 1 ) );

            if ( pathItems.size() == 2 )
            {
                ret.first = storage;
                ret.second = device;

                return ret;
            }

            int currentLevel = 2, currentParent = 0xFFFFFFFF;

            QMap<QString, LIBMTP_file_t*> files;

            // traverse further while depth not reached
            while ( currentLevel < pathItems.size() )
            {
                files = getFiles ( device, storage->id, currentParent );

                if ( files.contains ( pathItems.at ( currentLevel ) ) )
                {
                    currentParent = files.value ( pathItems.at ( currentLevel ) )->item_id;
                }
                else
                {
                    return ret;
                }
                currentLevel++;
            }

            ret.first = LIBMTP_Get_Filemetadata ( device, currentParent );
            ret.second = device;

            fileCache->addPath ( path, currentParent );
        }
    }

    return ret;
}

int MTPSlave::checkUrl ( const KUrl& url, bool redirect )
{
    kDebug ( KIO_MTP ) << url;

    if ( url.path().startsWith ( QLatin1String ( "udi=" ) ) && redirect )
    {
        QString udi = url.path( KUrl::RemoveTrailingSlash ).remove ( 0, 4 );

        kDebug ( KIO_MTP ) << "udi = " << udi;

        Solid::Device device ( udi );
        if ( !device.isValid() )
        {
            return 2;
        }
        Solid::GenericInterface *iface = device.as<Solid::GenericInterface>();
        QMap<QString, QVariant> properties = iface->allProperties();

        int busnum = properties.value ( QLatin1String ( "BUSNUM" ) ).toInt();
        int devnum = properties.value ( QLatin1String ( "DEVNUM" ) ).toInt();

        kDebug ( KIO_MTP ) << "UDI reports BUS/DEV:" << busnum << "/" << devnum;

        QMap<QString, LIBMTP_raw_device_t*> devices = getRawDevices();

        foreach ( const QString &deviceName, devices.keys() )
        {
            LIBMTP_raw_device_t* rawDevice = devices.value ( deviceName );
            int currentBusNum = rawDevice->bus_location;
            int currentDevNum = rawDevice->devnum;

            kDebug ( KIO_MTP ) << "LIBMTP has BUS/DEV:"<< currentBusNum << "/" << currentDevNum;

            if ( currentBusNum == busnum && currentDevNum == devnum )
            {
                KUrl newUrl;
                newUrl.setProtocol ( QLatin1String ( "mtp" ) );
                newUrl.setPath ( QLatin1Char ( '/' ) + deviceName );
                redirection ( newUrl );

                return 1;
            }
        }
    }
    else if ( url.path().startsWith(QLatin1Char ( '/' ) ) )
    {
        return 0;
    }
    return -1;
}

void MTPSlave::listDir ( const KUrl& url )
{
    kDebug ( KIO_MTP ) << url.path();

    int check = checkUrl( url );
    switch ( check )
    {
        case 0:
            break;
        case 1:
            finished();
            return;
        case 2:
            error( ERR_DOES_NOT_EXIST, url.path() );
            return;
        default:
            error( ERR_MALFORMED_URL, url.path() );
            return;
    }

    QStringList pathItems = url.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

    UDSEntry entry;

    QMap<QString, LIBMTP_raw_device_t*> devices = getRawDevices();

    // list devices
    if ( pathItems.size() == 0 )
    {
        kDebug ( KIO_MTP ) << "Root directory, listing devices";
        totalSize ( devices.size() );

        foreach ( const QString &deviceName, devices.keys() )
        {
            LIBMTP_mtpdevice_t *device = LIBMTP_Open_Raw_Device_Uncached ( devices.value ( deviceName ) );

            getEntry ( entry, device );

            LIBMTP_Release_Device ( device );

            listEntry ( entry, false );
            entry.clear();
        }

        listEntry ( entry, true );

        kDebug ( KIO_MTP ) << "[SUCCESS] :: Devices";
        finished();
    }
    // traverse into device
    else if ( devices.contains ( pathItems.at ( 0 ) ) )
    {
        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( url.path() );
        UDSEntry entry;

        if ( pair.first )
        {
            LIBMTP_mtpdevice_t *device = pair.second;

            // Device, list storages
            if ( pathItems.size() == 1 )
            {
                QMap<QString, LIBMTP_devicestorage_t*> storages = getDevicestorages ( device );

                kDebug ( KIO_MTP ) << "Listing storages for device " << pathItems.at ( 0 );
                totalSize ( storages.size() );

                if ( storages.size() > 0 )
                {
                    foreach ( const QString &storageName, storages.keys() )
                    {
                        getEntry ( entry, storages.value ( storageName ) );

                        listEntry ( entry, false );
                        entry.clear();
                    }

                    listEntry ( entry, true );
                    kDebug ( KIO_MTP ) << "[SUCCESS] :: Storages";
                }
                else
                {
                    warning( tr("No Storages found. Maybe you need to unlock your device?") );
                }
            }
            // Storage, list files and folders of storage root
            else
            {
                QMap<QString, LIBMTP_file_t*> files;

                if ( pathItems.size() == 2 )
                {
                    kDebug(KIO_MTP) << "Getting storage root listing";

                    LIBMTP_devicestorage_t *storage = (LIBMTP_devicestorage_t*)pair.first;

                    kDebug(KIO_MTP) << "We have a storage:" << (storage == NULL);

                    files = getFiles( device, storage->id );
                }
                else
                {
                    LIBMTP_file_t *parent = (LIBMTP_file_t*)pair.first;

                    files = getFiles( device, parent->storage_id, parent->item_id );
                }

                for ( QMap<QString, LIBMTP_file_t*>::iterator it = files.begin(); it != files.end(); it++ )
                {
                    LIBMTP_file_t *file = it.value();

                    QString filePath = url.path( KUrl::AddTrailingSlash ).append( it.key() );
                    fileCache->addPath( filePath, file->item_id );

                    getEntry ( entry, file );

                    listEntry ( entry, false );
                    entry.clear();
                }

                listEntry ( entry, true );

                kDebug ( KIO_MTP ) << "[SUCCESS] Files";
            }

            LIBMTP_Release_Device ( pair.second );

            finished();
        }
        else
        {
            error ( ERR_CANNOT_ENTER_DIRECTORY, url.path() );
            kDebug ( KIO_MTP ) << "[ERROR]";
        }
    }
    else
    {
        error ( ERR_CANNOT_ENTER_DIRECTORY, url.path() );
        kDebug ( KIO_MTP ) << "[ERROR]";
    }
}

void MTPSlave::stat ( const KUrl& url )
{
    kDebug ( KIO_MTP ) << url.path();

    int check = checkUrl( url );
    switch ( check )
    {
        case 0:
            break;
        case 1:
            finished();
            return;
        case 2:
            error( ERR_DOES_NOT_EXIST, url.path() );
            return;
        default:
            error( ERR_MALFORMED_URL, url.path() );
            return;
    }

    QStringList pathItems = url.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

    QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( url.path() );
    UDSEntry entry;

    if ( pair.first )
    {
        // Root
        if ( pathItems.size() < 1 )
        {
            entry.insert ( UDSEntry::UDS_NAME, QLatin1String ( "mtp:///" ) );
            entry.insert ( UDSEntry::UDS_FILE_TYPE, S_IFDIR );
            entry.insert ( UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH );
            entry.insert ( UDSEntry::UDS_MIME_TYPE, QLatin1String ( "inode/directory" ) );
        }
        // Device
        else if ( pathItems.size() < 2 )
        {
            getEntry ( entry, pair.second );
        }
        // Storage
        else if ( pathItems.size() < 3 )
        {
            getEntry ( entry, ( LIBMTP_devicestorage_t* ) pair.first );
        }
        // Folder/File
        else
        {
            getEntry ( entry, ( LIBMTP_file_t* ) pair.first );
        }

        LIBMTP_Release_Device ( pair.second );
    }
    statEntry ( entry );
    finished();
}

void MTPSlave::mimetype ( const KUrl& url )
{
    int check = checkUrl( url );
    switch ( check )
    {
        case 0:
            break;
        case 1:
            finished();
            return;
        case 2:
            error( ERR_DOES_NOT_EXIST, url.path() );
            return;
        default:
            error( ERR_MALFORMED_URL, url.path() );
            return;
    }

    kDebug ( KIO_MTP ) << url.path();

    QStringList pathItems = url.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

    QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( url.path() );

    if ( pair.first )
    {
        if ( pathItems.size() > 2 )
            mimetype ( getMimetype ( ( ( LIBMTP_file_t* ) pair.first )->filetype ) );
        else
            mimetype ( QString::fromLatin1 ( "inode/directory" ) );
    }
    else
    {
        error ( ERR_DOES_NOT_EXIST, url.path() );
        return;
    }
}

void MTPSlave::put ( const KUrl& url, int, JobFlags flags )
{
    int check = checkUrl( url );
    switch ( check )
    {
        case 0:
            break;
        default:
            error( ERR_MALFORMED_URL, url.path() );
            return;
    }

    kDebug ( KIO_MTP ) << url.path();

    QStringList destItems = url.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

    // Can't copy to root or device, needs storage
    if ( destItems.size() < 2 )
    {
        error ( ERR_UNSUPPORTED_ACTION, url.path() );
        return;
    }

    if ( !(flags & KIO::Overwrite) && getPath( url.path() ).first )
    {
        error( ERR_FILE_ALREADY_EXIST, url.path() );
        return;
    }

    destItems.takeLast();

    QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( url.directory() );

    if ( !pair.first )
    {
        error ( ERR_DOES_NOT_EXIST, url.path() );
        return;
    }

    LIBMTP_mtpdevice_t *device = pair.second;
    LIBMTP_file_t *parent = ( LIBMTP_file_t* ) pair.first;
    if ( parent->filetype != LIBMTP_FILETYPE_FOLDER )
    {
        error ( ERR_IS_FILE, url.directory() );
        return;
    }

    // We did get a total size from the application
    if ( hasMetaData ( QLatin1String ( "sourceSize" ) ) )
    {
        kDebug ( KIO_MTP ) << "direct put";

        LIBMTP_file_t *file = LIBMTP_new_file_t();
        file->parent_id = parent->item_id;
        file->filename = strdup ( url.fileName().toUtf8().data() );
        file->filetype = getFiletype ( url.fileName() );
        file->filesize = metaData ( QLatin1String ( "sourceSize" ) ).toULongLong();
        file->modificationdate = QDateTime::currentDateTime().toTime_t();
        file->storage_id = parent->storage_id;

        kDebug ( KIO_MTP ) << "Sending file" << file->filename;

        int ret = LIBMTP_Send_File_From_Handler ( device, &dataGet, this, file, &dataProgress, this );
        if ( ret != 0 )
        {
            error ( KIO::ERR_COULD_NOT_WRITE, url.fileName() );
            LIBMTP_Dump_Errorstack ( device );
            LIBMTP_Clear_Errorstack ( device );
            return;
        }
    }
    // We need to get the entire file first, then we can upload
    else
    {
        kDebug ( KIO_MTP ) << "use temp file";

        KTemporaryFile temp;
        QByteArray buffer;
        int len = 0;

        do
        {
            dataReq();
            len = readData ( buffer );
            temp.write ( buffer );
        }
        while ( len > 0 );

        QFileInfo info ( temp );

        LIBMTP_file_t *file = LIBMTP_new_file_t();
        file->parent_id = parent->item_id;
        file->filename = strdup ( url.fileName().toUtf8().data() );
        file->filetype = getFiletype ( url.fileName() );
        file->filesize = info.size();
        file->modificationdate = QDateTime::currentDateTime().toTime_t();
        file->storage_id = parent->storage_id;


        int ret = LIBMTP_Send_File_From_File_Descriptor ( device, temp.handle(), file, NULL, NULL );
        if ( ret != 0 )
        {
            error ( KIO::ERR_COULD_NOT_WRITE, url.fileName() );
            return;
        }
        finished();
    }
}

void MTPSlave::get ( const KUrl& url )
{
    int check = checkUrl( url );
    switch ( check )
    {
        case 0:
            break;
        default:
            error( ERR_MALFORMED_URL, url.path() );
            return;
    }

    kDebug ( KIO_MTP ) << url.path();

    QStringList pathItems = url.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

    // File
    if ( pathItems.size() > 2 )
    {
        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( url.path() );

        if ( pair.first )
        {
            LIBMTP_file_t *file = ( LIBMTP_file_t* ) pair.second;

            mimeType ( getMimetype ( file->filetype ) );
            totalSize ( file->filesize );

            LIBMTP_mtpdevice_t *device = pair.second;

            int ret = LIBMTP_Get_File_To_Handler ( device, file->item_id, &dataPut, this, &dataProgress, this );
            if ( ret != 0 )
            {
                error ( ERR_COULD_NOT_READ, url.path() );
                return;
            }
            data ( QByteArray() );
            finished();
        }
        else
            error ( ERR_DOES_NOT_EXIST, url.path() );

        LIBMTP_Release_Device ( pair.second );
    }
    else
        error ( ERR_UNSUPPORTED_ACTION, url.path() );
}

void MTPSlave::copy ( const KUrl& src, const KUrl& dest, int, JobFlags flags )
{
    kDebug ( KIO_MTP ) << src.path() << dest.path();

    // mtp:/// to mtp:///
    if ( src.protocol() == QLatin1String ( "mtp" ) && dest.protocol() == QLatin1String ( "mtp" ) )
    {
        kDebug ( KIO_MTP ) << "Copy on device: Not supported";
        // MTP doesn't support moving files directly on the device, so we have to download and then upload...

        error ( ERR_UNSUPPORTED_ACTION, i18n( "Cannot copy/move files on the device itself" ) );
    }
    // file:/// tp mtp:///
    if ( src.protocol() == QLatin1String ( "file" ) && dest.protocol() == QLatin1String ( "mtp" ) )
    {
        int check = checkUrl( dest );
        switch ( check )
        {
            case 0:
                break;
            default:
                error( ERR_MALFORMED_URL, dest.path() );
                return;
        }

        QStringList destItems = dest.path().split ( QLatin1Char ( '/') , QString::SkipEmptyParts );

        // Can't copy to root or device, needs storage
        if ( destItems.size() < 2 )
        {
            error ( ERR_UNSUPPORTED_ACTION, dest.path() );
            return;
        }

        kDebug ( KIO_MTP ) << "Copy file " << src.fileName() << "from filesystem to device" << src.directory ( KUrl::AppendTrailingSlash ) << dest.directory ( KUrl::AppendTrailingSlash );

        if ( !(flags & KIO::Overwrite) && getPath( dest.path() ).first )
        {
            error( ERR_FILE_ALREADY_EXIST, dest.path() );
            return;
        }

        destItems.takeLast();

        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( dest.directory() );

        if ( !pair.first )
        {
            error ( ERR_DOES_NOT_EXIST, dest.directory ( KUrl::AppendTrailingSlash ) );
            return;
        }

        LIBMTP_mtpdevice_t *device = pair.second;
        LIBMTP_file_t *parent = ( LIBMTP_file_t* ) pair.first;

        if ( parent->filetype != LIBMTP_FILETYPE_FOLDER )
        {
            error ( ERR_IS_FILE, dest.directory() );
            return;
        }

        QFileInfo source ( src.path() );

        LIBMTP_file_t *file = LIBMTP_new_file_t();
        file->parent_id = parent->item_id;
        file->filename = strdup ( dest.fileName().toUtf8().data() );
        file->filetype = getFiletype ( dest.fileName() );
        file->filesize = source.size();
        file->modificationdate = source.lastModified().toTime_t();
        file->storage_id = parent->storage_id;

        kDebug ( KIO_MTP ) << "Sending file" << file->filename << "with size" << file->filesize;

        totalSize ( source.size() );

        int ret = LIBMTP_Send_File_From_File ( device, src.path().toUtf8().data(), file, ( LIBMTP_progressfunc_t ) &dataProgress, this );
        if ( ret != 0 )
        {
            error ( KIO::ERR_COULD_NOT_WRITE, dest.fileName() );
            LIBMTP_Dump_Errorstack ( device );
            LIBMTP_Clear_Errorstack ( device );
            return;
        }

        kDebug ( KIO_MTP ) << "Sent file";
    }
    // mtp:/// to file:///
    if ( src.protocol() == QLatin1String ( "mtp" ) && dest.protocol() == QLatin1String ( "file" ) )
    {
        int check = checkUrl( src );
        switch ( check )
        {
            case 0:
                break;
            default:
                error( ERR_MALFORMED_URL, src.path() );
                return;
        }

        kDebug ( KIO_MTP ) << "Copy file " << src.fileName() << "from device to filesystem" << dest.directory ( KUrl::AppendTrailingSlash ) << dest.directory ( KUrl::AppendTrailingSlash );

        QFileInfo destination ( dest.path() );

        if ( !(flags & KIO::Overwrite) && destination.exists() )
        {
            error( ERR_FILE_ALREADY_EXIST, dest.path() );
            return;
        }

        QStringList srcItems = src.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

        // Can't copy to root or device, needs storage
        if ( srcItems.size() < 2 )
        {
            error ( ERR_UNSUPPORTED_ACTION, src.path() );
            return;
        }

        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( src.path() );

        LIBMTP_mtpdevice_t *device = pair.second;
        LIBMTP_file_t *source = ( LIBMTP_file_t* ) pair.first;
        if ( source->filetype == LIBMTP_FILETYPE_FOLDER )
        {
            error ( ERR_IS_DIRECTORY, src.directory() );
            return;
        }

        kDebug ( KIO_MTP ) << "Getting file" << source->filename << dest.fileName() << source->filesize;

        totalSize ( source->filesize );

        int ret = LIBMTP_Get_File_To_File ( device, source->item_id, dest.path().toUtf8().data(), ( LIBMTP_progressfunc_t ) &dataProgress, this );
        if ( ret != 0 )
        {
            error ( KIO::ERR_COULD_NOT_WRITE, dest.fileName() );
            LIBMTP_Dump_Errorstack ( device );
            LIBMTP_Clear_Errorstack ( device );
            return;
        }

        kDebug ( KIO_MTP ) << "Sent file";

    }
    finished();
}

void MTPSlave::mkdir ( const KUrl& url, int )
{
    int check = checkUrl( url );
    switch ( check )
    {
        case 0:
            break;
        default:
            error( ERR_MALFORMED_URL, url.path() );
            return;
    }

    kDebug ( KIO_MTP ) << url.path();

    QStringList pathItems = url.path().split ( QLatin1Char ( '/' ) , QString::SkipEmptyParts );

    if ( pathItems.size() > 2 && !getPath ( url.path() ).first )
    {
        char *dirName = strdup ( pathItems.takeLast().toUtf8().data() );

        LIBMTP_mtpdevice_t *device;
        LIBMTP_file_t *file;

        QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( url.directory() );

        if ( pair.first )
        {
            file = ( LIBMTP_file_t* ) pair.first;
            device = pair.second;

            if ( file && file->filetype == LIBMTP_FILETYPE_FOLDER )
            {
                kDebug ( KIO_MTP ) << "Found parent" << file->item_id << file->filename;
                kDebug ( KIO_MTP ) << "Attempting to create folder" << dirName;

                int ret = LIBMTP_Create_Folder ( device, dirName, file->item_id, file->storage_id );
                if ( ret != 0 )
                {
                    finished();
                    return;
                }
                fileCache->addPath( url.path(), ret );

                LIBMTP_Dump_Errorstack ( device );
                LIBMTP_Clear_Errorstack ( device );
            }
        }
    }
    else
    {
        error ( ERR_DIR_ALREADY_EXIST, url.path() );
        return;
    }

    error ( ERR_COULD_NOT_MKDIR, url.path() );
}

void MTPSlave::del ( const KUrl& url, bool )
{
    int check = checkUrl( url );
    switch ( check )
    {
        case 0:
            break;
        default:
            error( ERR_MALFORMED_URL, url.path() );
            return;
    }

    kDebug ( KIO_MTP ) << url.path();

    QStringList pathItems = url.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );

    if ( pathItems.size() < 2 )
    {
        error ( ERR_CANNOT_DELETE, url.path() );
        return;
    }

    QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( url.path() );

    LIBMTP_file_t *file = ( LIBMTP_file_t* ) pair.first;

    int ret = LIBMTP_Delete_Object ( pair.second, file->item_id );

    LIBMTP_destroy_file_t ( file );
    LIBMTP_Release_Device ( pair.second );

    if ( ret != 0 )
    {
        error ( ERR_CANNOT_DELETE, url.path() );
        return;
    }

    fileCache->removePath( url.path() );
    finished();
}

void MTPSlave::rename ( const KUrl& src, const KUrl& dest, JobFlags flags )
{
    int check = checkUrl( src );
    switch ( check )
    {
        case 0:
            break;
        default:
            error( ERR_MALFORMED_URL, src.path() );
            return;
    }
    check = checkUrl( dest );
    switch ( check )
    {
        case 0:
            break;
        default:
            error( ERR_MALFORMED_URL, dest.path() );
            return;
    }


    kDebug ( KIO_MTP ) << src.path();

    QStringList srcItems = src.path().split ( QLatin1Char ( '/' ), QString::SkipEmptyParts );
    QPair<void*, LIBMTP_mtpdevice_t*> pair = getPath ( src.path() );

    if ( pair.first )
    {
        // Rename Device
        if ( srcItems.size() == 1 )
        {
            LIBMTP_Set_Friendlyname ( pair.second, dest.fileName().toUtf8().data() );
        }
        // Rename Storage
        else if ( srcItems.size() == 2 )
        {
            error ( ERR_CANNOT_RENAME, src.path() );
            return;
        }
        else
        {
            LIBMTP_file_t *destination = ( LIBMTP_file_t* ) getPath ( dest.path() ).first;
            LIBMTP_file_t *source = ( LIBMTP_file_t* ) pair.first;

            if ( !(flags & KIO::Overwrite) && destination )
            {
                if ( destination->filetype == LIBMTP_FILETYPE_FOLDER )
                {
                    error( ERR_DIR_ALREADY_EXIST, dest.path() );
                }
                else
                {
                    error( ERR_FILE_ALREADY_EXIST, dest.path() );
                }
                return;
            }

            int ret = LIBMTP_Set_File_Name ( pair.second, source, dest.fileName().toUtf8().data() );

            if ( ret != 0 )
            {
                error ( ERR_CANNOT_RENAME, src.path() );
                return;
            }
            else
            {
                fileCache->addPath( dest.path(), source->item_id );
                fileCache->removePath( src.path() );
            }

            LIBMTP_destroy_file_t ( source );
            LIBMTP_Release_Device ( pair.second );
        }

        finished();
    }
    else
    {
        error ( ERR_DOES_NOT_EXIST, src.path() );
    }
}


#include "kio_mtp.moc"

