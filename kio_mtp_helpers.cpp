/*
 *  Helper implementations for KIO-MTP
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

#include <libmtp.h>

int dataProgress(uint64_t const sent, uint64_t const, void const *const priv)
{
    ((MTPSlave*)priv)->processedSize(sent);

    return 0;
}

/**
 * MTPDataPutFunc callback function, "puts" data from the device somewhere else
 */
uint16_t dataPut(void *params, void *priv, uint32_t sendlen, unsigned char *data, uint32_t *putlen)
{
    //     kDebug(KIO_MTP) << "transferring" << sendlen << "bytes to data()";
    ((MTPSlave*) priv)->data(QByteArray((char*) data, (int)sendlen));
    *putlen = sendlen;

    return LIBMTP_HANDLER_RETURN_OK;
}

/**
 * MTPDataGetFunc callback function, "gets" data and puts it on the device
 */
uint16_t dataGet(void *params, void *priv, uint32_t wantlen, unsigned char *data, uint32_t *gotlen)
{
    //     kDebug(KIO_MTP) << "transferring" << sendlen << "bytes to data()";
    ((MTPSlave*) priv)->dataReq();

    QByteArray buffer;
    *gotlen = ((MTPSlave*) priv)->readData(buffer);

    data = (unsigned char*) buffer.data();

    return LIBMTP_HANDLER_RETURN_OK;
}

QString getMimetype(LIBMTP_filetype_t filetype)
{
    switch (filetype)
    {
    case LIBMTP_FILETYPE_FOLDER:
        return QString("inode/directory");

    case LIBMTP_FILETYPE_WAV:
        return QString("audio/wav");
    case LIBMTP_FILETYPE_MP3:
        return QString("audio/x-mp3");
    case LIBMTP_FILETYPE_WMA:
        return QString("audio/x-ms-wma");
    case LIBMTP_FILETYPE_OGG:
        return QString("audio/x-vorbis+ogg");
    case LIBMTP_FILETYPE_AUDIBLE:
        return QString("");
    case LIBMTP_FILETYPE_MP4:
        return QString("audio/mp4");
    case LIBMTP_FILETYPE_UNDEF_AUDIO:
        return QString("");
    case LIBMTP_FILETYPE_WMV:
        return QString("video/x-ms-wmv");
    case LIBMTP_FILETYPE_AVI:
        return QString("video/x-msvideo");
    case LIBMTP_FILETYPE_MPEG:
        return QString("video/mpeg");
    case LIBMTP_FILETYPE_ASF:
        return QString("video/x-ms-asf");
    case LIBMTP_FILETYPE_QT:
        return QString("video/quicktime");
    case LIBMTP_FILETYPE_UNDEF_VIDEO:
        return QString("");
    case LIBMTP_FILETYPE_JPEG:
        return QString("image/jpeg");
    case LIBMTP_FILETYPE_JFIF:
        return QString("");
    case LIBMTP_FILETYPE_TIFF:
        return QString("image/tiff");
    case LIBMTP_FILETYPE_BMP:
        return QString("image/bmp");
    case LIBMTP_FILETYPE_GIF:
        return QString("image/gif");
    case LIBMTP_FILETYPE_PICT:
        return QString("image/x-pict");
    case LIBMTP_FILETYPE_PNG:
        return QString("image/png");
    case LIBMTP_FILETYPE_VCALENDAR1:
        return QString("text/x-vcalendar");
    case LIBMTP_FILETYPE_VCALENDAR2:
        return QString("text/x-vcalendar");
    case LIBMTP_FILETYPE_VCARD2:
        return QString("text/x-vcard");
    case LIBMTP_FILETYPE_VCARD3:
        return QString("text/x-vcard");
    case LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT:
        return QString("image/x-wmf");
    case LIBMTP_FILETYPE_WINEXEC:
        return QString("application/x-ms-dos-executable");
    case LIBMTP_FILETYPE_TEXT:
        return QString("text/plain");
    case LIBMTP_FILETYPE_HTML:
        return QString("text/html");
    case LIBMTP_FILETYPE_FIRMWARE:
        return QString("");
    case LIBMTP_FILETYPE_AAC:
        return QString("audio/aac");
    case LIBMTP_FILETYPE_MEDIACARD:
        return QString("");
    case LIBMTP_FILETYPE_FLAC:
        return QString("audio/flac");
    case LIBMTP_FILETYPE_MP2:
        return QString("video/mpeg");
    case LIBMTP_FILETYPE_M4A:
        return QString("audio/mp4");
    case LIBMTP_FILETYPE_DOC:
        return QString("application/msword");
    case LIBMTP_FILETYPE_XML:
        return QString("text/xml");
    case LIBMTP_FILETYPE_XLS:
        return QString("application/vnd.ms-excel");
    case LIBMTP_FILETYPE_PPT:
        return QString("application/vnd.ms-powerpoint");
    case LIBMTP_FILETYPE_MHT:
        return QString("");
    case LIBMTP_FILETYPE_JP2:
        return QString("image/jpeg2000");
    case LIBMTP_FILETYPE_JPX:
        return QString("application/x-jbuilder-project");
    case LIBMTP_FILETYPE_UNKNOWN:
        return QString("");

    default:
        return "";
    }
}

LIBMTP_filetype_t getFiletype(const QString &filename)
{
    LIBMTP_filetype_t filetype;

    QString ptype = filename.split(".").last();

    /* This need to be kept constantly updated as new file types arrive. */
    if ( ptype == QString("wav") ) {
        filetype = LIBMTP_FILETYPE_WAV;
    } else if ( ptype == QString("mp3") ) {
        filetype = LIBMTP_FILETYPE_MP3;
    } else if ( ptype == QString("wma") ) {
        filetype = LIBMTP_FILETYPE_WMA;
    } else if ( ptype == QString("ogg") ) {
        filetype = LIBMTP_FILETYPE_OGG;
    } else if ( ptype == QString("mp4") ) {
        filetype = LIBMTP_FILETYPE_MP4;
    } else if ( ptype == QString("wmv") ) {
        filetype = LIBMTP_FILETYPE_WMV;
    } else if ( ptype == QString("avi") ) {
        filetype = LIBMTP_FILETYPE_AVI;
    } else if ( ptype == QString("mpeg") ||
                ptype == QString("mpg") ) {
        filetype = LIBMTP_FILETYPE_MPEG;
    } else if ( ptype == QString("asf") ) {
        filetype = LIBMTP_FILETYPE_ASF;
    } else if ( ptype == QString("qt") ||
                ptype == QString("mov") ) {
        filetype = LIBMTP_FILETYPE_QT;
    } else if ( ptype == QString("wma") ) {
        filetype = LIBMTP_FILETYPE_WMA;
    } else if ( ptype == QString("jpg") ||
                ptype == QString("jpeg") ) {
        filetype = LIBMTP_FILETYPE_JPEG;
    } else if ( ptype == QString("jfif") ) {
        filetype = LIBMTP_FILETYPE_JFIF;
    } else if ( ptype == QString("tif") ||
                ptype == QString("tiff") ) {
        filetype = LIBMTP_FILETYPE_TIFF;
    } else if ( ptype == QString("bmp") ) {
        filetype = LIBMTP_FILETYPE_BMP;
    } else if ( ptype == QString("gif")) {
        filetype = LIBMTP_FILETYPE_GIF;
    } else if ( ptype == QString("pic") ||
                ptype == QString("pict")) {
        filetype = LIBMTP_FILETYPE_PICT;
    } else if ( ptype == QString("png") ) {
        filetype = LIBMTP_FILETYPE_PNG;
    } else if ( ptype == QString("wmf") ) {
        filetype = LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT;
    } else if ( ptype == QString("ics") ) {
        filetype = LIBMTP_FILETYPE_VCALENDAR2;
    } else if ( ptype == QString("exe") ||
                ptype == QString("com") ||
                ptype == QString("bat") ||
                ptype == QString("dll") ||
                ptype == QString("sys") ) {
        filetype = LIBMTP_FILETYPE_WINEXEC;
    } else if (ptype == QString("aac")) {
        filetype = LIBMTP_FILETYPE_AAC;
    } else if (ptype == QString("mp2")) {
        filetype = LIBMTP_FILETYPE_MP2;
    } else if (ptype == QString("flac")) {
        filetype = LIBMTP_FILETYPE_FLAC;
    } else if (ptype == QString("m4a")) {
        filetype = LIBMTP_FILETYPE_M4A;
    } else if (ptype == QString("doc")) {
        filetype = LIBMTP_FILETYPE_DOC;
    } else if (ptype == QString("xml")) {
        filetype = LIBMTP_FILETYPE_XML;
    } else if (ptype == QString("xls")) {
        filetype = LIBMTP_FILETYPE_XLS;
    } else if (ptype == QString("ppt")) {
        filetype = LIBMTP_FILETYPE_PPT;
    } else if (ptype == QString("mht")) {
        filetype = LIBMTP_FILETYPE_MHT;
    } else if (ptype == QString("jp2")) {
        filetype = LIBMTP_FILETYPE_JP2;
    } else if (ptype == QString("jpx")) {
        filetype = LIBMTP_FILETYPE_JPX;
    } else if (ptype == QString("bin")) {
        filetype = LIBMTP_FILETYPE_FIRMWARE;
    } else if (ptype == QString("vcf")) {
        filetype = LIBMTP_FILETYPE_VCARD3;
    } else {
        /* Tagging as unknown file type */
        filetype = LIBMTP_FILETYPE_UNKNOWN;
    }

    return filetype;
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

QMap<QString, LIBMTP_devicestorage_t*> getDevicestorages( LIBMTP_mtpdevice_t *&device )
{
    kDebug(KIO_MTP) << "[ENTER]" << (device == 0);

    QMap<QString, LIBMTP_devicestorage_t*> storages;
    if ( device )
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

//             kDebug(KIO_MTP) << "found storage" << storagename;

            storages.insert( storagename, storage );
        }
    }

    kDebug(KIO_MTP) << "[EXIT]";

    return storages;
}

QMap<QString, LIBMTP_file_t*> getFiles( LIBMTP_mtpdevice_t *&device, LIBMTP_devicestorage_t *&storage, uint32_t parent_id = 0xFFFFFFFF )
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
 * @return QPair with the object and its device. pair.fist is a nullpointer if the object doesn't exist or for root or, depending on the pathItems size device (1), storage (2) or file (>=3)
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

        if (pathItems.size() > 1 && storages.contains( pathItems.at(1) ) )
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

void getEntry( UDSEntry &entry, LIBMTP_mtpdevice_t* device )
{
    char *charName = LIBMTP_Get_Friendlyname( device );
    char *charModel = LIBMTP_Get_Modelname( device );

    // prefer friendly devicename over model
    QString deviceName;
    if ( !charName )
        deviceName = QString::fromUtf8(charModel);
    else
        deviceName = QString::fromUtf8(charName);

    entry.insert( UDSEntry::UDS_NAME, deviceName );
    entry.insert( UDSEntry::UDS_ICON_NAME, QString( "multimedia-player" ) );
    entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFDIR );
    entry.insert( UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH );
    entry.insert( UDSEntry::UDS_MIME_TYPE, "inode/directory" );
}

void getEntry( UDSEntry &entry, const LIBMTP_devicestorage_t* storage )
{
    char *charIdentifier = storage->VolumeIdentifier;
    char *charDescription = storage->StorageDescription;

    QString storageName;
    if ( !charIdentifier )
        storageName = QString::fromUtf8( charDescription );
    else
        storageName = QString::fromUtf8( charIdentifier );

    entry.insert( UDSEntry::UDS_NAME, storageName );
    entry.insert( UDSEntry::UDS_ICON_NAME, QString( "drive-removable-media" ) );
    entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFDIR );
    entry.insert( UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH );
    entry.insert( UDSEntry::UDS_MIME_TYPE, "inode/directory" );
}

void getEntry( UDSEntry &entry, const LIBMTP_file_t* file )
{
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
}
