#include "MainWindow.h"
#include "AskDialog.h"
#include "ConfigDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

namespace iso {

MainWindow::MainWindow(Config &cfg, Knowledge &kb, IsoProbe &probe, Classifier &classifier, Launcher &launcher, const QString &configPath, QWidget *parent)
    : QMainWindow(parent), m_cfg(cfg), m_kb(kb), m_probe(probe), m_classifier(classifier), m_launcher(launcher), m_configPath(configPath)
{
    setWindowTitle(QStringLiteral("iso-dispatcher"));
    resize(980, 700);

    auto *cw = new QWidget(this);
    auto *root = new QVBoxLayout(cw);

    m_fileMenu = menuBar()->addMenu(QString());
    m_openAct = m_fileMenu->addAction(QString());
    m_openAct->setShortcut(QKeySequence::Open);
    m_fileMenu->addSeparator();
    m_cfgAct = m_fileMenu->addAction(QString());
    m_fileMenu->addSeparator();
    m_quitAct = m_fileMenu->addAction(QString());
    m_quitAct->setShortcut(QKeySequence::Quit);

    m_langMenu = menuBar()->addMenu(QString());
    m_langEnAct = m_langMenu->addAction(QStringLiteral("English"));
    m_langEnAct->setCheckable(true);
    m_langNlAct = m_langMenu->addAction(QStringLiteral("Nederlands"));
    m_langNlAct->setCheckable(true);
    m_langEnAct->setChecked(m_cfg.language() != QStringLiteral("nl"));
    m_langNlAct->setChecked(m_cfg.language() == QStringLiteral("nl"));

    m_helpMenu = menuBar()->addMenu(QString());
    m_aboutAct = m_helpMenu->addAction(QString());

    auto *row = new QHBoxLayout();
    m_path = new QLineEdit(this);
    m_scanBtn = new QPushButton(this);
    row->addWidget(m_path, 1);
    row->addWidget(m_scanBtn);
    root->addLayout(row);

    m_evidence = new QTextEdit(this);
    m_evidence->setReadOnly(true);
    m_evidence->setMinimumHeight(180);
    m_evidenceLabel = new QLabel(this);
    root->addWidget(m_evidenceLabel);
    root->addWidget(m_evidence);

    m_scores = new QTableWidget(this);
    m_scores->setColumnCount(6);
    m_scores->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_scores->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_scores->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_scoresLabel = new QLabel(this);
    root->addWidget(m_scoresLabel);
    root->addWidget(m_scores, 1);

    auto *actions = new QHBoxLayout();
    m_launchBtn = new QPushButton(this);
    m_rememberBtn = new QPushButton(this);
    actions->addWidget(m_launchBtn);
    actions->addWidget(m_rememberBtn);
    actions->addStretch(1);
    root->addLayout(actions);

    m_status = new QLabel(this);
    root->addWidget(m_status);

    setCentralWidget(cw);

    connect(m_openAct, &QAction::triggered, this, &MainWindow::onBrowse);
    connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onScan);
    connect(m_launchBtn, &QPushButton::clicked, this, &MainWindow::onLaunch);
    connect(m_rememberBtn, &QPushButton::clicked, this, &MainWindow::onRemember);
    connect(m_cfgAct, &QAction::triggered, this, &MainWindow::onEditConfig);
    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
    connect(m_quitAct, &QAction::triggered, this, &QWidget::close);
    connect(m_langMenu, &QMenu::triggered, this, &MainWindow::onLanguageChanged);

    retranslateUi();
}

QString MainWindow::trUi(const QString &en, const QString &nl) const
{
    return m_cfg.language() == QStringLiteral("nl") ? nl : en;
}

void MainWindow::retranslateUi()
{
    m_fileMenu->setTitle(trUi(QStringLiteral("&File"), QStringLiteral("&Bestand")));
    m_openAct->setText(trUi(QStringLiteral("&Open..."), QStringLiteral("&Openen...")));
    m_cfgAct->setText(trUi(QStringLiteral("Edit &Config..."), QStringLiteral("Config &bewerken...")));
    m_quitAct->setText(trUi(QStringLiteral("&Quit"), QStringLiteral("&Afsluiten")));

    m_langMenu->setTitle(trUi(QStringLiteral("&Language"), QStringLiteral("&Taal")));
    m_langEnAct->setChecked(m_cfg.language() != QStringLiteral("nl"));
    m_langNlAct->setChecked(m_cfg.language() == QStringLiteral("nl"));

    m_helpMenu->setTitle(trUi(QStringLiteral("&Help"), QStringLiteral("&Help")));
    m_aboutAct->setText(trUi(QStringLiteral("&About iso-dispatcher"), QStringLiteral("&Over iso-dispatcher")));

    m_path->setPlaceholderText(trUi(
        QStringLiteral("Path to .iso file (content-only classification; no folder rules)"),
        QStringLiteral("Pad naar .iso bestand (alleen inhoudsclassificatie; geen mapregels)")));
    m_scanBtn->setText(trUi(QStringLiteral("Scan"), QStringLiteral("Scannen")));
    m_evidenceLabel->setText(trUi(QStringLiteral("Evidence"), QStringLiteral("Bewijs")));
    m_scoresLabel->setText(trUi(QStringLiteral("Scores"), QStringLiteral("Scores")));
    m_scores->setHorizontalHeaderLabels({
        trUi(QStringLiteral("Class"), QStringLiteral("Klasse")),
        trUi(QStringLiteral("Base"), QStringLiteral("Basis")),
        QStringLiteral("DB"),
        trUi(QStringLiteral("Total"), QStringLiteral("Totaal")),
        trUi(QStringLiteral("Priority"), QStringLiteral("Prioriteit")),
        trUi(QStringLiteral("Reasons"), QStringLiteral("Redenen"))
    });
    m_launchBtn->setText(trUi(QStringLiteral("Launch selected"), QStringLiteral("Start geselecteerde")));
    m_rememberBtn->setText(trUi(QStringLiteral("Remember selected for this file"), QStringLiteral("Onthoud selectie voor dit bestand")));
    m_status->setText(trUi(QStringLiteral("Ready. Config: "), QStringLiteral("Klaar. Config: ")) + m_configPath);
}

void MainWindow::onLanguageChanged(QAction *action)
{
    const QString lang = (action == m_langNlAct) ? QStringLiteral("nl") : QStringLiteral("en");
    m_cfg.set(QStringLiteral("language"), lang);
    QString err;
    if (!m_cfg.save(Config::defaultPath(), &err)) {
        QMessageBox::warning(this, trUi(QStringLiteral("Language"), QStringLiteral("Taal")),
                             trUi(QStringLiteral("Could not save language setting: "), QStringLiteral("Kon taalinstelling niet opslaan: ")) + err);
    }
    retranslateUi();
}

void MainWindow::openIso(const QString &path)
{
    m_path->setText(path);
    onScan();
}

void MainWindow::onBrowse()
{
    const QString title = trUi(QStringLiteral("Select disc image"), QStringLiteral("Selecteer schijfimage"));
    const QString f = QFileDialog::getOpenFileName(this, title, QString(), QStringLiteral("Disc images (*.iso *.ISO *.cue *.CUE *.ccd *.CCD *.mds *.MDS *.img *.IMG *.mdf *.MDF *.bin *.BIN *.chd *.CHD *.pbp *.PBP *.ps3boot.iso *.ps4boot.iso *.ps2boot.iso);;All files (*)"));
    if (!f.isEmpty()) { m_path->setText(f); onScan(); }
}

void MainWindow::setDecision(const ProbeResult &pr, const Decision &d)
{
    m_lastProbe = pr;
    m_lastDecision = d;

    QStringList featTexts; for (const auto &f : pr.features) featTexts << f.text();
    QString warnBlock;
    if (!pr.warnings.isEmpty()) warnBlock = QStringLiteral("\n\n") + trUi(QStringLiteral("WARNING:"), QStringLiteral("WAARSCHUWING:")) + QStringLiteral("\n- ") + pr.warnings.join(QStringLiteral("\n- "));
    m_evidence->setPlainText(QStringLiteral(
        "file: %1\nsize: %2\nfingerprint: %3\nmagic: %4\npvd.system_id: %5\npvd.volume_id: %6\nroot: %7\nfeatures: %8\n\ndecision: %9 score=%10 conf=%11 ask=%12%13")
        .arg(pr.path)
        .arg(pr.size)
        .arg(pr.fingerprint, pr.magicDesc, pr.systemId, pr.volumeId,
             pr.rootNames.mid(0, 20).join(QStringLiteral(", ")),
             featTexts.join(QStringLiteral(", ")),
             d.classId)
        .arg(d.score, 0, 'f', 1)
        .arg(d.confidence, 0, 'f', 3)
        .arg(d.ask ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(warnBlock));

    m_scores->setRowCount(d.all.size());
    for (int i = 0; i < d.all.size(); ++i) {
        const auto &b = d.all[i];
        m_scores->setItem(i, 0, new QTableWidgetItem(b.classId));
        m_scores->setItem(i, 1, new QTableWidgetItem(QString::number(b.base, 'f', 1)));
        m_scores->setItem(i, 2, new QTableWidgetItem(QString::number(b.learned, 'f', 2)));
        m_scores->setItem(i, 3, new QTableWidgetItem(QString::number(b.total, 'f', 1)));
        m_scores->setItem(i, 4, new QTableWidgetItem(QString::number(b.priority)));
        m_scores->setItem(i, 5, new QTableWidgetItem(b.reasons.join(QStringLiteral(" ; "))));
    }
    if (m_scores->rowCount() > 0) m_scores->selectRow(0);
    m_status->setText(d.ask ? trUi(QStringLiteral("Low confidence - verify before launch."), QStringLiteral("Lage zekerheid - controleer voor starten."))
                            : trUi(QStringLiteral("Confident decision."), QStringLiteral("Zekere beslissing.")));
}

void MainWindow::onScan()
{
    const QString path = m_path->text().trimmed();
    if (path.isEmpty()) return;
    const ProbeResult pr = m_probe.probe(path);
    if (!pr.error.isEmpty()) { QMessageBox::warning(this, trUi(QStringLiteral("Scan error"), QStringLiteral("Scanfout")), pr.error); return; }
    const Decision d = m_classifier.classify(pr);
    setDecision(pr, d);
}

void MainWindow::onLaunch()
{
    if (m_lastProbe.path.isEmpty()) { onScan(); if (m_lastProbe.path.isEmpty()) return; }
    QString chosen = m_lastDecision.classId;
    int row = m_scores->currentRow();
    if (row >= 0 && row < m_lastDecision.all.size()) chosen = m_lastDecision.all[row].classId;

    bool overridden = chosen != m_lastDecision.classId;
    if (m_lastDecision.ask || overridden) {
        AskDialog dlg(m_lastProbe, m_lastDecision, m_cfg, this);
        if (dlg.exec() != QDialog::Accepted) {
            m_kb.negativeCache(m_lastProbe.fingerprint);
            m_kb.save();
            m_status->setText(trUi(QStringLiteral("Cancelled; negative cache set."), QStringLiteral("Geannuleerd; negatieve cache gezet.")));
            return;
        }
        chosen = dlg.chosenClassId();
        overridden = chosen != m_lastDecision.classId;
        if (dlg.rememberFile()) m_kb.rememberFingerprint(m_lastProbe.fingerprint, chosen);
        if (dlg.rememberSignature()) m_kb.rememberSignature(m_lastProbe, chosen);
    }

    m_kb.recordDecision(m_lastProbe, chosen, m_lastDecision.ask, overridden);
    auto lr = m_launcher.launchClass(chosen, m_lastProbe.launchPath());
    if (!lr.ok) {
        const auto fb = m_cfg.fallbackElse();
        if (!fb.isEmpty()) lr = m_launcher.launchCommand(fb.first(), fb.mid(1), m_lastProbe.launchPath());
    }
    m_kb.save();
    m_status->setText(lr.message);
    if (!lr.ok) QMessageBox::warning(this, trUi(QStringLiteral("Launch failed"), QStringLiteral("Starten mislukt")), lr.message);
}

void MainWindow::onRemember()
{
    if (m_lastProbe.fingerprint.isEmpty()) return;
    int row = m_scores->currentRow();
    QString chosen = m_lastDecision.classId;
    if (row >= 0 && row < m_lastDecision.all.size()) chosen = m_lastDecision.all[row].classId;
    m_kb.rememberFingerprint(m_lastProbe.fingerprint, chosen);
    m_kb.recordDecision(m_lastProbe, chosen, true, chosen != m_lastDecision.classId);
    m_kb.save();
    m_status->setText(trUi(QStringLiteral("Remembered %1 for fingerprint %2").arg(chosen, m_lastProbe.fingerprint.left(12)),
                           QStringLiteral("Onthouden: %1 voor fingerprint %2").arg(chosen, m_lastProbe.fingerprint.left(12))));
}

void MainWindow::onEditConfig()
{
    ConfigDialog dlg(m_cfg, m_configPath, this);
    dlg.exec();
    m_status->setText(trUi(QStringLiteral("Config reloaded after editing. Current config: "), QStringLiteral("Config herladen na bewerken. Huidige config: ")) + m_configPath);
}

void MainWindow::onAbout()
{
    const QString text = trUi(
        QStringLiteral(
            "<h3>iso-dispatcher</h3>"
            "<p>Content-based ISO classifier/launcher.</p>"
            "<p><b>The following emulators/applications are required for iso-dispatcher to function:</b></p>"
            "<ul>"
            "<li><b>PS1 / PSX</b>: DuckStation or RetroArch (PSX core)</li>"
            "<li><b>PS2</b>: PCSX2 or another PS2 emulator</li>"
            "<li><b>PS3</b>: RPCS3</li>"
            "<li><b>PS4</b>: shadPS4 (shadPS4QtLauncher)</li>"
            "<li><b>PSP</b>: PPSSPP</li>"
            "<li><b>Wii / GameCube</b>: Dolphin</li>"
            "<li><b>Xbox</b>: xemu or another Xbox emulator</li>"
            "<li><b>Amiga</b>: FS-UAE / UAE / RetroArch Amiga core</li>"
            "<li><b>PC / DOS</b>: DOSBox, Wine or native application (depending on image)</li>"
            "</ul>"
            "<p>Configure exact paths/arguments via <i>Edit Config</i> or in <code>~/.config/iso-dispatcher.json</code>.</p>"),
        QStringLiteral(
            "<h3>iso-dispatcher</h3>"
            "<p>Inhoudsgebaseerde ISO-classificeerder/launcher.</p>"
            "<p><b>De volgende emulators/applicaties zijn noodzakelijk voor iso-dispatcher om te functioneren:</b></p>"
            "<ul>"
            "<li><b>PS1 / PSX</b>: DuckStation of RetroArch (PSX core)</li>"
            "<li><b>PS2</b>: PCSX2 of andere PS2-emulator</li>"
            "<li><b>PS3</b>: RPCS3</li>"
            "<li><b>PS4</b>: shadPS4 (shadPS4QtLauncher)</li>"
            "<li><b>PSP</b>: PPSSPP</li>"
            "<li><b>Wii / GameCube</b>: Dolphin</li>"
            "<li><b>Xbox</b>: xemu of andere Xbox-emulator</li>"
            "<li><b>Amiga</b>: FS-UAE / UAE / RetroArch Amiga core</li>"
            "<li><b>PC / DOS</b>: DOSBox, Wine of native applicatie (afhankelijk van image)</li>"
            "</ul>"
            "<p>Configureer de exacte paden/argumenten via <i>Config bewerken</i> of in <code>~/.config/iso-dispatcher.json</code>.</p>"));
    QMessageBox::about(this, trUi(QStringLiteral("About iso-dispatcher"), QStringLiteral("Over iso-dispatcher")), text);
}

} // namespace iso
