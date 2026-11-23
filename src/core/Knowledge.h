#pragma once

#include "Types.h"
#include "Config.h"

#include <QHash>
#include <QString>
#include <QJsonArray>

namespace iso {

// JSON knowledge store: in-memory QHash, batch save, atomic write.
// No path/folder rules are stored here; only content features and fingerprints.
class Knowledge
{
public:
    explicit Knowledge(const Config &cfg);

    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;
    bool dirty() const { return m_dirty; }

    // Classification support
    double scoreContribution(const ProbeResult &probe, const QString &classId) const;
    QString rememberedClass(const QString &fingerprint) const;

    // Learning
    void rememberFingerprint(const QString &fingerprint, const QString &classId);
    void rememberSignature(const ProbeResult &probe, const QString &classId);
    void recordDecision(const ProbeResult &probe, const QString &chosen, bool prompted, bool overridden);

    // Negative cache: avoid repeated prompts after cancel.
    bool isNegativelyCached(const QString &fingerprint) const;
    void negativeCache(const QString &fingerprint);

    // Diagnostics
    QJsonObject stats(const QList<ClassDef> &classes) const;
    QJsonArray recentDecisions(int n = 20) const;
    bool forgetFingerprint(const QString &fingerprint);
    bool exportJson(const QString &path, QString *error = nullptr) const;

private:
    struct Weight { double pos = 0; double neg = 0; QString updated; };
    QString featureKey(const Feature &f) const { return f.text(); }
    double weightFor(const QString &feature, const QString &classId) const;
    void bump(const QString &feature, const QString &classId, bool positive, double amount);

    const Config &m_cfg;
    QString m_path;
    bool m_dirty = false;

    QHash<QString, QString> m_fpClass;                 // fingerprint -> classId
    QHash<QString, qint64> m_negCache;                 // fingerprint -> epoch sec
    QHash<QString, QHash<QString, Weight>> m_weights;  // feature -> classId -> weight
    QJsonArray m_decisions;                            // last N decisions (bounded)
};

} // namespace iso
