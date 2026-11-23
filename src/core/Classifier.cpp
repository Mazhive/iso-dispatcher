#include "Classifier.h"
#include "Util.h"

#include <algorithm>
#include <cmath>

namespace iso {

Classifier::Classifier(const Config &cfg, Knowledge &knowledge) : m_cfg(cfg), m_kb(knowledge) {}

static bool matchOne(const Rule &rule, const ProbeResult &probe)
{
    if (rule.type == QStringLiteral("magic")) {
        for (const auto &n : rule.needles) {
            const QString un = n.toUpper();
            if (un == QStringLiteral("5D1C9EA3") && probe.wiiMagic) return true;
        }
        return false;
    }
    if (rule.type == QStringLiteral("raw") || rule.type == QStringLiteral("token")) {
        for (const auto &f : probe.features) {
            if (f.key != QStringLiteral("token") && rule.type == QStringLiteral("token")) continue;
            for (const auto &n : rule.needles) {
                if (f.detail.toUpper() == n.toUpper()) return true;
            }
        }
        return false;
    }
    if (rule.type == QStringLiteral("sector")) {
        for (const auto &n : rule.needles) if (n == QStringLiteral("2352") && probe.sector2352) return true;
        return false;
    }
    if (rule.type == QStringLiteral("pvd.system_id")) {
        const QString sys = probe.systemId.toUpper();
        for (const auto &n : rule.needles) if (sys.contains(n.toUpper())) return true;
        return false;
    }
    if (rule.type == QStringLiteral("pvd.volume_id")) {
        const QString vol = probe.volumeId.toUpper();
        for (const auto &n : rule.needles) if (vol.contains(n.toUpper())) return true;
        return false;
    }
    if (rule.type == QStringLiteral("mime")) {
        for (const auto &f : probe.features) {
            if (f.key == QStringLiteral("mime")) {
                for (const auto &n : rule.needles) if (f.detail.compare(n, Qt::CaseInsensitive) == 0) return true;
            }
        }
        return false;
    }
    if (rule.type == QStringLiteral("root")) {
        for (const QString &rn : probe.rootNames) {
            for (const auto &n : rule.needles) if (util::wildcard(n, rn)) return true;
        }
        return false;
    }
    if (rule.type == QStringLiteral("tree")) {
        for (const QString &tn : probe.treeNames) {
            for (const auto &n : rule.needles) if (util::wildcard(n, tn)) return true;
        }
        return false;
    }
    return false;
}

double Classifier::applyRules(const ClassDef &cls, const ProbeResult &probe, QStringList &reasons) const
{
    double score = 0.0;
    for (const auto &rule : cls.rules) {
        if (matchOne(rule, probe)) {
            score += rule.score;
            reasons << QStringLiteral("[%1 +%2] %3=%4")
                           .arg(cls.id)
                           .arg(rule.score)
                           .arg(rule.type)
                           .arg(rule.needles.join(QStringLiteral("|")));
        }
    }
    return score;
}

Decision Classifier::classify(const ProbeResult &probe) const
{
    Decision d;
    const auto classes = m_cfg.classes();
    if (classes.isEmpty()) return d;

    // Explicit boot-marker shortcut wins.
    if (probe.bootMarker && !probe.bootClass.isEmpty()) {
        ScoreBreak b;
        b.classId = probe.bootClass;
        b.base = 1000;
        b.total = 1000;
        b.reasons << QStringLiteral("[boot-marker]");
        if (!probe.bootEntry.isEmpty()) b.reasons << QStringLiteral("entry=%1").arg(probe.bootEntry);
        d.classId = probe.bootClass;
        d.score = 1000;
        d.confidence = 1.0;
        d.ask = false;
        d.all << b;
        return d;
    }

    // Learned fingerprint override wins (explicitly remembered file).
    const QString remembered = m_kb.rememberedClass(probe.fingerprint);
    if (!remembered.isEmpty()) {
        ScoreBreak b; b.classId = remembered; b.base = 1000; b.total = 1000; b.reasons << QStringLiteral("[learned fingerprint override]");
        d.classId = remembered; d.score = 1000; d.confidence = 1.0; d.ask = false; d.all << b;
        return d;
    }

    QList<ScoreBreak> breaks;
    for (const auto &cls : classes) {
        ScoreBreak b;
        b.classId = cls.id;
        b.base = applyRules(cls, probe, b.reasons);
        if (m_cfg.learnEnabled()) {
            b.learned = m_kb.scoreContribution(probe, cls.id);
            // Cap DB share so stats never completely override deterministic rules.
            const double cap = std::abs(b.base) * m_cfg.maxDbShare() + 20.0;
            b.learned = std::clamp(b.learned, -cap, cap);
            if (std::abs(b.learned) > 0.01) b.reasons << QStringLiteral("[db %+1]").arg(b.learned, 0, 'f', 2);
        }
        b.total = b.base + b.learned;
        breaks << b;
    }

    std::sort(breaks.begin(), breaks.end(), [](const ScoreBreak &a, const ScoreBreak &b) {
        if (a.total != b.total) return a.total > b.total;
        return a.base > b.base;
    });

    d.all = breaks;
    d.classId = breaks.first().classId;
    d.score = breaks.first().total;

    const double best = breaks.size() > 0 ? breaks[0].total : 0.0;
    const double second = breaks.size() > 1 ? breaks[1].total : 0.0;
    d.confidence = (best - second) / (std::abs(best) + 1.0);

    const bool lowScore = best < m_cfg.askMinScore();
    const bool tie = (best - second) < m_cfg.tieMargin() && best > 0;
    const bool noSignal = best <= 0.0;
    d.ask = m_cfg.askEnabled() && (lowScore || tie || noSignal);
    return d;
}

QStringList Classifier::explain(const ProbeResult &probe, const Decision &d) const
{
    QStringList out;
    out << QStringLiteral("file: %1 (%2 bytes)").arg(probe.path).arg(probe.size);
    out << QStringLiteral("fingerprint: %1").arg(probe.fingerprint);
    out << QStringLiteral("magic: %1").arg(probe.magicDesc.isEmpty() ? QStringLiteral("-") : probe.magicDesc);
    out << QStringLiteral("pvd.system_id: %1").arg(probe.systemId.isEmpty() ? QStringLiteral("-") : probe.systemId);
    out << QStringLiteral("pvd.volume_id: %1").arg(probe.volumeId.isEmpty() ? QStringLiteral("-") : probe.volumeId);
    out << QStringLiteral("root: %1").arg(probe.rootNames.mid(0, 12).join(QStringLiteral(", ")));
    out << QStringLiteral("decision: %1 score=%2 conf=%3 ask=%4")
               .arg(d.classId).arg(d.score, 0, 'f', 1).arg(d.confidence, 0, 'f', 3).arg(d.ask ? "yes" : "no");
    for (const auto &b : d.all.mid(0, 5)) {
        out << QStringLiteral("  - %1: base=%2 db=%3 total=%4")
                   .arg(b.classId).arg(b.base, 0, 'f', 1).arg(b.learned, 0, 'f', 2).arg(b.total, 0, 'f', 1);
        for (const auto &r : b.reasons.mid(0, 6)) out << QStringLiteral("      %1").arg(r);
    }
    return out;
}

} // namespace iso
