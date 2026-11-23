#include "AskDialog.h"

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QFormLayout>
#include <QPushButton>

namespace iso {

AskDialog::AskDialog(const ProbeResult &probe, const Decision &decision, const Config &cfg, QWidget *parent)
    : QDialog(parent), m_probe(probe), m_decision(decision), m_cfg(cfg)
{
    setWindowTitle(QStringLiteral("ISO not confidently recognized"));
    resize(600, 420);

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("File:"), new QLabel(probe.path, this));
    form->addRow(QStringLiteral("Size (bytes):"), new QLabel(QString::number(probe.size), this));
    form->addRow(QStringLiteral("Fingerprint:"), new QLabel(probe.fingerprint.left(16) + QStringLiteral("…"), this));
    root->addLayout(form);

    auto *ev = new QTextEdit(this);
    ev->setReadOnly(true);
    QStringList featTexts;
    for (const auto &f : probe.features) featTexts << f.text();
    const QString evidence = QStringLiteral("magic: %1\npvd.system_id: %2\npvd.volume_id: %3\nroot: %4\nfeatures: %5")
        .arg(probe.magicDesc, probe.systemId, probe.volumeId,
             probe.rootNames.mid(0, 15).join(QStringLiteral(", ")),
             featTexts.join(QStringLiteral(", ")));
    ev->setPlainText(evidence);
    root->addWidget(new QLabel(QStringLiteral("Evidence (content-only):"), this));
    root->addWidget(ev, 1);

    m_combo = new QComboBox(this);
    const auto classes = cfg.classes();
    for (int i = 0; i < classes.size(); ++i) {
        const auto &c = classes[i];
        m_combo->addItem(c.label + QStringLiteral("  [") + c.id + QStringLiteral("]"), c.id);
    }
    // set default to best guess
    int def = 0;
    for (int i = 0; i < classes.size(); ++i) if (classes[i].id == decision.classId) { def = i; break; }
    m_combo->setCurrentIndex(def);
    m_chosen = decision.classId;

    root->addWidget(new QLabel(QStringLiteral("Start with:"), this));
    root->addWidget(m_combo);

    m_rememberFile = new QCheckBox(QStringLiteral("Remember for this file (fingerprint)"), this);
    m_rememberFile->setChecked(cfg.rememberFile());
    root->addWidget(m_rememberFile);
    m_rememberSig = new QCheckBox(QStringLiteral("Remember this content signature (strong features)"), this);
    m_rememberSig->setChecked(cfg.rememberSignature());
    root->addWidget(m_rememberSig);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btns->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Start"));
    connect(btns, &QDialogButtonBox::accepted, this, &AskDialog::onAccept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btns);

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AskDialog::onCombo);
}

void AskDialog::onCombo(int idx)
{
    m_chosen = m_combo->itemData(idx).toString();
}

void AskDialog::onAccept()
{
    m_chosen = m_combo->currentData().toString();
    accept();
}

} // namespace iso
