#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QDateTime>

namespace iso {

// One observed piece of evidence, e.g. key="pvd.system_id" detail="PLAYSTATION".
struct Feature {
    QString key;
    QString detail;
    QString source;                 // magic | pvd | raw | token | root | tree | file
    QString text() const { return key + (detail.isEmpty() ? QString() : QStringLiteral("=") + detail); }
};

// A single deterministic rule from the JSON config.
struct Rule {
    QString type;                   // magic|raw|sector|pvd.system_id|pvd.volume_id|token|mime|root|tree
    QStringList needles;
    int window = 0;                 // raw/token scan window; 0 = scan.window_bytes
    int offset = 28;                // magic offset (0x1C for Wii)
    int score = 60;
};

struct ClassDef {
    QString id;
    QString label;
    QString exec;
    QStringList args;               // %f = iso path, %e = entry inside iso
    int priority = 0;
    QList<Rule> rules;
};

struct ProbeResult {
    QString path;
    qint64 size = 0;
    QString fingerprint;

    QString magicDesc;              // optional `file -b` output
    bool wiiMagic = false;
    bool udf = false;
    bool iso9660 = false;
    bool sector2352 = false;
    int sectorSize = 2048;

    bool pvdValid = false;
    QString systemId;
    QString volumeId;
    QString publisher;
    qlonglong rootLba = 0;
    int rootLen = 0;

    QStringList rootNames;
    QStringList treeNames;
    QList<Feature> features;
    QStringList warnings;           // non-fatal: e.g. isoinfo missing
    QString error;

    // Boot-marker shortcut (portable fake .iso/.txt marker pointing to a real entry).
    bool bootMarker = false;
    QString bootClass;
    QString bootEntry;              // resolved absolute path to launch

    QString launchPath() const { return bootMarker && !bootEntry.isEmpty() ? bootEntry : path; }

    QSet<QString> featureSet() const {
        QSet<QString> s;
        for (const auto &f : features) s.insert(f.text());
        return s;
    }
};

struct ScoreBreak {
    QString classId;
    double base = 0.0;              // deterministic rules from config
    double learned = 0.0;           // DB weight contribution (capped)
    double total = 0.0;
    QStringList reasons;
};

struct Decision {
    QString classId;                // best guess
    double score = 0.0;
    double confidence = 0.0;        // (best - second) / (best + 1)
    bool ask = false;               // prompt requested
    QList<ScoreBreak> all;          // ordered by total desc

    QString label(const QList<ClassDef> &classes) const {
        for (const auto &c : classes) if (c.id == classId) return c.label;
        return classId;
    }
};

struct LearnedStat {
    int pos = 0;
    int neg = 0;
    double weight = 0.0;
    QDateTime updated;
};

} // namespace iso
