#include "ConfigDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>

namespace iso {

ConfigDialog::ConfigDialog(Config &cfg, const QString &configPath, QWidget *parent)
    : QDialog(parent), m_cfg(cfg), m_configPath(configPath)
{
    const bool nl = m_cfg.language() == QStringLiteral("nl");
    setWindowTitle(nl ? QStringLiteral("Config bewerken - iso-dispatcher") : QStringLiteral("Edit Config - iso-dispatcher"));
    resize(980, 560);

    auto *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(QStringLiteral("Config: ") + m_configPath, this));
    root->addWidget(new QLabel(nl ? QStringLiteral("Bewerk per klasse de launcher. Args: gebruik %f voor ISO-pad, %e voor entry. Geen pad/folder-regels voor detectie.")
                                  : QStringLiteral("Edit the launcher per class. Args: use %f for ISO path, %e for entry. No path/folder rules for detection."), this));

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("id"), QStringLiteral("label"), QStringLiteral("exec"),
                                        nl ? QStringLiteral("args (spatie-gescheiden, %f/%e)") : QStringLiteral("args (space-separated, %f/%e)"),
                                        nl ? QStringLiteral("prioriteit") : QStringLiteral("priority")});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(m_table, 1);

    auto *rowBtns = new QHBoxLayout();
    auto *addBtn = new QPushButton(nl ? QStringLiteral("Rij toevoegen") : QStringLiteral("Add row"), this);
    auto *delBtn = new QPushButton(nl ? QStringLiteral("Rij verwijderen") : QStringLiteral("Remove row"), this);
    auto *browseExecBtn = new QPushButton(nl ? QStringLiteral("Browse exec...") : QStringLiteral("Browse exec..."), this);
    rowBtns->addWidget(addBtn);
    rowBtns->addWidget(delBtn);
    rowBtns->addWidget(browseExecBtn);
    rowBtns->addStretch(1);
    root->addLayout(rowBtns);

    auto *fbBox = new QGroupBox(nl ? QStringLiteral("Fallback (als launch faalt)") : QStringLiteral("Fallback (if launch fails)"), this);
    auto *fbForm = new QFormLayout(fbBox);
    m_fallbackExec = new QLineEdit(this);
    m_fallbackArgs = new QLineEdit(this);
    fbForm->addRow(QStringLiteral("exec"), m_fallbackExec);
    fbForm->addRow(QStringLiteral("args"), m_fallbackArgs);
    root->addWidget(fbBox);

    auto *actions = new QHBoxLayout();
    auto *reloadBtn = new QPushButton(nl ? QStringLiteral("Herladen") : QStringLiteral("Reload"), this);
    auto *saveBtn = new QPushButton(nl ? QStringLiteral("Opslaan") : QStringLiteral("Save"), this);
    auto *closeBtn = new QPushButton(nl ? QStringLiteral("Sluiten") : QStringLiteral("Close"), this);
    actions->addWidget(reloadBtn);
    actions->addStretch(1);
    actions->addWidget(saveBtn);
    actions->addWidget(closeBtn);
    root->addLayout(actions);

    connect(addBtn, &QPushButton::clicked, this, &ConfigDialog::onAddRow);
    connect(delBtn, &QPushButton::clicked, this, &ConfigDialog::onRemoveRow);
    connect(browseExecBtn, &QPushButton::clicked, this, &ConfigDialog::onBrowseExec);
    connect(reloadBtn, &QPushButton::clicked, this, &ConfigDialog::onReload);
    connect(saveBtn, &QPushButton::clicked, this, &ConfigDialog::onSave);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    loadFromConfig();
}

void ConfigDialog::loadFromConfig()
{
    const auto classes = m_cfg.classes();
    m_table->setRowCount(classes.size());
    for (int i = 0; i < classes.size(); ++i) {
        const auto &c = classes[i];
        m_table->setItem(i, 0, new QTableWidgetItem(c.id));
        m_table->setItem(i, 1, new QTableWidgetItem(c.label));
        m_table->setItem(i, 2, new QTableWidgetItem(c.exec));
        m_table->setItem(i, 3, new QTableWidgetItem(c.args.join(QLatin1Char(' '))));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(c.priority)));
    }
    const QStringList fb = m_cfg.fallbackElse();
    m_fallbackExec->setText(fb.value(0));
    m_fallbackArgs->setText(QStringList(fb.mid(1)).join(QLatin1Char(' ')));
}

void ConfigDialog::saveToConfig()
{
    QJsonArray arr;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto get = [&](int col) { auto *it = m_table->item(i, col); return it ? it->text().trimmed() : QString(); };
        const QString id = get(0);
        if (id.isEmpty()) continue;
        QJsonObject o;
        // Preserve existing rules/other fields by finding original class
        for (const auto &v : m_cfg.raw().value(QStringLiteral("classes")).toArray()) {
            const QJsonObject old = v.toObject();
            if (old.value(QStringLiteral("id")).toString() == id) { o = old; break; }
        }
        o.insert(QStringLiteral("id"), id);
        o.insert(QStringLiteral("label"), get(1).isEmpty() ? id : get(1));
        o.insert(QStringLiteral("exec"), get(2));
        QJsonArray args;
        const QString argLine = get(3);
        if (!argLine.isEmpty()) {
            for (const QString &a : argLine.split(QLatin1Char(' '), Qt::SkipEmptyParts)) args.append(a);
        }
        o.insert(QStringLiteral("args"), args);
        o.insert(QStringLiteral("priority"), get(4).toInt());
        if (!o.contains(QStringLiteral("rules"))) o.insert(QStringLiteral("rules"), QJsonArray());
        arr.append(o);
    }
    m_cfg.set(QStringLiteral("classes"), arr.toVariantList());

    QStringList fb;
    const QString fe = m_fallbackExec->text().trimmed();
    if (!fe.isEmpty()) {
        fb << fe;
        for (const QString &a : m_fallbackArgs->text().trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts)) fb << a;
    }
    m_cfg.set(QStringLiteral("fallback/else"), fb);
}

void ConfigDialog::onSave()
{
    saveToConfig();
    QString err;
    const bool nl = m_cfg.language() == QStringLiteral("nl");
    if (!m_cfg.save(m_configPath, &err)) {
        QMessageBox::warning(this, nl ? QStringLiteral("Save failed") : QStringLiteral("Save failed"), err);
        return;
    }
    QMessageBox::information(this, nl ? QStringLiteral("Saved") : QStringLiteral("Saved"),
                             (nl ? QStringLiteral("Config opgeslagen naar:\n") : QStringLiteral("Config saved to:\n")) + m_configPath);
}

void ConfigDialog::onReload()
{
    QString err;
    const bool nl = m_cfg.language() == QStringLiteral("nl");
    if (!m_cfg.load(m_configPath, &err)) {
        QMessageBox::warning(this, nl ? QStringLiteral("Load failed") : QStringLiteral("Load failed"), err);
        return;
    }
    loadFromConfig();
}

void ConfigDialog::onAddRow()
{
    int r = m_table->rowCount();
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem(QStringLiteral("new_class")));
    m_table->setItem(r, 1, new QTableWidgetItem(QStringLiteral("New Class")));
    m_table->setItem(r, 2, new QTableWidgetItem(QStringLiteral("")));
    m_table->setItem(r, 3, new QTableWidgetItem(QStringLiteral("%f")));
    m_table->setItem(r, 4, new QTableWidgetItem(QStringLiteral("0")));
}

void ConfigDialog::onRemoveRow()
{
    const int r = m_table->currentRow();
    if (r >= 0) m_table->removeRow(r);
}

void ConfigDialog::onBrowseExec()
{
    const bool nl = m_cfg.language() == QStringLiteral("nl");
    int r = m_table->currentRow();
    if (r < 0) {
        QMessageBox::information(this, nl ? QStringLiteral("No row selected") : QStringLiteral("No row selected"),
                                 nl ? QStringLiteral("Selecteer eerst een klasse-rij.") : QStringLiteral("Select a class row first."));
        return;
    }
    auto *it = m_table->item(r, 2);
    const QString current = it ? it->text() : QString();
    const QString f = QFileDialog::getOpenFileName(this, nl ? QStringLiteral("Choose executable") : QStringLiteral("Choose executable"),
                                                   current.isEmpty() ? QStringLiteral("/usr/bin") : current,
                                                   nl ? QStringLiteral("Programs (*)") : QStringLiteral("Programs (*)"));
    if (f.isEmpty()) return;
    if (!it) { it = new QTableWidgetItem(); m_table->setItem(r, 2, it); }
    it->setText(f);
}

} // namespace iso
