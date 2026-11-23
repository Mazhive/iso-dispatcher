#include "Cli.h"
#include "core/MarkerMaker.h"
#include "core/Util.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <iostream>

namespace iso {

static QTextStream out(stdout);
static QTextStream err(stderr);

Cli::Cli(Context ctx) : m(ctx) {}

QString Cli::helpText()
{
    return QStringLiteral(
        "iso-dispatcher - content-based ISO classifier/launcher\n\n"
        "Usage:\n"
        "  iso-dispatcher                          Start GUI\n"
        "  iso-dispatcher <file.iso>               Classify and launch (asks on low confidence)\n"
        "  iso-dispatcher gui [file.iso]           Start GUI, optionally preloading an ISO\n"
        "  iso-dispatcher classify <file.iso>      Classify only, print decision\n"
        "  iso-dispatcher why <file.iso>           Show full score/evidence breakdown\n"
        "  iso-dispatcher launch <file.iso> [--class id] [--dry-run]\n"
        "  iso-dispatcher learn <dir> [--dry-run] [--limit N]  Seed knowledge from ISO library\n"
        "  iso-dispatcher make-marker <path> [--class id] [--dry-run] [--force] [--json]\n"
        "  iso-dispatcher make-marker --batch <dir> [--class id] [--limit N] [--dry-run] [--force] [--json]\n"
        "                                          Propose/create boot markers (single or batch)\n"
        "  iso-dispatcher stats                    Knowledge stats\n"
        "  iso-dispatcher decisions [N]            Recent decisions\n"
        "  iso-dispatcher forget <fingerprint>     Forget a learned fingerprint\n"
        "  iso-dispatcher export <path.json>       Export knowledge\n"
        "  iso-dispatcher config path              Print config path\n"
        "  iso-dispatcher config init              Write default config\n"
        "  iso-dispatcher config get <key>         Get config value (dot path, e.g. scan/window_bytes)\n"
        "  iso-dispatcher config set <key> <val>   Set config value\n"
        "  iso-dispatcher about                    Show about / required emulators\n"
        "  iso-dispatcher --help | -h | ?          This help\n\n"
        "Boot markers:\n"
        "  - Text file starting with ISO-DISPATCHER-BOOT.\n"
        "  - Required fields: class=<id> and entry=<relative path> (or auto-resolve for known layouts).\n"
        "  - Entry must be relative/safe; cue FILE references are validated before launch.\n"
        "  - Class alias: psx -> ps1.\n\n"
        "Exit codes (CLI):\n"
        "  0 success / dry-run OK\n"
        "  1 general/usage error\n"
        "  2 marker invalid\n"
        "  3 entry/reference missing or invalid\n"
        "  4 class unknown/not configured\n"
        "  5 emulator/exec missing or launch failed\n\n"
        "Notes:\n"
        "  - Classification is CONTENT-ONLY. No folder/path rules are used.\n"
        "  - Config: $HOME/.config/iso-dispatcher.json\n"
        "  - Knowledge: configured by learn/db (default ~/.local/share/iso-dispatcher/knowledge.json)\n");
}

int Cli::run(const QStringList &args)
{
    if (args.size() <= 1) return -1; // GUI
    const QString cmd = args.at(1).trimmed();
    if (cmd == QStringLiteral("--help") || cmd == QStringLiteral("-h") || cmd == QStringLiteral("?") || cmd == QStringLiteral("help")) {
        out << helpText() << Qt::flush;
        return 0;
    }
    if (cmd == QStringLiteral("gui")) return -1;
    if (cmd == QStringLiteral("classify")) return cmdClassify(args.mid(2));
    if (cmd == QStringLiteral("why")) return cmdWhy(args.mid(2));
    if (cmd == QStringLiteral("launch")) return cmdLaunch(args.mid(2));
    if (cmd == QStringLiteral("learn")) return cmdLearn(args.mid(2));
    if (cmd == QStringLiteral("make-marker") || cmd == QStringLiteral("makemarker") || cmd == QStringLiteral("make_marker")) return cmdMakeMarker(args.mid(2));
    if (cmd == QStringLiteral("stats")) return cmdStats();
    if (cmd == QStringLiteral("decisions")) return cmdDecisions(args.mid(2));
    if (cmd == QStringLiteral("forget")) return cmdForget(args.mid(2));
    if (cmd == QStringLiteral("export")) return cmdExport(args.mid(2));
    if (cmd == QStringLiteral("config")) return cmdConfig(args.mid(2));
    if (cmd == QStringLiteral("about") || cmd == QStringLiteral("--about") || cmd == QStringLiteral("--version") || cmd == QStringLiteral("-V")) {
        out << "iso-dispatcher 0.0.7\n"
            << "Content-based ISO classifier/launcher\n\n"
            << "Required emulators/applications:\n"
            << "  - PS1 / PSX  : DuckStation or RetroArch (PSX core)\n"
            << "  - PS2        : PCSX2 or another PS2 emulator\n"
            << "  - PS3        : RPCS3\n"
            << "  - PS4        : shadPS4 (shadPS4QtLauncher)\n"
            << "  - PSP        : PPSSPP\n"
            << "  - Wii/GameCube: Dolphin\n"
            << "  - Xbox       : xemu or another Xbox emulator\n"
            << "  - Amiga      : FS-UAE / UAE / RetroArch Amiga core\n"
            << "  - PC/DOS     : DOSBox, Wine or native application (depending on image)\n\n"
            << "Configure exact paths/arguments via Edit Config or ~/.config/iso-dispatcher.json.\n" << Qt::flush;
        return 0;
    }

    // If it looks like a file, treat as launch.
    QFileInfo fi(cmd);
    if (fi.exists() && fi.isFile()) {
        QStringList a; a << cmd;
        return cmdLaunch(a);
    }

    err << "Unknown command: " << cmd << "\nUse --help or ?\n" << Qt::flush;
    return 2;
}

int Cli::cmdClassify(const QStringList &a)
{
    if (a.isEmpty()) { err << "classify: missing <file.iso>\n"; return 1; }
    const ProbeResult pr = m.probe.probe(a.first());
    if (!pr.error.isEmpty()) { err << pr.error << "\n"; return pr.errorCode ? pr.errorCode : 1; }
    const Decision d = m.classifier.classify(pr);
    QJsonObject o;
    o.insert(QStringLiteral("file"), pr.path);
    o.insert(QStringLiteral("fingerprint"), pr.fingerprint);
    o.insert(QStringLiteral("class"), d.classId);
    o.insert(QStringLiteral("label"), d.label(m.cfg.classes()));
    o.insert(QStringLiteral("score"), d.score);
    o.insert(QStringLiteral("confidence"), d.confidence);
    o.insert(QStringLiteral("ask"), d.ask);
    o.insert(QStringLiteral("bootMarker"), pr.bootMarker);
    if (pr.bootMarker) o.insert(QStringLiteral("bootEntry"), pr.bootEntry);
    QJsonArray feats; for (const auto &f : pr.features) feats.append(f.text()); o.insert(QStringLiteral("features"), feats);
    if (!pr.warnings.isEmpty()) { QJsonArray w; for (const auto &x : pr.warnings) w.append(x); o.insert(QStringLiteral("warnings"), w); }
    out << QJsonDocument(o).toJson(QJsonDocument::Indented) << Qt::flush;
    return 0;
}

int Cli::cmdWhy(const QStringList &a)
{
    if (a.isEmpty()) { err << "why: missing <file.iso>\n"; return 1; }
    const ProbeResult pr = m.probe.probe(a.first());
    if (!pr.error.isEmpty()) { err << pr.error << "\n"; return pr.errorCode ? pr.errorCode : 1; }
    const Decision d = m.classifier.classify(pr);
    for (const QString &line : m.classifier.explain(pr, d)) out << line << "\n";
    for (const QString &w : pr.warnings) err << "WARNING: " << w << "\n";
    if (!pr.warnings.isEmpty()) err << Qt::flush;
    out << Qt::flush;
    return 0;
}

int Cli::cmdLaunch(const QStringList &a)
{
    if (a.isEmpty()) { err << "launch: missing <file.iso>\n"; return 1; }
    QString iso; QString forceClass; bool dry = false;
    for (int i = 0; i < a.size(); ++i) {
        const QString x = a.at(i);
        if (x == QStringLiteral("--class") && i + 1 < a.size()) { forceClass = a.at(++i); }
        else if (x == QStringLiteral("--dry-run")) dry = true;
        else if (iso.isEmpty()) iso = x;
    }
    const ProbeResult pr = m.probe.probe(iso);
    if (!pr.error.isEmpty()) { err << pr.error << "\n"; return pr.errorCode ? pr.errorCode : 1; }

    Decision d = m.classifier.classify(pr);
    QString chosen = forceClass.isEmpty() ? d.classId : Config::normalizeClassId(forceClass);
    bool overridden = !forceClass.isEmpty() && chosen != d.classId;

    if (!forceClass.isEmpty() && !m.cfg.hasClass(chosen)) {
        err << "Unknown class: '" << forceClass << "' (normalized: '" << chosen << "')\n";
        err << "Known classes:";
        for (const auto &c : m.cfg.classes()) err << " " << c.id;
        err << "\n" << Qt::flush;
        return 4;
    }

    if (forceClass.isEmpty() && d.ask) {
        if (m.cfg.headless() == QStringLiteral("abort")) { err << "ambiguous; ask disabled in headless mode\n"; return 1; }
        out << "Uncertain classification for: " << pr.path << "\n";
        out << "Best guess: " << d.classId << " score=" << d.score << " conf=" << d.confidence << "\n";
        if (m.cfg.showEvidence()) {
            out << "Evidence: "; for (const auto &f : pr.features.mid(0, 12)) out << f.text() << " "; out << "\n";
        }
        const auto classes = m.cfg.classes();
        for (int i = 0; i < classes.size(); ++i) out << QStringLiteral("  %1) %2 [%3]\n").arg(i + 1).arg(classes[i].label, classes[i].id);
        out << "Select number (Enter=best guess, q=cancel): " << Qt::flush;
        std::string line; std::getline(std::cin, line);
        QString sel = QString::fromLocal8Bit(line.c_str()).trimmed();
        if (sel.compare(QStringLiteral("q"), Qt::CaseInsensitive) == 0) {
            m.kb.negativeCache(pr.fingerprint); m.kb.save(); return 1;
        }
        if (!sel.isEmpty()) {
            bool ok = false; int idx = sel.toInt(&ok);
            if (ok && idx >= 1 && idx <= classes.size()) { chosen = classes[idx - 1].id; overridden = chosen != d.classId; }
            else { chosen = Config::normalizeClassId(sel); overridden = chosen != d.classId; }
            if (!m.cfg.hasClass(chosen)) {
                err << "Unknown class: '" << sel << "'\n" << Qt::flush;
                return 4;
            }
        }
        m.kb.rememberFingerprint(pr.fingerprint, chosen);
    }

    if (chosen.isEmpty()) {
        const auto fb = m.cfg.fallbackElse();
        if (!fb.isEmpty()) chosen = QStringLiteral("__fallback__");
    }
    if (chosen.isEmpty()) { err << "No class selected and no fallback configured\n"; return 4; }

    if (chosen != QStringLiteral("__fallback__") && !m.cfg.hasClass(chosen)) {
        err << "Unknown/unconfigured class: '" << chosen << "'\n";
        return 4;
    }

    out << "Chosen class: " << chosen << "\n";
    if (pr.bootMarker) {
        out << "Boot marker: " << pr.path << "\n";
        out << "Resolved entry: " << pr.bootEntry << "\n";
    }
    auto execMissing = [](const QString &exec) {
        const QString e = exec.trimmed();
        if (e.isEmpty()) return true;
        QFileInfo fi(e);
        if (fi.isAbsolute() || e.contains(QLatin1Char('/'))) return !fi.exists();
        return QStandardPaths::findExecutable(e).isEmpty();
    };

    if (dry) {
        bool found = false;
        for (const auto &c : m.cfg.classes()) {
            if (c.id == Config::normalizeClassId(chosen)) {
                out << "Exec: " << c.exec << "\n";
                out << "Args: " << Launcher::substitute(c.args, pr.launchPath(), QString()).join(QLatin1Char(' ')) << "\n";
                if (execMissing(c.exec)) { err << "Emulator/exec missing for class " << c.id << ": " << c.exec << "\n" << Qt::flush; return 5; }
                found = true; break;
            }
        }
        if (!found) {
            const auto fb = m.cfg.fallbackElse();
            if (!fb.isEmpty()) {
                out << "Exec: " << fb.first() << "\nArgs: " << Launcher::substitute(fb.mid(1), pr.launchPath(), QString()).join(QLatin1Char(' ')) << "\n";
                if (execMissing(fb.first())) { err << "Fallback exec missing: " << fb.first() << "\n" << Qt::flush; return 5; }
            } else { err << "No exec/fallback found for class " << chosen << "\n"; return 4; }
        }
        out << "(dry-run) not launching\n" << Qt::flush;
        return 0;
    }

    for (const auto &c : m.cfg.classes()) {
        if (c.id == Config::normalizeClassId(chosen)) {
            if (execMissing(c.exec)) { err << "Emulator/exec missing for class " << c.id << ": " << c.exec << "\n" << Qt::flush; return 5; }
            break;
        }
    }

    m.kb.recordDecision(pr, chosen, d.ask, overridden);
    auto lr = m.launcher.launchClass(chosen, pr.launchPath());
    if (!lr.ok && m.cfg.fallbackAction() == QStringLiteral("ask")) {
        const auto fb = m.cfg.fallbackElse();
        if (!fb.isEmpty()) lr = m.launcher.launchCommand(fb.first(), fb.mid(1), pr.launchPath());
    }
    m.kb.save();
    out << lr.message << "\n" << Qt::flush;
    if (lr.ok) return 0;
    if (lr.message.contains(QStringLiteral("unknown class"))) return 4;
    if (lr.message.contains(QStringLiteral("failed to launch")) || lr.message.contains(QStringLiteral("empty exec"))) return 5;
    return 1;
}

int Cli::cmdLearn(const QStringList &a)
{
    if (a.isEmpty()) { err << "learn: missing <dir>\n"; return 2; }
    QString dir; bool dry = false; int limit = 0;
    for (int i = 0; i < a.size(); ++i) {
        const QString x = a.at(i);
        if (x == QStringLiteral("--dry-run")) dry = true;
        else if (x == QStringLiteral("--limit") && i + 1 < a.size()) limit = a.at(++i).toInt();
        else if (dir.isEmpty()) dir = x;
    }
    int count = 0, learned = 0;
    QDirIterator it(dir, {QStringLiteral("*.iso"), QStringLiteral("*.ISO"), QStringLiteral("*.cue"), QStringLiteral("*.CUE"), QStringLiteral("*.ccd"), QStringLiteral("*.CCD"), QStringLiteral("*.mds"), QStringLiteral("*.MDS"), QStringLiteral("*.chd"), QStringLiteral("*.CHD"), QStringLiteral("*.pbp"), QStringLiteral("*.PBP")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        if (limit > 0 && count >= limit) break;
        ++count;
        const ProbeResult pr = m.probe.probe(f);
        if (!pr.error.isEmpty()) { err << "skip: " << f << " : " << pr.error << "\n"; continue; }
        const Decision d = m.classifier.classify(pr);
        out << QStringLiteral("[%1] %2 -> %3 score=%4 ask=%5\n").arg(count).arg(QFileInfo(f).fileName(), d.classId).arg(d.score, 0, 'f', 1).arg(d.ask ? "yes" : "no");
        if (!dry && !d.ask && !d.classId.isEmpty()) {
            m.kb.recordDecision(pr, d.classId, false, false);
            ++learned;
        }
    }
    if (!dry) m.kb.save();
    out << "Scanned: " << count << ", learned: " << learned << (dry ? " (dry-run)" : "") << "\n" << Qt::flush;
    return 0;
}

int Cli::cmdMakeMarker(const QStringList &a)
{
    if (a.isEmpty()) { err << "make-marker: missing <path>\n"; return 2; }
    QString input;
    QString forcedClass;
    bool dry = false;
    bool json = false;
    bool force = false;
    bool batch = false;
    int limit = 0;
    for (int i = 0; i < a.size(); ++i) {
        const QString x = a.at(i);
        if (x == QStringLiteral("--batch")) {
            batch = true;
            if (i + 1 < a.size() && !a.at(i + 1).startsWith(QStringLiteral("--")) && input.isEmpty()) {
                input = a.at(++i);
            }
        }
        else if ((x == QStringLiteral("--class") || x == QStringLiteral("--system")) && i + 1 < a.size()) forcedClass = a.at(++i);
        else if (x == QStringLiteral("--dry-run")) dry = true;
        else if (x == QStringLiteral("--json")) json = true;
        else if (x == QStringLiteral("--force")) force = true;
        else if (x == QStringLiteral("--limit") && i + 1 < a.size()) limit = a.at(++i).toInt();
        else if (x.startsWith(QStringLiteral("--class="))) forcedClass = x.mid(QStringLiteral("--class=").size());
        else if (x.startsWith(QStringLiteral("--system="))) forcedClass = x.mid(QStringLiteral("--system=").size());
        else if (x.startsWith(QStringLiteral("--limit="))) limit = x.mid(QStringLiteral("--limit=").size()).toInt();
        else if (input.isEmpty()) input = x;
        else { err << "make-marker: unknown argument: " << x << "\n"; return 2; }
    }
    if (input.isEmpty()) { err << "make-marker: missing <path>\n"; return 2; }
    if (batch) return cmdMakeMarkerBatch(input, forcedClass, dry, json, force, limit);

    MarkerMaker mm(m.cfg, m.probe, m.classifier);
    const MarkerProposal p = mm.propose(input, forcedClass);

    QString action = p.status; // proposed/unchanged/exists/ambiguous/error
    if (p.ok && p.status == QStringLiteral("exists") && force) action = QStringLiteral("overwrite");
    if (p.ok && p.status == QStringLiteral("proposed")) action = dry ? QStringLiteral("proposed") : QStringLiteral("create");

    auto emitJson = [&](int exitCode) {
        QJsonObject o = p.toJson();
        o.insert(QStringLiteral("dryRun"), dry);
        o.insert(QStringLiteral("force"), force);
        o.insert(QStringLiteral("action"), action);
        out << QJsonDocument(o).toJson(QJsonDocument::Indented) << Qt::flush;
        return exitCode;
    };

    if (json && !p.ok) return emitJson(p.status == QStringLiteral("ambiguous") ? 2 : 2);

    if (!json) {
        out << "Status: " << p.status << "\n";
        if (!p.classId.isEmpty()) out << "Class: " << p.classId << "\n";
        if (!p.entry.isEmpty()) out << "Entry: " << p.entry << "\n";
        if (!p.markerPath.isEmpty()) out << "Marker: " << p.markerPath << "\n";
        if (!p.candidates.isEmpty()) {
            out << "Candidates:\n";
            for (const QString &c : p.candidates) out << "  - " << c << "\n";
        }
        if (!p.markerContent.isEmpty()) {
            out << "Content:\n" << p.markerContent;
            if (!p.markerContent.endsWith(QLatin1Char('\n'))) out << "\n";
        }
        if (!p.message.isEmpty()) out << "Message: " << p.message << "\n";
    }

    if (!p.ok) {
        if (json) return emitJson(2);
        err << Qt::flush;
        return 2;
    }

    if (p.status == QStringLiteral("unchanged")) {
        if (!json) out << "Skipped: marker already up-to-date\n";
        return json ? emitJson(0) : (out << Qt::flush, 0);
    }

    if (p.status == QStringLiteral("exists") && !force) {
        if (!json) err << "Warning: marker exists with different content; use --force to overwrite: " << p.markerPath << "\n";
        return json ? emitJson(0) : (err << Qt::flush, 0);
    }

    if (dry) {
        if (!json) {
            if (action == QStringLiteral("overwrite")) out << "(dry-run) would overwrite\n";
            else out << "(dry-run) not writing\n";
        }
        return json ? emitJson(0) : (out << Qt::flush, 0);
    }

    QFile f(p.markerPath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (action == QStringLiteral("overwrite")) mode |= QIODevice::Truncate;
    else mode |= QIODevice::NewOnly;

    if (!f.open(mode)) {
        err << "Failed to write marker: " << p.markerPath << "\n" << Qt::flush;
        return json ? emitJson(1) : 1;
    }
    f.write(p.markerContent.toUtf8());
    f.close();

    if (action == QStringLiteral("overwrite")) {
        if (!json) out << "Overwritten: " << p.markerPath << "\n";
    } else {
        if (!json) out << "Created: " << p.markerPath << "\n";
    }
    return json ? emitJson(0) : (out << Qt::flush, 0);
}

int Cli::cmdMakeMarkerBatch(const QString &root, const QString &forcedClass, bool dry, bool json, bool force, int limit)
{
    QFileInfo ri(root);
    if (!ri.exists() || !ri.isDir()) {
        err << "make-marker --batch: not a directory: " << root << "\n" << Qt::flush;
        return 2;
    }

    MarkerMaker mm(m.cfg, m.probe, m.classifier);
    struct Counts { int created = 0, overwritten = 0, unchanged = 0, skipped = 0, ambiguous = 0, errors = 0, proposed = 0; } counts;
    QJsonArray items;

    auto handleProposal = [&](const QString &path) {
        const MarkerProposal p = mm.propose(path, forcedClass);
        // Skip irrelevant dirs with no candidates - common in recursive scan.
        if (!p.ok && p.status == QStringLiteral("error") && p.message.contains(QStringLiteral("No entry candidates found"))) {
            return;
        }

        QString action = p.status;
        int itemRc = 0;
        if (p.ok && p.status == QStringLiteral("exists") && force) action = QStringLiteral("overwrite");
        if (p.ok && p.status == QStringLiteral("proposed")) action = dry ? QStringLiteral("proposed") : QStringLiteral("create");

        if (!p.ok) {
            if (p.status == QStringLiteral("ambiguous")) { counts.ambiguous++; itemRc = 2; }
            else { counts.errors++; itemRc = 2; }
        } else if (p.status == QStringLiteral("unchanged")) {
            counts.unchanged++;
        } else if (p.status == QStringLiteral("exists") && !force) {
            counts.skipped++; action = QStringLiteral("skipped");
        } else if (dry) {
            if (action == QStringLiteral("overwrite")) counts.overwritten++; else counts.proposed++;
        } else {
            QFile f(p.markerPath);
            QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
            if (action == QStringLiteral("overwrite")) mode |= QIODevice::Truncate;
            else mode |= QIODevice::NewOnly;
            if (!f.open(mode)) {
                counts.errors++; itemRc = 1; action = QStringLiteral("error");
            } else {
                f.write(p.markerContent.toUtf8());
                f.close();
                if (action == QStringLiteral("overwrite")) counts.overwritten++;
                else counts.created++;
            }
        }

        QJsonObject o = p.toJson();
        o.insert(QStringLiteral("path"), path);
        o.insert(QStringLiteral("action"), action);
        o.insert(QStringLiteral("dryRun"), dry);
        o.insert(QStringLiteral("force"), force);
        if (itemRc != 0) o.insert(QStringLiteral("exitCode"), itemRc);
        items.append(o);

        if (!json) {
            out << "[" << action << "] " << path;
            if (!p.classId.isEmpty()) out << " class=" << p.classId;
            if (!p.entry.isEmpty()) out << " entry=" << p.entry;
            if (!p.message.isEmpty() && action != QStringLiteral("proposed") && action != QStringLiteral("create") && action != QStringLiteral("overwrite") && action != QStringLiteral("unchanged")) out << " :: " << p.message;
            out << "\n";
        }
    };

    // Include root, then recurse. Deduplicate by canonical path.
    QSet<QString> seen;
    QStringList dirs;
    dirs << ri.absoluteFilePath();
    QDirIterator it(ri.absoluteFilePath(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) dirs << it.next();

    int processed = 0;
    for (const QString &d : dirs) {
        const QString canon = QFileInfo(d).canonicalFilePath();
        if (canon.isEmpty() || seen.contains(canon)) continue;
        seen.insert(canon);
        // Skip marker files themselves; propose expects dir/file input but dirs are our targets.
        if (d.endsWith(QStringLiteral(".ps1boot.iso"), Qt::CaseInsensitive) ||
            d.endsWith(QStringLiteral(".ps2boot.iso"), Qt::CaseInsensitive) ||
            d.endsWith(QStringLiteral(".ps3boot.iso"), Qt::CaseInsensitive) ||
            d.endsWith(QStringLiteral(".pspboot.iso"), Qt::CaseInsensitive))
            continue;
        // Quick prefilter: has relevant content? avoid calling propose on every random dir.
        bool relevant = false;
        {
            QDir qd(d);
            const QFileInfoList entries = qd.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &e : entries) {
                const QString n = e.fileName();
                if (e.isDir()) {
                    if (n.compare(QStringLiteral("PS3_GAME"), Qt::CaseInsensitive) == 0) { relevant = true; break; }
                    continue;
                }
                const QString suf = e.suffix().toLower();
                if (suf == QStringLiteral("cue") || suf == QStringLiteral("iso") || suf == QStringLiteral("chd") || suf == QStringLiteral("cso") || suf == QStringLiteral("pbp") || suf == QStringLiteral("bin") || suf == QStringLiteral("img") || suf == QStringLiteral("mdf")) { relevant = true; break; }
                if (n.compare(QStringLiteral("EBOOT.BIN"), Qt::CaseInsensitive) == 0) { relevant = true; break; }
                if (n.endsWith(QStringLiteral("boot.iso"), Qt::CaseInsensitive)) { relevant = true; break; }
            }
        }
        if (!relevant) continue;

        handleProposal(d);
        ++processed;
        if (limit > 0 && processed >= limit) break;
    }

    const int totalIssues = counts.ambiguous + counts.errors;
    if (json) {
        QJsonObject root;
        root.insert(QStringLiteral("root"), ri.absoluteFilePath());
        root.insert(QStringLiteral("dryRun"), dry);
        root.insert(QStringLiteral("force"), force);
        root.insert(QStringLiteral("limit"), limit);
        QJsonObject c;
        c.insert(QStringLiteral("created"), counts.created);
        c.insert(QStringLiteral("overwritten"), counts.overwritten);
        c.insert(QStringLiteral("unchanged"), counts.unchanged);
        c.insert(QStringLiteral("skipped"), counts.skipped);
        c.insert(QStringLiteral("ambiguous"), counts.ambiguous);
        c.insert(QStringLiteral("errors"), counts.errors);
        c.insert(QStringLiteral("proposed"), counts.proposed);
        c.insert(QStringLiteral("processed"), processed);
        root.insert(QStringLiteral("counts"), c);
        root.insert(QStringLiteral("items"), items);
        out << QJsonDocument(root).toJson(QJsonDocument::Indented) << Qt::flush;
    } else {
        out << "---\n";
        out << "Processed: " << processed
            << " created=" << counts.created
            << " overwritten=" << counts.overwritten
            << " unchanged=" << counts.unchanged
            << " skipped=" << counts.skipped
            << " ambiguous=" << counts.ambiguous
            << " errors=" << counts.errors
            << " proposed=" << counts.proposed
            << " dryRun=" << (dry ? "yes" : "no") << "\n" << Qt::flush;
    }
    return totalIssues > 0 ? 1 : 0;
}

int Cli::cmdStats()
{
    out << QJsonDocument(m.kb.stats(m.cfg.classes())).toJson(QJsonDocument::Indented) << Qt::flush;
    return 0;
}

int Cli::cmdDecisions(const QStringList &a)
{
    int n = 20;
    if (!a.isEmpty()) n = a.first().toInt();
    out << QJsonDocument(m.kb.recentDecisions(n)).toJson(QJsonDocument::Indented) << Qt::flush;
    return 0;
}

int Cli::cmdForget(const QStringList &a)
{
    if (a.isEmpty()) { err << "forget: missing <fingerprint>\n"; return 2; }
    const bool ok = m.kb.forgetFingerprint(a.first());
    if (ok) m.kb.save();
    out << (ok ? "forgotten\n" : "not found\n") << Qt::flush;
    return ok ? 0 : 1;
}

int Cli::cmdExport(const QStringList &a)
{
    if (a.isEmpty()) { err << "export: missing <path>\n"; return 2; }
    QString e;
    const bool ok = m.kb.exportJson(a.first(), &e);
    if (!ok) err << e << "\n";
    else out << "exported to " << a.first() << "\n";
    out << Qt::flush;
    return ok ? 0 : 1;
}

int Cli::cmdConfig(const QStringList &a)
{
    if (a.isEmpty()) { err << "config: need init|get|set|path\n"; return 2; }
    const QString sub = a.first();
    if (sub == QStringLiteral("path")) { out << m.configPath << "\n"; return 0; }
    if (sub == QStringLiteral("init")) {
        QString e; bool ok = Config::writeDefaults(m.configPath, &e);
        if (!ok) { err << e << "\n"; return 1; }
        out << "wrote " << m.configPath << "\n"; return 0;
    }
    if (sub == QStringLiteral("get")) {
        if (a.size() < 2) { err << "config get: missing key\n"; return 2; }
        out << QJsonDocument(QJsonObject{{a.at(1), QJsonValue::fromVariant(m.cfg.get(a.at(1)))}}).toJson(QJsonDocument::Compact) << "\n";
        return 0;
    }
    if (sub == QStringLiteral("set")) {
        if (a.size() < 3) { err << "config set: missing key value\n"; return 2; }
        // Simple typed parse: bool/int/double/json/string.
        const QString key = a.at(1);
        const QString val = a.mid(2).join(QLatin1Char(' '));
        QVariant v;
        if (val == QStringLiteral("true")) v = true;
        else if (val == QStringLiteral("false")) v = false;
        else { bool ok=false; int i=val.toInt(&ok); if(ok) v=i; else { double d=val.toDouble(&ok); if(ok && val.contains('.')) v=d; else v=val; } }
        m.cfg.set(key, v);
        QString e; if (!m.cfg.save(m.configPath, &e)) { err << e << "\n"; return 1; }
        out << "ok\n"; return 0;
    }
    err << "unknown config subcommand\n"; return 2;
}

} // namespace iso
