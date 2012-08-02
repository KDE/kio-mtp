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
    int meta = qRegisterMetaType<StringMap>();
    int reg = qDBusRegisterMetaType<StringMap>();
    
    kDebug(KIO_MTP) << pool << app << meta << "" << reg;
    
    currentTransaction == 0;
    mtpdInterface = new org::mtpd("org.mtpd", "/filesystem", QDBusConnection::sessionBus(), this);
    
    infoMessage("Currently indexind...");
}

MTPSlave::~MTPSlave()
{
}

void MTPSlave::waitLoop()
{
    QEventLoop loop;
    connect( this, SIGNAL( breakLoop() ),
             &loop, SLOT( quit() ) );
    loop.exec();
}

void MTPSlave::getEntry(const QString& path, UDSEntry &entry)
{
    QDBusPendingReply<QString, qulonglong, QString, qulonglong> reply = mtpdInterface->info(path);
    reply.waitForFinished();
    
    if (reply.isValid())
    {
        entry.insert( UDSEntry::UDS_NAME, reply.argumentAt<0>() );
        
        QString originalmimetype = reply.argumentAt<2>();
        QString mimetype = originalmimetype;
        
        if (originalmimetype == "mtpd/device")
        {
            mimetype = "inode/directory";
            entry.insert(UDSEntry::UDS_ICON_NAME, QString("multimedia-player"));
        }
        if (originalmimetype == "mtpd/storage")
        {
            mimetype = "inode/directory";
            entry.insert(UDSEntry::UDS_ICON_NAME, QString("drive-removable-media"));
        }
        entry.insert(UDSEntry::UDS_MIME_TYPE, mimetype);
        
        if (mimetype == "inode/directory")
        {
            entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFDIR );
        }
        else
        {
            entry.insert( UDSEntry::UDS_FILE_TYPE, S_IFREG );
            entry.insert( UDSEntry::UDS_SIZE, reply.argumentAt<1>());
            
            qulonglong modificationdate = reply.argumentAt<3>();
            entry.insert( UDSEntry::UDS_ACCESS_TIME, modificationdate);
            entry.insert( UDSEntry::UDS_MODIFICATION_TIME, modificationdate);
            entry.insert( UDSEntry::UDS_CREATION_TIME, modificationdate);
        }
    }
    else
    {
        QDBusError error = reply.error();
        kError(KIO_MTP) << error.name();
        kError(KIO_MTP) << error.message();
    }
}

void MTPSlave::listDir( const KUrl& url )
{
    kDebug(KIO_MTP) << "listDir() [" << url << "]";
    
    const QString path = url.path();
    
    QDBusPendingReply<StringMap> reply = mtpdInterface->list(path);
    reply.waitForFinished();
    
    UDSEntry entry;
    
    if (reply.isValid())
    {
        StringMap map = reply.value();
        
        QList<QString> keys = map.keys();
        
        foreach (const QString &name, keys)
        {
            QString childPath = QString(path).append("/").append(name);
            getEntry(childPath, entry);
            listEntry(entry, false);
            entry.clear();
        }
    }
    else
    {
        QDBusError error = reply.error();
        kError(KIO_MTP) << error.name();
        kError(KIO_MTP) << error.message();
    }
    
    listEntry(entry, true);
    finished();
    
    kDebug(KIO_MTP) << "[EXIT]";
}

void MTPSlave::mimetype(const KUrl& url)
{
    
}

void MTPSlave::stat( const KUrl& url )
{
    kDebug(KIO_MTP) << "stat() [" << url << "]";
    
    UDSEntry entry;
    getEntry(url.path(), entry);
    statEntry(entry);
    
    finished();
}

void MTPSlave::copy(const KUrl& src, const KUrl& dest, int permissions, JobFlags flags)
{
    // On device
    if (src.protocol() == "mtp" && dest.protocol() == "mtp")
    {
        
    }
    // To device
    if (src.protocol() == "file" && dest.protocol() == "mtp")
    {
        kDebug(KIO_MTP) << "Copy file " << src.path() << " from filesystem to device: " << dest.path();
        
        QDBusPendingReply<int> reply = mtpdInterface->copyFileToDevice(src.path(), dest.path());
        reply.waitForFinished();
        
        if (reply.isValid())
        {
            // set transaction ID to the transmitted value and connect to signals so we can receive updates
            currentTransaction = reply.value();
            connect ( mtpdInterface, SIGNAL ( transactionProgress ( int, qulonglong, qulonglong ) ),
                      this, SLOT ( slotProgress ( int, qulonglong, qulonglong ) ) );
            connect ( mtpdInterface, SIGNAL ( transactionFinished ( int ) ),
                      this, SLOT ( slotFinished ( int ) ) );
            waitLoop();
        }
    }
    // From device
    if (src.protocol() == "mtp" && dest.protocol() == "file")
    {
        kDebug(KIO_MTP) << "Copy file " << src.path() << " from device to filesystem: " << dest.path();
        
        QDBusPendingReply<int> reply = mtpdInterface->copyFileFromDevice(src.path(), dest.path());
        reply.waitForFinished();
        
        if (reply.isValid())
        {
            // set transaction ID to the transmitted value and connect to signals so we can receive updates
            currentTransaction = reply.value();
            connect ( mtpdInterface, SIGNAL ( transactionProgress ( int, qulonglong, qulonglong ) ),
                      this, SLOT ( slotProgress ( int, qulonglong, qulonglong ) ) );
            connect ( mtpdInterface, SIGNAL ( transactionFinished ( int ) ),
                      this, SLOT ( slotFinished ( int ) ) );
            waitLoop();
        }
    }
}

void MTPSlave::slotProgress(int transactionID, qulonglong sentBytes, qulonglong totalBytes)
{
    kDebug(KIO_MTP) << "Received Update for transactionID=" << transactionID << "(Our ID=" << currentTransaction << ")";
    
    if (transactionID == currentTransaction)
    {
        processedSize(sentBytes);
    }
}

void MTPSlave::slotFinished(int transactionID)
{
    if (transactionID == currentTransaction)
    {
        // reset transaction id and disconnect signals so we don't reveive updates
        currentTransaction == 0;
        disconnect ( mtpdInterface, SIGNAL ( transactionProgress ( int, qulonglong, qulonglong ) ),
                  this, SLOT ( slotProgress ( int, qulonglong, qulonglong ) ) );
        disconnect ( mtpdInterface, SIGNAL ( transactionFinished ( int ) ),
                  this, SLOT ( slotFinished ( int ) ) );
        
        emit breakLoop();
        finished();
    }
}

#include "kio_mtp.moc"
