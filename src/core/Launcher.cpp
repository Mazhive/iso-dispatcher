#include "Launcher.h"

#include <QProcess>
#include <QFileInfo>

namespace iso {

Launcher::Launcher(const Config &cfg) : m_cfg(cfg) {}

QStringList Launcher::substitute(const QStringList &argsTemplate, const QString &isoPath, const QString &entry)
{
    QStringList out;
    for (const QString &a : argsTemplate) {
        QString s = a;
        s.replace(QStringLiteral("%f"), isoPath);
        s.replace(QStringLiteral("%e"), entry);
        out << s;
    }
    return out;
}

Launcher::Result Launcher::launchCommand(const QString &exec, const QStringList &argsTemplate, const QString &isoPath, const QString &entry) const
{
    Result r;
    if (exec.trimmed().isEmpty()) { r.message = QStringLiteral("empty exec"); return r; }
    // Safety: never recursively dispatch the same ISO through generic openers.
    const QString e = exec.trimmed().toLower();
    if (e == QStringLiteral("xdg-open") || e == QStringLiteral("gio") || e == QStringLiteral("kde-open")) {
        r.message = QStringLiteral("refusing recursive opener; use explicit app exec");
        return r;
    }
    const QStringList args = substitute(argsTemplate, isoPath, entry);
    qint64 pid = 0;
    QProcess p;
    p.setProgram(exec);
    p.setArguments(args);
    p.setWorkingDirectory(QFileInfo(isoPath).absolutePath());
    p.setStandardOutputFile(QProcess::nullDevice());
    p.setStandardErrorFile(QProcess::nullDevice());
    p.setStandardInputFile(QProcess::nullDevice());
    const bool ok = p.startDetached(&pid);
    r.ok = ok;
    r.pid = pid;
    r.message = ok ? QStringLiteral("launched %1 pid=%2").arg(exec).arg(pid)
                   : QStringLiteral("failed to launch %1").arg(exec);
    return r;
}

Launcher::Result Launcher::launchClass(const QString &classId, const QString &isoPath, const QString &entry) const
{
    const QString nid = Config::normalizeClassId(classId);
    for (const auto &c : m_cfg.classes()) {
        if (c.id == nid) return launchCommand(c.exec, c.args, isoPath, entry);
    }
    Result r; r.message = QStringLiteral("unknown class: ") + classId; return r;
}

} // namespace iso
