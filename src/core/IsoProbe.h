#pragma once

#include "Types.h"
#include "Config.h"

#include <QString>

namespace iso {

class IsoProbe
{
public:
    explicit IsoProbe(const Config &cfg);

    // Scan ISO content only; no path/folder use in classification.
    ProbeResult probe(const QString &isoPath) const;

private:
    QByteArray readWindow(const QString &path, qint64 offset, int size) const;
    QString fileMagic(const QString &path) const;
    void parsePvd(ProbeResult &r, const QByteArray &pvd) const;
    void readRootDir(ProbeResult &r) const;
    void readTree(ProbeResult &r) const;
    void scanTokens(ProbeResult &r, const QByteArray &window) const;
    void scanRootNames(ProbeResult &r) const;
    void scanUdfHint(ProbeResult &r) const;

    const Config &m_cfg;
};

} // namespace iso
