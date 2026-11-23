#include "App.h"
#include "cli/Cli.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <iostream>

namespace iso {

bool App::ensureConfig(const QString &path, Config &cfg)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        QString err;
        if (!Config::writeDefaults(path, &err)) {
            std::cerr << "Cannot write default config: " << err.toLocal8Bit().constData() << "\n";
            return false;
        }
        std::cerr << "Created default config: " << path.toLocal8Bit().constData() << "\n";
    }
    QString err;
    if (!cfg.load(path, &err)) {
        std::cerr << "Config error: " << err.toLocal8Bit().constData() << "\n";
        return false;
    }
    return true;
}

static bool wantsGui(const QStringList &args)
{
    if (args.size() <= 1) return true;
    const QString cmd = args.at(1).trimmed().toLower();
    if (cmd == QStringLiteral("gui")) return true;
    // known CLI commands
    static const QStringList cli = {QStringLiteral("classify"), QStringLiteral("why"), QStringLiteral("launch"),
                                    QStringLiteral("learn"), QStringLiteral("stats"), QStringLiteral("decisions"),
                                    QStringLiteral("forget"), QStringLiteral("export"), QStringLiteral("config"),
                                    QStringLiteral("--help"), QStringLiteral("-h"), QStringLiteral("?"), QStringLiteral("help")};
    if (cli.contains(cmd)) return false;
    // A file path argument is treated as CLI launch by default (can be changed); but also convenient in GUI?
    // We treat it as CLI for automation; use `gui <file>` for GUI.
    return false;
}

int App::run(int argc, char **argv)
{
    QStringList args;
    for (int i = 0; i < argc; ++i) args << QString::fromLocal8Bit(argv[i]);

    const QString configPath = Config::defaultPath();
    Config cfg;
    // For CLI help/config init before config exists we still need to proceed carefully.
    const bool gui = wantsGui(args);

    if (gui) {
        QApplication qapp(argc, argv);
        QApplication::setApplicationName(QStringLiteral("iso-dispatcher"));
        QApplication::setOrganizationName(QStringLiteral("iso-dispatcher"));

        if (!ensureConfig(configPath, cfg)) return 2;
        Knowledge kb(cfg);
        QString kerr; kb.load(&kerr); // non-fatal if missing
        IsoProbe probe(cfg);
        Classifier classifier(cfg, kb);
        Launcher launcher(cfg);

        MainWindow w(cfg, kb, probe, classifier, launcher, configPath);
        w.show();
        if (args.size() > 2) w.openIso(args.at(2));
        const int rc = qapp.exec();
        kb.save();
        return rc;
    }

    QCoreApplication qapp(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("iso-dispatcher"));

    // config init should work even if config missing
    if (args.size() >= 3 && args.at(1) == QStringLiteral("config") && args.at(2) == QStringLiteral("init")) {
        QString e; bool ok = Config::writeDefaults(configPath, &e);
        std::cout << (ok ? "wrote " : "error ") << (ok ? configPath.toLocal8Bit().constData() : e.toLocal8Bit().constData()) << "\n";
        return ok ? 0 : 1;
    }
    if (args.size() >= 2) {
        const QString cmd = args.at(1);
        if (cmd == QStringLiteral("--help") || cmd == QStringLiteral("-h") || cmd == QStringLiteral("?") || cmd == QStringLiteral("help")) {
            std::cout << Cli::helpText().toLocal8Bit().constData();
            return 0;
        }
    }

    if (!ensureConfig(configPath, cfg)) return 2;
    Knowledge kb(cfg);
    QString kerr; if (!kb.load(&kerr) && !kerr.isEmpty()) std::cerr << "knowledge: " << kerr.toLocal8Bit().constData() << "\n";
    IsoProbe probe(cfg);
    Classifier classifier(cfg, kb);
    Launcher launcher(cfg);

    Cli cli({cfg, kb, probe, classifier, launcher, configPath});
    const int rc = cli.run(args);
    if (kb.dirty()) kb.save();
    return rc < 0 ? 0 : rc;
}

} // namespace iso
