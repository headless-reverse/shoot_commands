#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("settings");
    auto main = new QVBoxLayout(this);
    m_safeCheck = new QCheckBox("Safe mode (block destructive commands)");
    main->addWidget(m_safeCheck);
    auto btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    auto ok = new QPushButton("OK");
    auto cancel = new QPushButton("Cancel");
    btnRow->addWidget(ok);
    btnRow->addWidget(cancel);
    main->addLayout(btnRow);
    connect(ok, &QPushButton::clicked, this, &SettingsDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &SettingsDialog::reject);
}

SettingsDialog::~SettingsDialog() = default;
void SettingsDialog::setSafeMode(bool v) { m_safeCheck->setChecked(v); }
bool SettingsDialog::safeMode() const { return m_safeCheck->isChecked(); }
