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


#ifndef KIO_MTP_H
#define KIO_MTP_H

#include <kdebug.h>
#include <kio/global.h>
#include <kio/slavebase.h>
#include <kurl.h>
#include <klocale.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define MAX_XFER_BUF_SIZE           16348
#define KIO_MTP                     7000

using namespace KIO;


class MTPSlave : public QObject, public KIO::SlaveBase
{
    Q_OBJECT

public:
    /*
     * Overwritten KIO-functions, see "kio_mtp.cpp"
     */
    MTPSlave(const QByteArray& pool, const QByteArray& app);
    virtual ~MTPSlave();

    virtual void listDir( const KUrl& url );
//     virtual void stat( const KUrl& url );
//     virtual void mimetype(const KUrl& url);
//     virtual void get( const KUrl& url );
//     virtual void put(const KUrl& url, int permissions, JobFlags flags);
//     virtual void copy(const KUrl& src, const KUrl& dest, int permissions, JobFlags flags);
//     virtual void mkdir(const KUrl& url, int permissions);
//     virtual void del(const KUrl& url, bool isfile);
//     virtual void rename(const KUrl& src, const KUrl& dest, JobFlags flags);
};

#endif  //#endif KIO_MTP_H
