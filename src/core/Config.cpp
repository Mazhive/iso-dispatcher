#include "Config.h"
#include "Util.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QVariant>

namespace iso {

static const char *kDefaultConfig = R"JSON({
  "version": 1,
  "comment": "iso-dispatcher: pure content classification. NO path/folder rules by design.",
  "scan": {
    "fast_bytes": 65536,
    "window_bytes": 2097152,
    "pvd_record_offset": 32924,
    "root_dir_bytes": 8192,
    "max_dirs": 64,
    "use_file_probe": true
  },
  "ask": {
    "enabled": true,
    "min_score": 70,
    "tie_margin": 10,
    "default": "best_guess",
    "headless": "menu",
    "show_evidence": true,
    "negative_cache_hours": 24
  },
  "learn": {
    "enabled": true,
    "db": "~/.local/share/iso-dispatcher/knowledge.json",
    "alpha": 1.0,
    "half_life_days": 180,
    "min_votes": 3,
    "auto_launch_weight": 0.0,
    "override_weight": 1.5,
    "max_db_share": 0.4,
    "target_accuracy": 0.90
  },
  "fallback": {
    "action": "ask",
    "else": ["isomounter", "%f"]
  },
  "classes": [
    {
      "id": "wii", "label": "Nintendo Wii (Dolphin)", "priority": 10,
      "exec": "dolphin-emu", "args": ["%f"],
      "rules": [ {"type":"magic","needles":["5D1C9EA3"],"offset":28,"score":100} ]
    },
    {
      "id": "xbox", "label": "Xbox (Xemu)", "priority": 10,
      "exec": "xemu", "args": ["%f"],
      "rules": [
        {"type":"raw","needles":["MICROSOFT*XBOX*MEDIA"],"window":73728,"score":100},
        {"type":"raw","needles":["DEFAULT.XBE"],"window":73728,"score":30}
      ]
    },
    {
      "id": "ps1", "label": "PlayStation 1 (DuckStation)", "priority": 9,
      "exec": "duckstation", "args": ["%f"],
      "rules": [
        {"type":"sector","needles":["2352"],"score":60},
        {"type":"token","needles":["SYSTEM.CNF"],"score":30},
        {"type":"token","needles":["BOOT ="],"score":30}
      ]
    },
    {
      "id": "ps2", "label": "PlayStation 2 (PCSX2)", "priority": 9,
      "exec": "pcsx2", "args": ["%f"],
      "rules": [
        {"type":"pvd.system_id","needles":["PLAYSTATION"],"score":100},
        {"type":"token","needles":["BOOT2 ="],"score":20}
      ]
    },
    {
      "id": "psp", "label": "PlayStation Portable (PPSSPP)", "priority": 9,
      "exec": "ppsspp", "args": ["%f"],
      "rules": [
        {"type":"pvd.system_id","needles":["PSP GAME"],"score":100},
        {"type":"root","needles":["PSP_GAME"],"score":30}
      ]
    },
    {
      "id": "ps3", "label": "PlayStation 3 (RPCS3)", "priority": 9,
      "exec": "rpcs3", "args": ["%f"],
      "rules": [
        {"type":"root","needles":["PS3_DISC.SFB"],"score":100},
        {"type":"token","needles":["PS3_DISC"],"score":60},
        {"type":"tree","needles":["EBOOT.BIN"],"score":40}
      ]
    },
    {
      "id": "amiga", "label": "Commodore Amiga (FS-UAE)", "priority": 9,
      "exec": "fs-uae", "args": ["%f"],
      "rules": [ {"type":"pvd.system_id","needles":["AMIGA"],"score":100} ]
    },
    {
      "id": "pc", "label": "PC / UDF game (mount)", "priority": 1,
      "exec": "gnome-disk-image-mounter", "args": ["%f"],
      "rules": [
        {"type":"mime","needles":["application/x-udf-image"],"score":80},
        {"type":"token","needles":["SETUP.EXE","AUTORUN.INF","INSTALL.EXE"],"score":80},
        {"type":"root","needles":["SETUP.EXE","AUTORUN.INF","INSTALL.EXE","*.CAB","*.SH"],"score":70},
        {"type":"tree","needles":["*.EXE","*.INF"],"score":50}
      ]
    }
  ]
}
)JSON";

QString Config::defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath(QStringLiteral(".config/iso-dispatcher.json"));
}

bool Config::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open config: ") + f.errorString();
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("Bad config JSON: ") + pe.errorString();
        return false;
    }
    m_root = doc.object();
    return true;
}

bool Config::save(const QString &path, QString *error) const
{
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    const QString tmp = path + QStringLiteral(".tmp");
    QFile t(tmp);
    if (!t.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = t.errorString();
        return false;
    }
    t.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    t.close();
    QFile::remove(path);
    if (!t.rename(path)) {
        if (error) *error = t.errorString();
        return false;
    }
    return true;
}

bool Config::writeDefaults(const QString &path, QString *error)
{
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(kDefaultConfig);
    return true;
}

QVariant Config::get(const QString &dotPath, const QVariant &fallback) const
{
    QJsonValue v = m_root;
    const auto parts = dotPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const auto &p : parts) {
        if (!v.isObject()) return fallback;
        v = v.toObject().value(p);
    }
    return v.isUndefined() ? fallback : v.toVariant();
}

bool Config::set(const QString &dotPath, const QVariant &value)
{
    const auto parts = dotPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;
    QJsonObject root = m_root;
    auto assign = [](QJsonObject &obj, const QStringList &keys, const QJsonValue &val, auto &&self) -> void {
        if (keys.size() == 1) { obj.insert(keys.first(), val); return; }
        QString k = keys.first();
        QJsonObject child = obj.value(k).toObject();
        self(child, keys.mid(1), val, self);
        obj.insert(k, child);
    };
    assign(root, parts, QJsonValue::fromVariant(value), assign);
    m_root = root;
    return true;
}

int Config::fastBytes() const { return get(QStringLiteral("scan/fast_bytes"), 65536).toInt(); }
int Config::windowBytes() const { return get(QStringLiteral("scan/window_bytes"), 2097152).toInt(); }
int Config::pvdRecordOffset() const { return get(QStringLiteral("scan/pvd_record_offset"), 32924).toInt(); }
int Config::rootDirBytes() const { return get(QStringLiteral("scan/root_dir_bytes"), 8192).toInt(); }
int Config::maxDirs() const { return get(QStringLiteral("scan/max_dirs"), 64).toInt(); }
bool Config::useFileProbe() const { return get(QStringLiteral("scan/use_file_probe"), true).toBool(); }

bool Config::askEnabled() const { return get(QStringLiteral("ask/enabled"), true).toBool(); }
double Config::askMinScore() const { return get(QStringLiteral("ask/min_score"), 70).toDouble(); }
double Config::tieMargin() const { return get(QStringLiteral("ask/tie_margin"), 10).toDouble(); }
QString Config::headless() const { return get(QStringLiteral("ask/headless"), QStringLiteral("menu")).toString(); }
bool Config::showEvidence() const { return get(QStringLiteral("ask/show_evidence"), true).toBool(); }
bool Config::rememberFile() const { return get(QStringLiteral("ask/remember_file"), true).toBool(); }
bool Config::rememberSignature() const { return get(QStringLiteral("ask/remember_signature"), true).toBool(); }
int Config::negativeCacheHours() const { return get(QStringLiteral("ask/negative_cache_hours"), 24).toInt(); }

bool Config::learnEnabled() const { return get(QStringLiteral("learn/enabled"), true).toBool(); }
QString Config::knowledgePath() const
{
    QString p = get(QStringLiteral("learn/db"), QStringLiteral("~/.local/share/iso-dispatcher/knowledge.json")).toString();
    if (p.startsWith(QLatin1Char('~'))) p = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).filePath(p.mid(1));
    return p;
}
double Config::alpha() const { return get(QStringLiteral("learn/alpha"), 1.0).toDouble(); }
double Config::halfLifeDays() const { return get(QStringLiteral("learn/half_life_days"), 180).toDouble(); }
double Config::minVotes() const { return get(QStringLiteral("learn/min_votes"), 3).toDouble(); }
double Config::autoLaunchWeight() const { return get(QStringLiteral("learn/auto_launch_weight"), 0.0).toDouble(); }
double Config::overrideWeight() const { return get(QStringLiteral("learn/override_weight"), 1.5).toDouble(); }
double Config::maxDbShare() const { return get(QStringLiteral("learn/max_db_share"), 0.4).toDouble(); }

QString Config::fallbackAction() const { return get(QStringLiteral("fallback/action"), QStringLiteral("ask")).toString(); }
QStringList Config::fallbackElse() const
{
    const QVariant v = get(QStringLiteral("fallback/else"));
    if (v.isValid() && v.canConvert<QStringList>()) return v.toStringList();
    return QStringList{QStringLiteral("isomounter"), QStringLiteral("%f")};
}

QList<ClassDef> Config::classes() const
{
    QList<ClassDef> out;
    const QJsonArray arr = m_root.value(QStringLiteral("classes")).toArray();
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        ClassDef c;
        c.id = o.value(QStringLiteral("id")).toString();
        c.label = o.value(QStringLiteral("label")).toString(c.id);
        c.exec = o.value(QStringLiteral("exec")).toString();
        c.priority = o.value(QStringLiteral("priority")).toInt();
        for (const auto &av : o.value(QStringLiteral("args")).toArray()) c.args << av.toString();
        for (const auto &rv : o.value(QStringLiteral("rules")).toArray()) {
            const QJsonObject ro = rv.toObject();
            Rule r;
            r.type = ro.value(QStringLiteral("type")).toString();
            r.window = ro.value(QStringLiteral("window")).toInt(0);
            r.offset = ro.value(QStringLiteral("offset")).toInt(28);
            for (const auto &nv : ro.value(QStringLiteral("needles")).toArray()) r.needles << nv.toString();
            r.score = ro.value(QStringLiteral("score")).toInt(60);
            c.rules << r;
        }
        if (!c.id.isEmpty()) out << c;
    }
    return out;
}

QString Config::labelFor(const QString &id) const
{
    for (const auto &c : classes()) if (c.id == id) return c.label;
    return id;
}

} // namespace iso
