/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  Philipp Schmidt <philschmidt@gmx.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "filecache.h"

#include <KDebug>

#include <QTimer>

FileCache::FileCache ( QObject* parent ) : QThread ( parent )
{
    connect ( this, SIGNAL ( s_insertItem ( QString,QPair<QDateTime,uint32_t> ) ),
              this, SLOT ( insertItem ( QString,QPair<QDateTime,uint32_t> ) ) );
    connect ( this, SIGNAL ( s_removeItem ( QString ) ),
              this, SLOT ( removeItem ( QString ) ) );
}

uint32_t FileCache::queryPath ( const QString& path, int timeToLive )
{
    kDebug(KIO_MTP) << "Querying" << path;

    QPair< QDateTime, uint32_t > item = cache.value ( path );

    if ( item.second != 0 )
    {
        QDateTime dateTime = QDateTime::currentDateTime();

        if ( item.first > dateTime )
        {
            kDebug(KIO_MTP) << "Found item with ttl:" << item.first << "- now:" << dateTime;

            dateTime = dateTime.addSecs ( timeToLive );

            item.first = qMax<QDateTime> ( item.first, dateTime );

            emit ( s_insertItem ( path, item ) );

            return item.second;
        }
        else
        {
            kDebug(KIO_MTP) << "Item too old, removed";

            cache.remove( path );
            return 0;
        }
    }

    return 0;
}

void FileCache::insertItem ( const QString& path, QPair< QDateTime, uint32_t > item )
{
    cache.insert ( path, item );
}

void FileCache::removeItem ( const QString& path )
{
    cache.remove( path );
}

void FileCache::addPath ( const QString& path, uint32_t id, int timeToLive )
{
    QDateTime dateTime = QDateTime::currentDateTime();
    dateTime = dateTime.addSecs ( timeToLive );

    QPair< QDateTime, uint32_t > item ( dateTime, id );

    emit ( s_insertItem ( path, item ) );
}

void FileCache::removePath ( const QString& path )
{
    emit ( s_removeItem ( path ) );
}

#include "filecache.moc"
