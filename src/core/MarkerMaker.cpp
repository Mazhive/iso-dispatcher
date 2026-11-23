#include "MarkerMaker.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace iso {

QJsonObject MarkerProposal::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("ok"), ok);
    o.insert(QStringLiteral("status"), status);
    o.insert(QStringLiteral("message"), message);
    o.insert(QStringLiteral("class"), classId);
    o.insert(QStringLiteral("entry"), entry);
    o.insert(QStringLiteral("markerPath"), markerPath);
    o.insert(QStringLiteral("markerContent"), markerContent);
    QJsonArray c; for (const QString &x : candidates) c.append(x);
    o.insert(QStringLiteral("candidates"), c);
    return o;
}

MarkerMaker::MarkerMaker(const Config &cfg, IsoProbe &probe, Classifier &classifier)
    : m_cfg(cfg), m_probe(probe), m_classifier(classifier) {}

static QString normExt(const QString &name)
{
    return QFileInfo(name).suffix().toLower();
}

static bool hasAnyExt(const QString &name, const QStringList &exts)
{
    const QString e = normExt(name);
    for (const QString &x : exts) if (e == x.toLower()) return true;
    return false;
}

QString MarkerMaker::relativeEntry(const QString &dir, const QString &absPath)
{
    QDir d(dir);
    QString rel = d.relativeFilePath(absPath);
    rel = QDir::cleanPath(rel);
    return rel;
}

QString MarkerMaker::markerFileName(const QString &dir, const QString &classId)
{
    const QString base = QFileInfo(dir).completeBaseName().isEmpty() ? QFileInfo(dir).fileName() : QFileInfo(dir).fileName();
    const QString safeBase = base.isEmpty() ? QStringLiteral("game") : base;
    return QDir(dir).absoluteFilePath(safeBase + QStringLiteral(".") + classId + QStringLiteral("boot.iso"));
}

QStringList MarkerMaker::findCandidates(const QString &dir, const QString &classId) const
{
    QDir d(dir);
    if (!d.exists()) return {};
    const QFileInfoList entries = d.entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot, QDir::Name);
    QStringList out;
    const QString cls = Config::normalizeClassId(classId);

    auto addIf = [&](const QFileInfo &fi, const QStringList &exts) {
        if (hasAnyExt(fi.fileName(), exts)) out << fi.absoluteFilePath();
    };

    if (cls == QStringLiteral("ps1") || cls == QStringLiteral("ps2")) {
        // B2: prefer .cue, then .chd/.iso/.bin (careful)
        for (const auto &fi : entries) addIf(fi, {QStringLiteral("cue")});
        if (out.isEmpty()) for (const auto &fi : entries) addIf(fi, {QStringLiteral("chd"), QStringLiteral("iso"), QStringLiteral("bin")});
        // Avoid selecting existing boot markers as entries
        QStringList filtered;
        for (const QString &p : out) {
            const QString n = QFileInfo(p).fileName().toLower();
            if (n.endsWith(QStringLiteral("boot.iso"))) continue;
            filtered << p;
        }
        return filtered;
    }
    if (cls == QStringLiteral("ps3")) {
        const QStringList eboots = {
            d.absoluteFilePath(QStringLiteral("PS3_GAME/USRDIR/EBOOT.BIN")),
            d.absoluteFilePath(QStringLiteral("USRDIR/EBOOT.BIN")),
            d.absoluteFilePath(QStringLiteral("EBOOT.BIN")),
        };
        for (const QString &p : eboots) if (QFileInfo::exists(p)) out << QFileInfo(p).absoluteFilePath();
        if (out.isEmpty()) for (const auto &fi : entries) addIf(fi, {QStringLiteral("iso")});
        return out;
    }
    if (cls == QStringLiteral("psp")) {
        for (const auto &fi : entries) addIf(fi, {QStringLiteral("iso"), QStringLiteral("cso"), QStringLiteral("pbp")});
        const QString eboot = d.absoluteFilePath(QStringLiteral("EBOOT.PBP"));
        if (out.isEmpty() && QFileInfo::exists(eboot)) out << QFileInfo(eboot).absoluteFilePath();
        QStringList filtered;
        for (const QString &p : out) {
            const QString n = QFileInfo(p).fileName().toLower();
            if (n.endsWith(QStringLiteral("boot.iso"))) continue;
            filtered << p;
        }
        return filtered;
    }

    // Generic: existing readable files with common disc extensions, exclude boot markers.
    for (const auto &fi : entries) {
        const QString n = fi.fileName().toLower();
        if (n.endsWith(QStringLiteral("boot.iso"))) continue;
        if (hasAnyExt(fi.fileName(), {QStringLiteral("cue"), QStringLiteral("iso"), QStringLiteral("chd"), QStringLiteral("ccd"), QStringLiteral("mds"), QStringLiteral("pbp"), QStringLiteral("cso"), QStringLiteral("bin")})) {
            out << fi.absoluteFilePath();
        }
    }
    return out;
}

QString MarkerMaker::pickEntry(const QString &dir, const QString &classId, QStringList *ambiguous) const
{
    const QStringList cands = findCandidates(dir, classId);
    if (ambiguous) *ambiguous = cands;
    if (cands.isEmpty()) return {};
    const QString cls = Config::normalizeClassId(classId);

    if (cls == QStringLiteral("ps1") || cls == QStringLiteral("ps2") || cls == QStringLiteral("psp")) {
        // If we selected by extension priority in findCandidates, first group is preferred.
        // For cue group: if exactly 1 cue, pick it; if multiple cues, ambiguous.
        QStringList cues, others;
        for (const QString &p : cands) {
            if (normExt(p) == QStringLiteral("cue")) cues << p;
            else others << p;
        }
        if (!cues.isEmpty()) {
            if (cues.size() == 1) return cues.first();
            if (ambiguous) *ambiguous = cues;
            return {};
        }
        if (others.size() == 1) return others.first();
        if (ambiguous) *ambiguous = others;
        return {};
    }

    if (cls == QStringLiteral("ps3")) {
        // findCandidates returns EBOOT candidates in priority order
        if (!cands.isEmpty()) return cands.first();
        return {};
    }

    if (cands.size() == 1) return cands.first();
    return {};
}

QString MarkerMaker::inferClass(const QString &dir, const QString &entryAbs) const
{
    // Try to infer by probing the selected entry.
    QString probePath = entryAbs;
    QFileInfo fi(entryAbs);
    if (fi.isDir()) {
        // try common files
        for (const QString &n : {QStringLiteral("SYSTEM.CNF"), QStringLiteral("PS3_GAME/USRDIR/EBOOT.BIN"), QStringLiteral("EBOOT.BIN")}) {
            const QString p = QDir(entryAbs).absoluteFilePath(n);
            if (QFileInfo::exists(p)) { probePath = p; break; }
        }
    }
    if (!probePath.isEmpty() && QFileInfo::exists(probePath)) {
        const ProbeResult pr = m_probe.probe(probePath);
        if (pr.error.isEmpty()) {
            const Decision d = m_classifier.classify(pr);
            if (!d.classId.isEmpty() && !d.ask) return Config::normalizeClassId(d.classId);
            // if ask, still return best guess? No - be explicit. But B1 allows optional class.
        }
    }

    // Heuristics from directory name/extensions
    const QString base = QFileInfo(dir).fileName().toLower();
    if (base.contains(QStringLiteral("ps3")) || QFileInfo::exists(QDir(dir).absoluteFilePath(QStringLiteral("PS3_GAME/USRDIR/EBOOT.BIN")))) return QStringLiteral("ps3");
    if (base.contains(QStringLiteral("psp")) ) return QStringLiteral("psp");
    if (base.contains(QStringLiteral("ps2"))) return QStringLiteral("ps2");
    // PSX/PS1 cue/bin common - but don't guess too hard; leave unknown.
    return {};
}

MarkerProposal MarkerMaker::propose(const QString &inputPath, const QString &forcedClass) const
{
    MarkerProposal p;
    QFileInfo in(inputPath);
    if (!in.exists()) {
        p.status = QStringLiteral("error");
        p.message = QStringLiteral("Input path not found: %1").arg(inputPath);
        return p;
    }

    QString dir = in.isDir() ? in.absoluteFilePath() : in.absoluteDir().absolutePath();
    QString entryAbs;
    if (in.isFile()) entryAbs = in.absoluteFilePath();

    QString cls = Config::normalizeClassId(forcedClass);
    if (!cls.isEmpty() && !m_cfg.hasClass(cls)) {
        p.status = QStringLiteral("error");
        p.message = QStringLiteral("Unknown class: %1").arg(forcedClass);
        p.classId = cls;
        return p;
    }

    if (cls.isEmpty()) {
        if (!entryAbs.isEmpty()) cls = inferClass(dir, entryAbs);
        else cls = inferClass(dir, dir);
    }
    if (cls.isEmpty()) {
        p.status = QStringLiteral("error");
        p.message = QStringLiteral("Cannot infer class for '%1'. Provide --class <id>.").arg(inputPath);
        return p;
    }
    p.classId = cls;

    QStringList ambiguous;
    if (entryAbs.isEmpty()) {
        entryAbs = pickEntry(dir, cls, &ambiguous);
        if (entryAbs.isEmpty()) {
            if (!ambiguous.isEmpty()) {
                p.status = QStringLiteral("ambiguous");
                p.message = QStringLiteral("Multiple entry candidates found for class %1; specify input file or resolve ambiguity").arg(cls);
                for (const QString &c : ambiguous) p.candidates << relativeEntry(dir, c);
                return p;
            }
            p.status = QStringLiteral("error");
            p.message = QStringLiteral("No suitable entry found in '%1' for class %2").arg(dir, cls);
            return p;
        }
    } else {
        // if file given, validate it's inside dir and relative-safe
        const QString rel = relativeEntry(dir, entryAbs);
        if (rel.startsWith(QStringLiteral(".."))) {
            p.status = QStringLiteral("error");
            p.message = QStringLiteral("Entry is outside target directory: %1").arg(entryAbs);
            return p;
        }
    }

    p.entry = relativeEntry(dir, entryAbs);
    p.markerPath = markerFileName(dir, cls);
    p.markerContent = QStringLiteral("ISO-DISPATCHER-BOOT\nclass=%1\nentry=%2\n").arg(cls, p.entry);

    // Check if existing marker at proposed path is identical
    QFileInfo mpi(p.markerPath);
    if (mpi.exists()) {
        QFile mf(p.markerPath);
        if (mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString existing = QString::fromUtf8(mf.readAll());
            if (existing == p.markerContent) {
                p.ok = true;
                p.status = QStringLiteral("unchanged");
                p.message = QStringLiteral("Marker already up-to-date: %1").arg(p.markerPath);
                return p;
            }
        }
        p.ok = true;
        p.status = QStringLiteral("exists");
        p.message = QStringLiteral("Marker already exists (different content): %1").arg(p.markerPath);
        return p;
    }

    p.ok = true;
    p.status = QStringLiteral("proposed");
    p.message = QStringLiteral("Proposed marker for %1").arg(dir);
    return p;
}

} // namespace iso
