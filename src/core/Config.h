#pragma once

#include "Types.h"

#include <QJsonObject>
#include <QVariant>
#include <QString>

namespace iso {

class Config
{
public:
    static QString defaultPath();               // $HOME/.config/iso-dispatcher.json

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    static bool writeDefaults(const QString &path, QString *error = nullptr);

    QJsonObject raw() const { return m_root; }
    QVariant get(const QString &dotPath, const QVariant &fallback = QVariant()) const;
    bool set(const QString &dotPath, const QVariant &value);

    // scan
    int fastBytes() const;
    int windowBytes() const;
    int pvdRecordOffset() const;
    int rootDirBytes() const;
    int maxDirs() const;
    bool useFileProbe() const;

    // ask
    bool askEnabled() const;
    double askMinScore() const;
    double tieMargin() const;
    QString headless() const;
    bool showEvidence() const;
    bool rememberFile() const;
    bool rememberSignature() const;
    int negativeCacheHours() const;

    // learn
    bool learnEnabled() const;
    QString knowledgePath() const;
    double alpha() const;
    double halfLifeDays() const;
    double minVotes() const;
    double autoLaunchWeight() const;
    double overrideWeight() const;
    double maxDbShare() const;

    // fallback / classes
    QString fallbackAction() const;
    QStringList fallbackElse() const;
    QList<ClassDef> classes() const;
    QString labelFor(const QString &id) const;

private:
    QJsonObject m_root;
};

} // namespace iso
