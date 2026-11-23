#pragma once

#include "Config.h"
#include "Classifier.h"
#include "IsoProbe.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace iso {

struct MarkerProposal {
    bool ok = false;
    QString status;          // proposed | unchanged | ambiguous | error
    QString message;
    QString markerPath;      // proposed .psXboot.iso path
    QString markerContent;   // full text content
    QString classId;
    QString entry;           // relative to marker dir
    QStringList candidates;  // ambiguous candidates
    QJsonObject toJson() const;
};

class MarkerMaker
{
public:
    MarkerMaker(const Config &cfg, IsoProbe &probe, Classifier &classifier);

    MarkerProposal propose(const QString &inputPath, const QString &forcedClass = QString()) const;

private:
    QStringList findCandidates(const QString &dir, const QString &classId) const;
    QString pickEntry(const QString &dir, const QString &classId, QStringList *ambiguous) const;
    QString inferClass(const QString &dir, const QString &entryAbs) const;
    static QString markerFileName(const QString &dir, const QString &classId);
    static QString relativeEntry(const QString &dir, const QString &absPath);

    const Config &m_cfg;
    IsoProbe &m_probe;
    Classifier &m_classifier;
};

} // namespace iso
