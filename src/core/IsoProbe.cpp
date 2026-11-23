#include "IsoProbe.h"
#include "Util.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>

namespace iso {

IsoProbe::IsoProbe(const Config &cfg) : m_cfg(cfg) {}

QByteArray IsoProbe::readWindow(const QString &path, qint64 offset, int size) const
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    if (offset > 0 && !f.seek(offset)) return {};
    return f.read(size);
}

QString IsoProbe::fileMagic(const QString &path) const
{
    if (!m_cfg.useFileProbe()) return {};
    QProcess p;
    p.start(QStringLiteral("file"), {QStringLiteral("-b"), path});
    if (!p.waitForFinished(3000)) { p.kill(); return {}; }
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

void IsoProbe::parsePvd(ProbeResult &r, const QByteArray &pvd) const
{
    // ISO 9660 Primary Volume Descriptor: PVD at 0x8000, magic CD001 at +1.
    if (pvd.size() < 2048) return;
    const QByteArray sig = pvd.mid(1, 5);
    if (sig != "CD001") return;
    r.pvdValid = true;
    r.systemId = util::cleanIsoString(pvd.mid(8, 32));
    r.volumeId = util::cleanIsoString(pvd.mid(40, 32));
    r.publisher = util::cleanIsoString(pvd.mid(318, 128));

    // Root Directory Record at 156 (34 bytes in the PVD).
    const QByteArray rr = pvd.mid(156, 34);
    if (rr.size() >= 34) {
        auto le32 = [&](int off) -> quint32 {
            return quint32(quint8(rr[off])) | (quint32(quint8(rr[off+1])) << 8) | (quint32(quint8(rr[off+2])) << 16) | (quint32(quint8(rr[off+3])) << 24);
        };
        r.rootLba = qlonglong(le32(2));
        r.rootLen = int(le32(10));
    }

    r.features.append({QStringLiteral("pvd.system_id"), r.systemId.toUpper(), QStringLiteral("pvd")});
    r.features.append({QStringLiteral("pvd.volume_id"), r.volumeId.toUpper(), QStringLiteral("pvd")});
    if (!r.publisher.isEmpty()) r.features.append({QStringLiteral("pvd.publisher"), r.publisher.toUpper(), QStringLiteral("pvd")});
}

void IsoProbe::readRootDir(ProbeResult &r) const
{
    if (!r.pvdValid || r.rootLba <= 0) return;
    const qint64 off = r.rootLba * qint64(r.sectorSize);
    const int want = std::min(qMax(2048, r.rootLen), m_cfg.rootDirBytes());
    const QByteArray data = readWindow(r.path, off, want);
    if (data.isEmpty()) return;

    int i = 0;
    while (i < data.size()) {
        quint8 len = quint8(data[i]);
        if (len == 0) {
            // Next sector boundary or end.
            int next = ((i / r.sectorSize) + 1) * r.sectorSize;
            if (next <= i) next = i + 1;
            i = next;
            continue;
        }
        if (i + len > data.size()) break;
        const QByteArray rec = data.mid(i, len);
        if (rec.size() >= 33) {
            quint8 nameLen = quint8(rec[32]);
            if (33 + nameLen <= rec.size()) {
                QByteArray name = rec.mid(33, nameLen);
                // Skip . and ..
                if (!(nameLen == 1 && (quint8(name[0]) == 0x00 || quint8(name[0]) == 0x01))) {
                    QString n = util::cleanIsoString(name);
                    // Strip version ;1
                    int sc = n.indexOf(QLatin1Char(';'));
                    if (sc >= 0) n = n.left(sc);
                    n = n.trimmed().toUpper();
                    if (!n.isEmpty()) r.rootNames << n;
                }
            }
        }
        i += len;
    }
    r.rootNames.removeDuplicates();
}

void IsoProbe::readTree(ProbeResult &r) const
{
    // Bounded directory walk through isoinfo metadata. This reads ISO content metadata,
    // not the file's path/folder on disk.
    const QString isoInfo = QStandardPaths::findExecutable(QStringLiteral("isoinfo"));
    if (isoInfo.isEmpty()) {
        r.warnings << QStringLiteral("isoinfo niet gevonden; tree/root-metadata scan is beperkt. Installeer voor completere detectie: sudo apt install genisoimage (of wodim/cdrkit afhankelijk van distro).");
        return;
    }
    QProcess p;
    p.start(isoInfo, {QStringLiteral("-f"), QStringLiteral("-i"), r.path});
    if (!p.waitForFinished(5000)) {
        p.kill();
        r.warnings << QStringLiteral("isoinfo timeout/fout; tree-metadata scan overgeslagen.");
        return;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        r.warnings << QStringLiteral("isoinfo gaf een fout terug; tree-metadata scan mogelijk onvolledig.");
        // Continue parsing any partial output.
    }
    const QByteArray out = p.readAllStandardOutput();
    int count = 0;
    const int maxDirs = m_cfg.maxDirs();
    const QList<QByteArray> lines = out.split('\n');
    for (const QByteArray &line : lines) {
        if (count >= maxDirs) break;
        QString s = util::cleanIsoString(line);
        if (s.isEmpty()) continue;
        ++count;
        QString up = s.toUpper();
        int slash = up.lastIndexOf(QLatin1Char('/'));
        QString base = slash >= 0 ? up.mid(slash + 1) : up;
        int sc = base.indexOf(QLatin1Char(';'));
        if (sc >= 0) base = base.left(sc);
        if (!base.isEmpty()) r.treeNames << base;
    }
    r.treeNames.removeDuplicates();
}

void IsoProbe::scanTokens(ProbeResult &r, const QByteArray &window) const
{
    const QByteArray up = window.toUpper();
    auto has = [&](const char *tok) { return up.contains(QByteArray(tok)); };
    if (has("BOOT2 =")) r.features.append({QStringLiteral("token"), QStringLiteral("BOOT2 ="), QStringLiteral("token")});
    else if (has("BOOT =")) r.features.append({QStringLiteral("token"), QStringLiteral("BOOT ="), QStringLiteral("token")});
    if (has("SYSTEM.CNF")) r.features.append({QStringLiteral("token"), QStringLiteral("SYSTEM.CNF"), QStringLiteral("token")});
    if (has("PSP_GAME")) r.features.append({QStringLiteral("token"), QStringLiteral("PSP_GAME"), QStringLiteral("token")});
    if (has("PS3_DISC")) r.features.append({QStringLiteral("token"), QStringLiteral("PS3_DISC"), QStringLiteral("token")});
    if (has("MICROSOFT*XBOX*MEDIA")) r.features.append({QStringLiteral("token"), QStringLiteral("MICROSOFT*XBOX*MEDIA"), QStringLiteral("token")});
    if (has("DEFAULT.XBE")) r.features.append({QStringLiteral("token"), QStringLiteral("DEFAULT.XBE"), QStringLiteral("token")});
    if (has("SETUP.EXE")) r.features.append({QStringLiteral("token"), QStringLiteral("SETUP.EXE"), QStringLiteral("token")});
    if (has("AUTORUN.INF")) r.features.append({QStringLiteral("token"), QStringLiteral("AUTORUN.INF"), QStringLiteral("token")});
    if (has("INSTALL.EXE")) r.features.append({QStringLiteral("token"), QStringLiteral("INSTALL.EXE"), QStringLiteral("token")});
    if (has("EBOOT.BIN")) r.features.append({QStringLiteral("token"), QStringLiteral("EBOOT.BIN"), QStringLiteral("token")});
}

void IsoProbe::scanRootNames(ProbeResult &r) const
{
    for (const QString &n : r.rootNames) {
        r.features.append({QStringLiteral("root"), n, QStringLiteral("root")});
    }
}

void IsoProbe::scanUdfHint(ProbeResult &r) const
{
    // file -b is a content probe (magic/structure). Not a path rule.
    if (r.magicDesc.contains(QStringLiteral("UDF"), Qt::CaseInsensitive)
        || r.magicDesc.contains(QStringLiteral("application/x-udf"), Qt::CaseInsensitive)) {
        r.udf = true;
        r.features.append({QStringLiteral("mime"), QStringLiteral("application/x-udf-image"), QStringLiteral("file")});
    }
    if (r.magicDesc.contains(QStringLiteral("Nintendo Wii disc image"), Qt::CaseInsensitive)) {
        r.wiiMagic = true;
        r.features.append({QStringLiteral("magic"), QStringLiteral("WII"), QStringLiteral("file")});
    }
    if (r.magicDesc.contains(QStringLiteral("2352"), Qt::CaseInsensitive)) {
        r.sector2352 = true;
        r.sectorSize = 2352;
        r.features.append({QStringLiteral("sector"), QStringLiteral("2352"), QStringLiteral("file")});
    }
    if (r.magicDesc.contains(QStringLiteral("ISO 9660"), Qt::CaseInsensitive)) r.iso9660 = true;
}

ProbeResult IsoProbe::probe(const QString &isoPath) const
{
    ProbeResult r;
    QFileInfo fi(isoPath);
    r.path = fi.absoluteFilePath();
    r.size = fi.size();
    if (!fi.exists() || !fi.isFile()) { r.error = QStringLiteral("file not found"); return r; }

    // Boot-marker shortcut: portable text file that points to a real launch entry.
    {
        const QByteArray head0 = readWindow(r.path, 0, 4096);
        if (head0.startsWith("ISO-DISPATCHER-BOOT")) {
            r.bootMarker = true;
            r.magicDesc = QStringLiteral("iso-dispatcher boot marker");
            r.features.append({QStringLiteral("marker"), QStringLiteral("ISO-DISPATCHER-BOOT"), QStringLiteral("marker")});
            const QString text = QString::fromUtf8(head0);
            const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
            QString entry;
            for (const QString &line : lines) {
                const QString t = line.trimmed();
                if (t.startsWith(QLatin1Char('#')) || t.isEmpty()) continue;
                const int eq = t.indexOf(QLatin1Char('='));
                if (eq <= 0) continue;
                const QString k = t.left(eq).trimmed().toLower();
                const QString v = t.mid(eq + 1).trimmed();
                if (k == QStringLiteral("class")) r.bootClass = v;
                else if (k == QStringLiteral("entry") || k == QStringLiteral("path")) entry = v;
            }
            const QDir baseDir = fi.absoluteDir();
            if (!entry.isEmpty()) {
                QFileInfo ei(entry);
                r.bootEntry = ei.isAbsolute() ? ei.absoluteFilePath() : baseDir.absoluteFilePath(entry);
            } else if (r.bootClass.compare(QStringLiteral("ps3"), Qt::CaseInsensitive) == 0) {
                const QString autoEboot = baseDir.absoluteFilePath(QStringLiteral("PS3_GAME/USRDIR/EBOOT.BIN"));
                if (QFileInfo::exists(autoEboot)) r.bootEntry = autoEboot;
            } else if (r.bootClass.compare(QStringLiteral("ps2"), Qt::CaseInsensitive) == 0) {
                // PS2 extracted layout: SYSTEM.CNF contains BOOT2 = cdrom0:\SLUS_123.45;1
                const QString cnfPath = baseDir.absoluteFilePath(QStringLiteral("SYSTEM.CNF"));
                QFile cnf(cnfPath);
                if (cnf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString cnfText = QString::fromLatin1(cnf.readAll());
                    const QRegularExpression re(QStringLiteral("BOOT2\\s*=\\s*cdrom0:\\\\?([^;\\r\\n]+)"), QRegularExpression::CaseInsensitiveOption);
                    const QRegularExpressionMatch m = re.match(cnfText);
                    if (m.hasMatch()) {
                        QString boot = m.captured(1).trimmed();
                        boot.replace(QLatin1Char('\\'), QLatin1Char('/'));
                        // PS2 discs usually have the ELF at root; try as-is and uppercase variants.
                        const QString cand = baseDir.absoluteFilePath(boot);
                        if (QFileInfo::exists(cand)) r.bootEntry = QFileInfo(cand).absoluteFilePath();
                        else {
                            const QString up = baseDir.absoluteFilePath(boot.toUpper());
                            if (QFileInfo::exists(up)) r.bootEntry = QFileInfo(up).absoluteFilePath();
                        }
                    }
                }
            } else if (r.bootClass.compare(QStringLiteral("ps4"), Qt::CaseInsensitive) == 0) {
                const QString autoEboot = baseDir.absoluteFilePath(QStringLiteral("eboot.bin"));
                if (QFileInfo::exists(autoEboot)) r.bootEntry = autoEboot;
                else {
                    const QString upper = baseDir.absoluteFilePath(QStringLiteral("EBOOT.BIN"));
                    if (QFileInfo::exists(upper)) r.bootEntry = upper;
                }
            }
            if (!r.bootClass.isEmpty()) r.features.append({QStringLiteral("boot.class"), r.bootClass.toUpper(), QStringLiteral("marker")});
            if (!r.bootEntry.isEmpty()) r.features.append({QStringLiteral("boot.entry"), r.bootEntry, QStringLiteral("marker")});
            else r.warnings << QStringLiteral("Boot-marker gevonden maar geen geldige entry/EBOOT.BIN.");

            QCryptographicHash h(QCryptographicHash::Sha1);
            h.addData("boot-marker\n");
            h.addData(r.bootClass.toUtf8());
            h.addData(r.bootEntry.toUtf8());
            h.addData(r.path.toUtf8());
            r.fingerprint = QString::fromLatin1(h.result().toHex());
            return r;
        }
    }

    // Descriptor image formats: probe the referenced data file, but launch the descriptor.
    {
        const QString ext = fi.suffix().toLower();
        if (ext == QStringLiteral("cue") || ext == QStringLiteral("ccd") || ext == QStringLiteral("mds")) {
            QString dataFile;
            if (ext == QStringLiteral("cue")) {
                QFile cf(r.path);
                if (cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString txt = QString::fromLocal8Bit(cf.readAll());
                    const QRegularExpression re(QStringLiteral("^\\s*FILE\\s+[\"']?([^\"'\\r\\n]+)[\"']?"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
                    const QRegularExpressionMatch m = re.match(txt);
                    if (m.hasMatch()) {
                        QString ref = m.captured(1).trimmed();
                        QFileInfo ri(ref);
                        dataFile = ri.isAbsolute() ? ri.absoluteFilePath() : fi.absoluteDir().absoluteFilePath(ref);
                    }
                }
            } else if (ext == QStringLiteral("ccd")) {
                const QString img = fi.absoluteDir().absoluteFilePath(fi.completeBaseName() + QStringLiteral(".img"));
                if (QFileInfo::exists(img)) dataFile = img;
            } else if (ext == QStringLiteral("mds")) {
                const QString mdf = fi.absoluteDir().absoluteFilePath(fi.completeBaseName() + QStringLiteral(".mdf"));
                if (QFileInfo::exists(mdf)) dataFile = mdf;
            }
            if (!dataFile.isEmpty() && QFileInfo::exists(dataFile)) {
                ProbeResult pr = probe(dataFile);
                pr.bootMarker = true;
                pr.bootEntry = r.path;
                pr.bootClass.clear();
                pr.warnings << QStringLiteral("Descriptor-image: content geprobed via %1, launch via %2").arg(dataFile, r.path);
                pr.features.append({QStringLiteral("descriptor"), ext.toUpper(), QStringLiteral("file")});
                return pr;
            }
        }
    }

    // 1. Fast header (64K) + magic + Wii bytes + PVD.
    const QByteArray head = readWindow(r.path, 0, std::max(m_cfg.fastBytes(), 0x9000));
    r.magicDesc = fileMagic(r.path);
    scanUdfHint(r);

    // Wii magic at 0x1C: 5D 1C 9E A3
    if (head.size() > 0x20) {
        const QByteArray w = head.mid(0x18, 4).toHex().toUpper();
        if (w == "5D1C9EA3") { r.wiiMagic = true; r.features.append({QStringLiteral("magic"), QStringLiteral("5D1C9EA3@0x1C"), QStringLiteral("magic")}); }
    }

    // PVD at 0x8000 (2048 bytes) - content, not path.
    if (head.size() >= 0x8000 + 2048) {
        parsePvd(r, head.mid(0x8000, 2048));
    } else {
        parsePvd(r, readWindow(r.path, 0x8000, 2048));
    }

    // 2. Root dir seek (metadata, bounded).
    readRootDir(r);
    scanRootNames(r);

    // 3. Windowed token scan (bounded, 2MB default). No full ISO read.
    const int winSize = m_cfg.windowBytes();
    const QByteArray window = (winSize <= head.size()) ? head.left(winSize) : readWindow(r.path, 0, winSize);
    scanTokens(r, window);

    // 4. Bounded tree walk via isoinfo metadata (only when helpful / unresolved hints).
    readTree(r);

    // Fingerprint: size + first 64KB hash + volume_id. Cheap, stable on rename/move.
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(QByteArray::number(r.size));
    h.addData(head.left(65536));
    h.addData(r.volumeId.toUtf8());
    r.fingerprint = QString::fromLatin1(h.result().toHex());

    // Dedup features.
    QList<Feature> uniq;
    QSet<QString> seen;
    for (const auto &f : r.features) {
        const QString k = f.text();
        if (seen.contains(k)) continue;
        seen.insert(k);
        uniq << f;
    }
    r.features = uniq;

    return r;
}

} // namespace iso
