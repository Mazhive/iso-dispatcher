#pragma once

#include "core/Config.h"
#include "core/Knowledge.h"
#include "core/Classifier.h"
#include "core/IsoProbe.h"
#include "core/Launcher.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

namespace iso {

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(Config &cfg, Knowledge &kb, IsoProbe &probe, Classifier &classifier, Launcher &launcher, const QString &configPath, QWidget *parent = nullptr);
    void openIso(const QString &path);

private slots:
    void onBrowse();
    void onScan();
    void onLaunch();
    void onRemember();
    void onEditConfig();
    void onAbout();

private:
    void setDecision(const ProbeResult &pr, const Decision &d);

    Config &m_cfg;
    Knowledge &m_kb;
    IsoProbe &m_probe;
    Classifier &m_classifier;
    Launcher &m_launcher;
    QString m_configPath;

    QLineEdit *m_path = nullptr;
    QTextEdit *m_evidence = nullptr;
    QTableWidget *m_scores = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_launchBtn = nullptr;
    QPushButton *m_rememberBtn = nullptr;

    ProbeResult m_lastProbe;
    Decision m_lastDecision;
};

} // namespace iso
