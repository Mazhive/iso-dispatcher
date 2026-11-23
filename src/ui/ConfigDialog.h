#pragma once

#include "core/Config.h"

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>

namespace iso {

class ConfigDialog : public QDialog
{
    Q_OBJECT
public:
    ConfigDialog(Config &cfg, const QString &configPath, QWidget *parent = nullptr);

private slots:
    void onSave();
    void onReload();
    void onAddRow();
    void onRemoveRow();
    void onBrowseExec();

private:
    void loadFromConfig();
    void saveToConfig();

    Config &m_cfg;
    QString m_configPath;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_fallbackExec = nullptr;
    QLineEdit *m_fallbackArgs = nullptr;
};

} // namespace iso
