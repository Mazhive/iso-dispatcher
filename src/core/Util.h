#pragma once

#include <QString>
#include <QStringList>

namespace iso::util {

QString cleanIsoString(const QByteArray &raw);   // trim NUL/spaces, keep printable ASCII
bool wildcard(const QString &pattern, const QString &text); // * and ? supported, case-insensitive
QString stamp();                                 // ISO-8601 UTC
QString nowCacheKey();                           // for negative cache

} // namespace iso::util
