#include "Knowledge.h"
#include "Util.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <cmath>

namespace iso {

Knowledge::Knowledge(const Config &cfg) : m_cfg(cfg), m_path(cfg.knowledgePath()) {}

bool Knowledge::load(QString *error)
{
    m_fpClass.clear(); m_negCache.clear(); m_weights.clear(); m_decisions = QJsonArray(); m_dirty = false;
    QFile f(m_path);
    if (!f.exists()) return true;
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = f.errorString(); return false; }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) { if (error) *error = pe.errorString(); return false; }
    const QJsonObject root = doc.object();

    const QJsonObject fp = root.value(QStringLiteral("fingerprints")).toObject();
    for (auto it = fp.begin(); it != fp.end(); ++it) m_fpClass.insert(it.key(), it.value().toString());

    const QJsonObject neg = root.value(QStringLiteral("negative_cache")).toObject();
    for (auto it = neg.begin(); it != neg.end(); ++it) m_negCache.insert(it.key(), qint64(it.value().toDouble()));

    const QJsonObject w = root.value(QStringLiteral("weights")).toObject();
    for (auto fit = w.begin(); fit != w.end(); ++fit) {
        const QJsonObject byClass = fit.value().toObject();
        QHash<QString, Weight> row;
        for (auto cit = byClass.begin(); cit != byClass.end(); ++cit) {
            const QJsonObject wo = cit.value().toObject();
            Weight ww;
            ww.pos = wo.value(QStringLiteral("pos")).toDouble();
            ww.neg = wo.value(QStringLiteral("neg")).toDouble();
            ww.updated = wo.value(QStringLiteral("updated")).toString();
            row.insert(cit.key(), ww);
        }
        m_weights.insert(fit.key(), row);
    }

    m_decisions = root.value(QStringLiteral("decisions")).toArray();
    return true;
}

bool Knowledge::save(QString *error) const
{
    QFileInfo fi(m_path);
    QDir().mkpath(fi.absolutePath());
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("saved_at"), util::stamp());

    QJsonObject fp;
    for (auto it = m_fpClass.begin(); it != m_fpClass.end(); ++it) fp.insert(it.key(), it.value());
    root.insert(QStringLiteral("fingerprints"), fp);

    QJsonObject neg;
    for (auto it = m_negCache.begin(); it != m_negCache.end(); ++it) neg.insert(it.key(), double(it.value()));
    root.insert(QStringLiteral("negative_cache"), neg);

    QJsonObject w;
    for (auto fit = m_weights.begin(); fit != m_weights.end(); ++fit) {
        QJsonObject byClass;
        for (auto cit = fit.value().begin(); cit != fit.value().end(); ++cit) {
            QJsonObject wo;
            wo.insert(QStringLiteral("pos"), cit.value().pos);
            wo.insert(QStringLiteral("neg"), cit.value().neg);
            wo.insert(QStringLiteral("updated"), cit.value().updated);
            byClass.insert(cit.key(), wo);
        }
        w.insert(fit.key(), byClass);
    }
    root.insert(QStringLiteral("weights"), w);
    root.insert(QStringLiteral("decisions"), m_decisions);

    const QString tmp = m_path + QStringLiteral(".tmp");
    QFile t(tmp);
    if (!t.open(QIODevice::WriteOnly | QIODevice::Truncate)) { if (error) *error = t.errorString(); return false; }
    t.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    t.close();
    QFile::remove(m_path);
    if (!QFile::rename(tmp, m_path)) { if (error) *error = QStringLiteral("rename failed"); return false; }
    return true;
}

double Knowledge::weightFor(const QString &feature, const QString &classId) const
{
    auto fit = m_weights.find(feature);
    if (fit == m_weights.end()) return 0.0;
    auto cit = fit->find(classId);
    if (cit == fit->end()) return 0.0;
    const double alpha = m_cfg.alpha();
    const double pos = cit->pos;
    const double neg = cit->neg;
    const double votes = pos + neg;
    if (votes < m_cfg.minVotes()) return 0.0;
    // Laplace-smoothed log odds.
    return std::log((pos + alpha) / (neg + alpha));
}

double Knowledge::scoreContribution(const ProbeResult &probe, const QString &classId) const
{
    double sum = 0.0;
    for (const auto &f : probe.features) sum += weightFor(featureKey(f), classId);
    // Clamp via classifier too, but keep sane here.
    return sum;
}

QString Knowledge::rememberedClass(const QString &fingerprint) const
{
    return m_fpClass.value(fingerprint);
}

void Knowledge::rememberFingerprint(const QString &fingerprint, const QString &classId)
{
    if (fingerprint.isEmpty() || classId.isEmpty()) return;
    m_fpClass.insert(fingerprint, classId);
    m_dirty = true;
}

void Knowledge::rememberSignature(const ProbeResult &probe, const QString &classId)
{
    // Learn a stable content signature: pvd.system_id and strong tokens.
    for (const auto &f : probe.features) {
        if (f.key == QStringLiteral("pvd.system_id") || f.key == QStringLiteral("magic") || f.key == QStringLiteral("mime")) {
            bump(featureKey(f), classId, true, m_cfg.overrideWeight());
        }
    }
}

void Knowledge::bump(const QString &feature, const QString &classId, bool positive, double amount)
{
    auto &row = m_weights[feature];
    auto &w = row[classId];
    if (positive) w.pos += amount; else w.neg += amount;
    w.updated = util::stamp();
    m_dirty = true;
}

void Knowledge::recordDecision(const ProbeResult &probe, const QString &chosen, bool prompted, bool overridden)
{
    if (!m_cfg.learnEnabled() || chosen.isEmpty()) return;
    const double amt = overridden ? m_cfg.overrideWeight() : (prompted ? 1.0 : m_cfg.autoLaunchWeight());
    if (amt <= 0.0 && !prompted) return; // avoid self-fulfilling auto loop by default

    const QSet<QString> present;
    QSet<QString> featSet;
    for (const auto &f : probe.features) featSet.insert(featureKey(f));

    // For all known classes, nudge features: positive for chosen, negative for others only on explicit prompt/override.
    // We learn from the selected class; negative evidence is only applied when user made an explicit choice.
    for (const auto &f : probe.features) {
        const QString fk = featureKey(f);
        bump(fk, chosen, true, amt);
        if (prompted || overridden) {
            // Mild negative for other prominent classes is handled by the classifier via relative weights;
            // we keep it simple: don't invent negatives for all classes, only record chosen.
        }
    }

    QJsonObject d;
    d.insert(QStringLiteral("at"), util::stamp());
    d.insert(QStringLiteral("fingerprint"), probe.fingerprint);
    d.insert(QStringLiteral("file"), QFileInfo(probe.path).fileName());
    d.insert(QStringLiteral("chosen"), chosen);
    d.insert(QStringLiteral("prompted"), prompted);
    d.insert(QStringLiteral("overridden"), overridden);
    d.insert(QStringLiteral("features"), QJsonArray::fromStringList(QStringList(featSet.begin(), featSet.end())));
    m_decisions.append(d);
    while (m_decisions.size() > 500) m_decisions.removeFirst();
    m_dirty = true;
}

bool Knowledge::isNegativelyCached(const QString &fingerprint) const
{
    auto it = m_negCache.find(fingerprint);
    if (it == m_negCache.end()) return false;
    const qint64 age = QDateTime::currentSecsSinceEpoch() - it.value();
    return age < qint64(m_cfg.negativeCacheHours()) * 3600;
}

void Knowledge::negativeCache(const QString &fingerprint)
{
    if (fingerprint.isEmpty()) return;
    m_negCache.insert(fingerprint, QDateTime::currentSecsSinceEpoch());
    m_dirty = true;
}

QJsonObject Knowledge::stats(const QList<ClassDef> &classes) const
{
    QJsonObject out;
    out.insert(QStringLiteral("fingerprints"), m_fpClass.size());
    out.insert(QStringLiteral("weight_features"), m_weights.size());
    out.insert(QStringLiteral("decisions"), m_decisions.size());
    QJsonObject perClass;
    for (const auto &c : classes) {
        int feats = 0; double sum = 0;
        for (auto fit = m_weights.begin(); fit != m_weights.end(); ++fit) {
            auto cit = fit->find(c.id);
            if (cit == fit->end()) continue;
            if ((cit->pos + cit->neg) < m_cfg.minVotes()) continue;
            feats++; sum += std::log((cit->pos + m_cfg.alpha()) / (cit->neg + m_cfg.alpha()));
        }
        QJsonObject co; co.insert(QStringLiteral("features"), feats); co.insert(QStringLiteral("weight_sum"), sum);
        perClass.insert(c.id, co);
    }
    out.insert(QStringLiteral("classes"), perClass);
    return out;
}

QJsonArray Knowledge::recentDecisions(int n) const
{
    QJsonArray out;
    const int start = std::max(0, int(m_decisions.size()) - n);
    for (int i = start; i < m_decisions.size(); ++i) out.append(m_decisions.at(i));
    return out;
}

bool Knowledge::forgetFingerprint(const QString &fingerprint)
{
    const bool removed = m_fpClass.remove(fingerprint) > 0 || m_negCache.remove(fingerprint) > 0;
    if (removed) m_dirty = true;
    return removed;
}

bool Knowledge::exportJson(const QString &path, QString *error) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { if (error) *error = f.errorString(); return false; }
    QJsonObject root;
    root.insert(QStringLiteral("fingerprints"), QJsonObject::fromVariantHash([&](){ QVariantHash h; for(auto it=m_fpClass.begin();it!=m_fpClass.end();++it) h.insert(it.key(),it.value()); return h; }()));
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace iso
