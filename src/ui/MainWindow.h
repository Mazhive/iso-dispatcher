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
#include <QAction>
#include <QMenu>

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
    void onLanguageChanged(QAction *action);

private:
    void setDecision(const ProbeResult &pr, const Decision &d);
    void retranslateUi();
    QString trUi(const QString &en, const QString &nl) const;

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
    QLabel *m_evidenceLabel = nullptr;
    QLabel *m_scoresLabel = nullptr;
    QPushButton *m_launchBtn = nullptr;
    QPushButton *m_rememberBtn = nullptr;
    QPushButton *m_scanBtn = nullptr;

    QMenu *m_fileMenu = nullptr;
    QMenu *m_helpMenu = nullptr;
    QMenu *m_langMenu = nullptr;
    QAction *m_openAct = nullptr;
    QAction *m_cfgAct = nullptr;
    QAction *m_quitAct = nullptr;
    QAction *m_aboutAct = nullptr;
    QAction *m_langEnAct = nullptr;
    QAction *m_langNlAct = nullptr;

    ProbeResult m_lastProbe;
    Decision m_lastDecision;
};

} // namespace iso
