#include "Util.h"

#include <QDateTime>
#include <QByteArray>

namespace iso::util {

QString cleanIsoString(const QByteArray &raw)
{
    QByteArray b = raw;
    while (!b.isEmpty()) {
        const char c = b.at(b.size() - 1);
        if (c == '\0' || c == ' ') b.chop(1);
        else break;
    }
    int first = 0;
    while (first < b.size() && (b.at(first) == '\0' || b.at(first) == ' ')) ++first;
    b = b.mid(first);
    QString s;
    s.reserve(b.size());
    for (unsigned char c : b) {
        if (c >= 0x20 && c <= 0x7e) s += QLatin1Char(char(c));
        else if (c == 0) break;
        else s += QLatin1Char(' ');
    }
    return s.trimmed();
}

bool wildcard(const QString &pattern, const QString &text)
{
    const QString p = pattern.toUpper();
    const QString t = text.toUpper();
    int pi = 0, ti = 0, star = -1, mark = 0;
    while (ti < t.size()) {
        if (pi < p.size() && (p.at(pi) == QLatin1Char('?') || p.at(pi) == t.at(ti))) {
            ++pi; ++ti;
        } else if (pi < p.size() && p.at(pi) == QLatin1Char('*')) {
            star = pi++; mark = ti;
        } else if (star >= 0) {
            pi = star + 1; ti = ++mark;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p.at(pi) == QLatin1Char('*')) ++pi;
    return pi == p.size();
}

QString stamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QString nowCacheKey()
{
    return QString::number(QDateTime::currentSecsSinceEpoch());
}

} // namespace iso::util
