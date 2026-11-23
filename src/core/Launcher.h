#pragma once

#include "Types.h"
#include "Config.h"

#include <QString>
#include <QStringList>

namespace iso {

class Launcher
{
public:
    explicit Launcher(const Config &cfg);

    struct Result { bool ok = false; QString message; qint64 pid = 0; };

    // Resolve class by id and launch it detached. Never uses xdg-open/gio open on the ISO itself.
    Result launchClass(const QString &classId, const QString &isoPath, const QString &entry = QString()) const;
    Result launchCommand(const QString &exec, const QStringList &argsTemplate, const QString &isoPath, const QString &entry = QString()) const;

    static QStringList substitute(const QStringList &argsTemplate, const QString &isoPath, const QString &entry);

private:
    const Config &m_cfg;
};

} // namespace iso
