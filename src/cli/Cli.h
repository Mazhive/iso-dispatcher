#pragma once

#include "core/Config.h"
#include "core/Knowledge.h"
#include "core/Classifier.h"
#include "core/IsoProbe.h"
#include "core/Launcher.h"

#include <QStringList>

namespace iso {

class Cli
{
public:
    struct Context {
        Config &cfg;
        Knowledge &kb;
        IsoProbe &probe;
        Classifier &classifier;
        Launcher &launcher;
        QString configPath;
    };

    explicit Cli(Context ctx);

    // Returns exit code. Returns -1 if GUI should be started.
    int run(const QStringList &args);

    static QString helpText();

private:
    int cmdClassify(const QStringList &a);
    int cmdWhy(const QStringList &a);
    int cmdLaunch(const QStringList &a);
    int cmdLearn(const QStringList &a);
    int cmdStats();
    int cmdDecisions(const QStringList &a);
    int cmdForget(const QStringList &a);
    int cmdExport(const QStringList &a);
    int cmdConfig(const QStringList &a);

    Context m;
};

} // namespace iso
