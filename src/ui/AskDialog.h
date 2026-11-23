#pragma once

#include "core/Types.h"
#include "core/Config.h"

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

namespace iso {

class AskDialog : public QDialog
{
    Q_OBJECT
public:
    AskDialog(const ProbeResult &probe, const Decision &decision, const Config &cfg, QWidget *parent = nullptr);

    QString chosenClassId() const { return m_chosen; }
    bool rememberFile() const { return m_rememberFile->isChecked(); }
    bool rememberSignature() const { return m_rememberSig->isChecked(); }

private slots:
    void onAccept();
    void onCombo(int idx);

private:
    const ProbeResult &m_probe;
    const Decision &m_decision;
    const Config &m_cfg;
    QString m_chosen;

    QComboBox *m_combo = nullptr;
    QCheckBox *m_rememberFile = nullptr;
    QCheckBox *m_rememberSig = nullptr;
};

} // namespace iso
