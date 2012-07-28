
#ifndef METATYPE_H
#define METATYPE_H

#include <QMetaType>
#include <QMap>
#include <QString>

typedef QMap<QString,QString> StringMap;
Q_DECLARE_METATYPE(StringMap)

#endif //METATYPE_H
