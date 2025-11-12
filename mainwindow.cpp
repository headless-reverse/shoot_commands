#include "mainwindow.h"
#include "commandexecutor.h"
#include "settingsdialog.h"
#include "sequencerunner.h"
#include <fstream>
#include <iostream>
#include <QApplication>
#include <QCoreApplication>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QDockWidget>
#include <QListWidget>
#include <QMenuBar>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLineEdit>
#include <QTextEdit>
#include <QMessageBox>
#include <QFileInfo>
#include <QEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QDir>
#include <QHeaderView>
#include <QAction>
#include <QCloseEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QStandardItem>
#include <QSpinBox>
#include <QTabWidget>

class LogDialog : public QDialog {
public:
    explicit LogDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("headless");
        resize(900, 400);
        setStyleSheet("background: #000; border: none;");
        auto layout = new QVBoxLayout(this);
        m_output = new QTextEdit();
        m_output->setReadOnly(true);
        m_output->setStyleSheet("background: #000; color: #f0f0f0; border: none; font-family: monospace;");
        layout->addWidget(m_output);
        m_fontSize = m_output->font().pointSize();
        installEventFilter(this);
    }
    void setDocument(QTextDocument *doc) {
        if (doc) m_output->setDocument(doc);
    }
    void appendText(const QString &text, const QColor &color = QColor("#f0f0f0")) {
        m_output->setTextColor(color);
        m_output->append(text);
        m_output->setTextColor(QColor("#f0f0f0"));
    }
    void clear() { m_output->clear(); }
    QString text() const { return m_output->toPlainText(); }
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            if (ke->modifiers() == Qt::ControlModifier && ke->key() == Qt::Key_C) {
                return false;
            }
        }
        return QDialog::eventFilter(obj, event);
    }
    void wheelEvent(QWheelEvent *event) override {
        if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
            int delta = event->angleDelta().y();
            if (delta > 0) m_fontSize = qMin(m_fontSize + 1, 32);
            else m_fontSize = qMax(m_fontSize - 1, 8);
            QFont f = m_output->font(); f.setPointSize(m_fontSize); m_output->setFont(f);
            event->accept();
        } else QDialog::wheelEvent(event);
    }
private:
    QTextEdit *m_output = nullptr;
    int m_fontSize = 10;
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("shoot_commands");
    resize(1100, 720);
    ensureJsonPathLocal(); // Zmieniono, aby używać /usr/local/etc/shoot_commands
    m_executor = new CommandExecutor(this);
    m_commandTimer = new QTimer(this);
    connect(m_commandTimer, &QTimer::timeout, this, &MainWindow::executeScheduledCommand);
    m_sequenceRunner = new SequenceRunner(m_executor, this);
    connect(m_sequenceRunner, &SequenceRunner::sequenceStarted, this, &MainWindow::onSequenceStarted);
    connect(m_sequenceRunner, &SequenceRunner::sequenceFinished, this, &MainWindow::onSequenceFinished);
    connect(m_sequenceRunner, &SequenceRunner::commandExecuting, this, &MainWindow::onWorkflowCommandExecuting);
    connect(m_sequenceRunner, &SequenceRunner::logMessage, this, &MainWindow::handleWorkflowLog);
    m_isRootShell = m_settings.value("isRootShell", false).toBool();
    connect(m_executor, &CommandExecutor::outputReceived, this, &MainWindow::onOutput);
    connect(m_executor, &CommandExecutor::errorReceived, this, &MainWindow::onError);
    connect(m_executor, &CommandExecutor::started, this, &MainWindow::onProcessStarted);
    connect(m_executor, &CommandExecutor::finished, this, &MainWindow::onProcessFinished);
    setupMenus();
    m_categoryList = new QListWidget();
    m_categoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_categoryList, &QListWidget::currentItemChanged, this, &MainWindow::onCategoryChanged);
    m_dockCategories = new QDockWidget(tr("Categories"), this);
    m_dockCategories->setObjectName("dockCategories");
    m_dockCategories->setWidget(m_categoryList);
    m_dockCategories->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCategories);
    m_commandModel = new QStandardItemModel(this);
    m_commandModel->setHorizontalHeaderLabels(QStringList{"Command", "Description"});
    m_commandProxy = new QSortFilterProxyModel(this);
    m_commandProxy->setSourceModel(m_commandModel);
    m_commandProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_commandProxy->setFilterKeyColumn(-1);
    m_commandView = new QTreeView();
    m_commandView->setModel(m_commandProxy);
    m_commandView->setRootIsDecorated(false);
    m_commandView->setAlternatingRowColors(true);
    m_commandView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_commandView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_commandView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_commandView->setEditTriggers(QAbstractItemView::NoEditTriggers); // Blokowanie edycji
    connect(m_commandView, &QTreeView::doubleClicked, this, &MainWindow::onCommandDoubleClicked);
    connect(m_commandView, &QTreeView::clicked, [this](const QModelIndex &idx){
        QModelIndex s = m_commandProxy->mapToSource(idx);
        if (s.isValid()) {
            QString cmd = m_commandModel->item(s.row(), 0)->text();
            m_commandEdit->setText(cmd);
            if (m_dockControls) { m_dockControls->setVisible(true); m_dockControls->raise(); if (m_commandEdit) { m_commandEdit->setFocus(); m_commandEdit->selectAll(); } }
        }
    });
    m_commandView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_commandView, &QWidget::customContextMenuRequested, this, [this](const QPoint &pt){
        QModelIndex idx = m_commandView->indexAt(pt);
        if (!idx.isValid()) return;
        QModelIndex s = m_commandProxy->mapToSource(idx);
        if (!s.isValid()) return;
        QString cmd = m_commandModel->item(s.row(), 0)->text();
        QMenu menu(this);
        menu.addAction("Execute", [this, cmd](){ m_commandEdit->setText(cmd); runCommand(); });
        menu.addAction("Edit", [this, s](){ m_commandView->selectionModel()->clear(); m_commandView->setCurrentIndex(m_commandProxy->mapFromSource(s)); editCommand(); });
        menu.addAction("Remove", [this, s](){ m_commandView->selectionModel()->clear(); m_commandView->setCurrentIndex(m_commandProxy->mapFromSource(s)); removeCommand(); });
        menu.exec(m_commandView->viewport()->mapToGlobal(pt));
    });
    m_dockCommands = new QDockWidget(tr("Commands"), this);
    m_dockCommands->setObjectName("dockCommands");
    m_dockCommands->setWidget(m_commandView);
    m_dockCommands->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCommands);
    splitDockWidget(m_dockCategories, m_dockCommands, Qt::Horizontal);    
    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setStyleSheet("background: #000; color: #f0f0f0; font-family: monospace;");
    m_dockLog = new QDockWidget(tr("Log Output"), this);
    m_dockLog->setObjectName("dockLog");
    m_dockLog->setWidget(m_log);
    m_dockLog->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_dockLog);    
    m_dockControls = new QDockWidget(tr("Controls"), this); 
    m_dockControls->setObjectName("dockControls");
    m_dockControls->setWidget(createControlsWidget());
    m_dockControls->setAllowedAreas(Qt::TopDockWidgetArea | Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::TopDockWidgetArea, m_dockControls);
    setupWorkflowDock();     
    m_viewCategoriesAct = m_dockCategories->toggleViewAction();
    m_viewCommandsAct = m_dockCommands->toggleViewAction();
    m_viewLogAct = m_dockLog->toggleViewAction();
    m_viewControlsAct = m_dockControls->toggleViewAction();
    m_viewWorkflowAct = m_dockWorkflow->toggleViewAction();    
    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_viewCategoriesAct);
    viewMenu->addAction(m_viewCommandsAct);
    viewMenu->addAction(m_viewControlsAct);
    viewMenu->addAction(m_viewWorkflowAct);
    viewMenu->addAction(m_viewLogAct);
    LogDialog *dlg = new LogDialog(this);
    dlg->setDocument(m_log->document());
    dlg->move(this->x() + this->width() + 20, this->y());
    dlg->show();
    m_detachedLogDialog = dlg;
    loadCommands();
    populateCategoryList();
    if (m_categoryList->count() > 0) m_categoryList->setCurrentRow(0);
    if (!m_settings.contains("windowState")) {
        restoreDefaultLayout(); 
    }    
    restoreWindowStateFromSettings();    
    m_commandView->installEventFilter(this);
    m_categoryList->installEventFilter(this);
    if (m_commandEdit) m_commandEdit->installEventFilter(this);
}

MainWindow::~MainWindow() {
    if (m_commandTimer && m_commandTimer->isActive()) m_commandTimer->stop();
    saveCommands();
    saveWindowStateToSettings();
}

void MainWindow::setupMenus() {
    QMenu *file = menuBar()->addMenu("&File");
    QAction *loadAct = file->addAction("Load commands…");
    QAction *saveAct = file->addAction("Save commands as…");
    QAction *quitAct = file->addAction("Quit");
    connect(loadAct, &QAction::triggered, this, [this]{
        QDir startDir = QDir("/usr/local/etc/shoot_commands");
        QString fn = QFileDialog::getOpenFileName(
                             this,
                             tr("Load JSON"),
                             startDir.path(),
                             tr("JSON files (*.json);;All files (*)"));
        if (!fn.isEmpty()) {
            m_jsonFile = fn;
            loadCommands();
            populateCategoryList();
        }
    });
    connect(saveAct, &QAction::triggered, this, [this]{
        QDir startDir = QDir("/usr/local/etc/shoot_commands");
        QString fn = QFileDialog::getSaveFileName(
                             this,
                             tr("Save JSON"),
                             startDir.filePath("shoot_commands.json"),
                             tr("JSON files (*.json);;All files (*)"));
        if (!fn.isEmpty()) {
            m_jsonFile = fn;
            saveCommands();
        }
    });
    connect(quitAct, &QAction::triggered, this, &QMainWindow::close);
    QMenu *proc = menuBar()->addMenu("&Process");
    QAction *stopAct = proc->addAction("Stop current command");
    connect(stopAct, &QAction::triggered, this, &MainWindow::stopCommand);
    QMenu *settings = menuBar()->addMenu("&Settings");
    QAction *restoreAct = settings->addAction("Restore default layout");
    connect(restoreAct, &QAction::triggered, this, &MainWindow::restoreDefaultLayout);
    QAction *appSettingsAct = settings->addAction("Application settings...");
    connect(appSettingsAct, &QAction::triggered, this, &MainWindow::showSettingsDialog);
}

void MainWindow::ensureJsonPathLocal() {
    m_jsonFile = QDir("/usr/local/etc/shoot_commands").filePath("shoot_commands.json");
}

QWidget* MainWindow::createWorkflowTabWidget() {
    QWidget *w = new QWidget();
    auto mainLayout = new QVBoxLayout(w);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    auto loadLayout = new QHBoxLayout();
    QPushButton *loadBtn = new QPushButton("Load Workflow JSON...");
    loadBtn->setStyleSheet("background-color: #8BC34A; color: black;");
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadWorkflowFile);
    loadLayout->addWidget(loadBtn);
    QPushButton *runBtn = new QPushButton("Run Sequence");
    runBtn->setStyleSheet("background-color: #4CAF50; color: black;");
    connect(runBtn, &QPushButton::clicked, m_sequenceRunner, &SequenceRunner::startSequence);
    loadLayout->addWidget(runBtn);
    QPushButton *stopBtn = new QPushButton("Stop Sequence");
    stopBtn->setStyleSheet("background-color: #F44336; color: black;");
    connect(stopBtn, &QPushButton::clicked, m_sequenceRunner, &SequenceRunner::stopSequence);
    loadLayout->addWidget(stopBtn);
    mainLayout->addLayout(loadLayout);
    mainLayout->addStretch(1);
    return w;
}

void MainWindow::setupWorkflowDock() {
    m_dockWorkflow = new QDockWidget(tr("Workflow"), this);
    m_dockWorkflow->setObjectName("dockWorkflow");
    m_dockWorkflow->setWidget(createWorkflowTabWidget());
    m_dockWorkflow->setAllowedAreas(Qt::TopDockWidgetArea | Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea | Qt::BottomDockWidgetArea);    
    addDockWidget(Qt::TopDockWidgetArea, m_dockWorkflow);
    splitDockWidget(m_dockControls, m_dockWorkflow, Qt::Horizontal);
}

QWidget* MainWindow::createControlsWidget() {
    QWidget *w = new QWidget();
    w->setMinimumHeight(120); /* Zwiększenie minimalnej wysokości */
    w->setMinimumWidth(350);        
    auto mainLayout = new QVBoxLayout(w);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    m_commandEdit = new QLineEdit();
    connect(m_commandEdit, &QLineEdit::returnPressed, this, &MainWindow::runCommand);
    mainLayout->addWidget(m_commandEdit);
    auto timeLayout = new QHBoxLayout();
    timeLayout->addWidget(new QLabel("Interval (s):"));
    m_intervalSpinBox = new QSpinBox();
    m_intervalSpinBox->setRange(1, 86400); /* 1 sekunda do 24 godzin */
    m_intervalSpinBox->setValue(5);
    m_intervalSpinBox->setMaximumWidth(60);
    timeLayout->addWidget(m_intervalSpinBox);
    m_periodicToggle = new QCheckBox("Periodic");
    m_periodicToggle->setChecked(true); /* Domyślnie cykliczne */
    timeLayout->addWidget(m_periodicToggle);
    timeLayout->addStretch(1);
    m_scheduleBtn = new QPushButton("Schedule");
    m_scheduleBtn->setStyleSheet("background-color: #FFAA00; color: black;");
    connect(m_scheduleBtn, &QPushButton::clicked, this, &MainWindow::onScheduleButtonClicked);
    timeLayout->addWidget(m_scheduleBtn);
    QPushButton *stopTimerBtn = new QPushButton("Stop Timer");
    stopTimerBtn->setStyleSheet("background-color: #E68D8D; color: black;");
    connect(stopTimerBtn, &QPushButton::clicked, [this]{
        if (m_commandTimer->isActive()) {
            m_commandTimer->stop();
            appendLog("Scheduled command timer stopped by user.", "#E68D8D");
        } else {
            appendLog("Timer is not currently running.", "#BDBDBD");
        }
    });
    timeLayout->addWidget(stopTimerBtn);
    mainLayout->addLayout(timeLayout);
    auto btnLayout = new QHBoxLayout();
    m_rootToggle = new QCheckBox("Run as Root (su -)");
    m_rootToggle->setChecked(m_isRootShell);        
    connect(m_rootToggle, &QCheckBox::checkStateChanged, this, [this](int state) {
        m_isRootShell = (state == Qt::Checked);            
        m_settings.setValue("isRootShell", m_isRootShell);
    });        
    btnLayout->addWidget(m_rootToggle);
    btnLayout->addSpacing(20);
    m_runBtn = new QPushButton("Execute");
    connect(m_runBtn, &QPushButton::clicked, this, &MainWindow::runCommand);
    btnLayout->addWidget(m_runBtn);
    m_stopBtn = new QPushButton("Stop Process");
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stopCommand);
    btnLayout->addWidget(m_stopBtn);
    m_clearBtn = new QPushButton("Clear Log");
    connect(m_clearBtn, &QPushButton::clicked, [this](){ m_log->clear(); m_detachedLogDialog->clear(); }); /* Ulepszenie: czyszczenie obu logów */
    btnLayout->addWidget(m_clearBtn);
    m_saveBtn = new QPushButton("Save Log .txt");
    connect(m_saveBtn, &QPushButton::clicked, [this](){
        QString defDir = "/usr/local/log";
        QDir d(defDir);
        if (!d.exists()) QDir().mkpath(defDir);
        
        QString def = QString("shell_log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        QString fn = QFileDialog::getSaveFileName(this, "Save log", d.filePath(def), "Text Files (*.txt);;All Files (*)");
        if (!fn.isEmpty()) {
            QFile f(fn);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(m_log->toPlainText().toUtf8());
                f.close();
                QMessageBox::information(this, "Saved", QString("Saved: %1").arg(fn));
            } else {
                QMessageBox::warning(this, "Error", "Cannot write file");
            }
        }
    });
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);
    // Rząd 4: Add, Edit, Remove
    auto editLayout = new QHBoxLayout();
    auto addBtn = new QPushButton("Add Command");
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addCommand);
    editLayout->addWidget(addBtn);
    auto editBtn = new QPushButton("Edit Command");
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editCommand);
    editLayout->addWidget(editBtn);
    auto rmBtn = new QPushButton("Remove Command");
    connect(rmBtn, &QPushButton::clicked, this, &MainWindow::removeCommand);
    editLayout->addWidget(rmBtn);
    mainLayout->addLayout(editLayout);        
    return w;
}

void MainWindow::loadWorkflowFile() {
    QDir startDir = QDir("/usr/local/etc/shoot_commands");
    QStringList fns = QFileDialog::getOpenFileNames(
                                             this,
                                             tr("Load Workflow JSON(s)"),
                                             startDir.path(),
                                             tr("JSON files (*.json);;All files (*)"));
    if (!fns.isEmpty()) {        
        m_workflowQueue = fns; // Zapisujemy całą kolejkę        
        QString fn = m_workflowQueue.first();
        if (m_sequenceRunner->loadWorkflow(fn)) {
             appendLog(QString("Workflow loaded successfully from: %1. (%2 files in queue)").arg(fn).arg(m_workflowQueue.count()), "#8BC34A");
        } else {
             appendLog(QString("Error loading workflow from: %1").arg(fn), "#F44336");
        }
    }
}

void MainWindow::onSequenceStarted() {
    appendLog("--- HEADLESS SEQUENCE STARTED ---", "#4CAF50");    
    if (m_dockWorkflow) {
        m_dockWorkflow->setVisible(true);
        m_dockWorkflow->raise();        
    }
}

void MainWindow::onSequenceFinished(bool success) {
    if (success) {
        appendLog("--- HEADLESS SEQUENCE FINISHED SUCCESSFULLY ---", "#4CAF50");
    } else {
        appendLog("--- HEADLESS SEQUENCE TERMINATED WITH ERROR ---", "#F44336");
    }
}

void MainWindow::onWorkflowCommandExecuting(const QString &cmd, int index, int total) {
    appendLog(QString(">> HEADLESS [%1/%2]: %3").arg(index+1).arg(total).arg(cmd), "#00BCD4");
}

void MainWindow::handleWorkflowLog(const QString &text, const QString &color) {
    appendLog(text, color);
}

void MainWindow::onScheduleButtonClicked() {
    const QString cmdText = m_commandEdit->text().trimmed();
    const int interval = m_intervalSpinBox->value();
    const bool periodic = m_periodicToggle->isChecked();
    if (cmdText.isEmpty()) {
        QMessageBox::warning(this, "Schedule Error", "Command cannot be empty.");
        return;
    }
    bool safeMode = m_settings.value("safeMode", false).toBool();
    if (safeMode && isDestructiveCommand(cmdText)) {
        QMessageBox::warning(this, "Safe mode", "Application is in Safe Mode. Destructive commands are blocked from scheduling.");
        return;
    }
    if (m_commandTimer->isActive()) {
        m_commandTimer->stop();
        appendLog("Previous scheduled command canceled.", "#FFAA66");
    }
    m_scheduledCommand = cmdText;
    m_commandTimer->setSingleShot(!periodic);
    m_commandTimer->setInterval(interval * 1000); /* Interwał w milisekundach */
    if (periodic) {
        appendLog(QString("Scheduled periodic command: %1 (every %2 s)").arg(cmdText).arg(interval), "#4CAF50");
    } else {
        appendLog(QString("Scheduled one-shot command: %1 (in %2 s)").arg(cmdText).arg(interval), "#00BCD4");
    }
    m_commandTimer->start();
}

void MainWindow::executeScheduledCommand() {
    const QString cmdToRun = m_scheduledCommand;
    if (m_commandTimer->isSingleShot()) {
        m_commandTimer->stop();
    }       
    m_commandEdit->setText(cmdToRun);
    runCommand();
}

void MainWindow::loadCommands() {
    m_commands.clear();
    std::ifstream ifs(m_jsonFile.toStdString());
    if (!ifs.is_open()) {
        QStringList cats = {
            "System", "systemctl", "config"
        };
        for (const QString &c: cats) m_commands.insert(c, {});
        saveCommands();
        return;
    }
    try {
        nlohmann::json j;
        ifs >> j;            
        for (auto it = j.begin(); it != j.end(); ++it) {
            const QString category = QString::fromStdString(it.key());
            QVector<SystemCmd> vec;            
            if (it.value().is_array()) {
                for (const auto& cmd_obj : it.value()) {
                    if (cmd_obj.is_object() && cmd_obj.contains("command") && cmd_obj.contains("description")) {
                        SystemCmd c;
                        c.command = QString::fromStdString(cmd_obj.at("command").get<std::string>());
                        c.description = QString::fromStdString(cmd_obj.at("description").get<std::string>());
                        vec.append(c);
                    }
                }
            }
            m_commands.insert(category, vec);
        }
    } catch (const nlohmann::json::exception& e) {
        QMessageBox::warning(this, "Error JSON", QString("Cannot parse JSON file: %1\nError: %2").arg(m_jsonFile).arg(e.what()));
        QStringList cats = { "System", "systemctl", "config" };
        for (const QString &c: cats) m_commands.insert(c, {});
        return;
    }
}

void MainWindow::saveCommands() {
    nlohmann::json j_root = nlohmann::json::object();
    for (auto it = m_commands.begin(); it != m_commands.end(); ++it) {
        nlohmann::json j_array = nlohmann::json::array();
        for (const SystemCmd &c: it.value()) {
            j_array.push_back({
                {"command", c.command.toStdString()},
                {"description", c.description.toStdString()}
            });
        }
        j_root[it.key().toStdString()] = j_array;
    }    
    QFileInfo fileInfo(m_jsonFile);
    QString dirPath = fileInfo.absolutePath();
    QDir dir;
    if (!dir.mkpath(dirPath)) {
        QMessageBox::critical(this, "Error Save JSON", QString("Cannot create directory: %1. Check permissions (or run as root).").arg(dirPath));
        return;
    }    
    try {
        std::ofstream ofs(m_jsonFile.toStdString());
        if (ofs.is_open()) {
            ofs << j_root.dump(4);
            ofs.close();
        } else {
            QMessageBox::critical(this, "Error Save JSON", QString("Cannot write JSON: %1. Check permissions (or run as root).").arg(m_jsonFile));
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error JSON Save", QString("Cannot save JSON file: %1").arg(e.what()));
    }
}

void MainWindow::populateCategoryList() {
    m_categoryList->clear();
    for (auto it = m_commands.constBegin(); it != m_commands.constEnd(); ++it) m_categoryList->addItem(it.key());
}

void MainWindow::populateCommandList(const QString &category) {
    m_commandModel->removeRows(0, m_commandModel->rowCount());
    auto vec = m_commands.value(category);
    for (const SystemCmd &c: vec) {
        QList<QStandardItem*> row;
        row << new QStandardItem(c.command) << new QStandardItem(c.description);
        m_commandModel->appendRow(row);
    }
    m_commandView->resizeColumnToContents(1);
}

void MainWindow::onCategoryChanged(QListWidgetItem *current, QListWidgetItem *) {
    if (!current) return;
    populateCommandList(current->text());
    m_commandEdit->clear();
    m_inputHistoryIndex = -1;
}

void MainWindow::onCommandSelected(const QModelIndex &current, const QModelIndex &) {
    if (!current.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(current);
    if (s.isValid()) {
        QString cmd = m_commandModel->item(s.row(), 0)->text();
        m_commandEdit->setText(cmd);
        if (m_dockControls) { m_dockControls->setVisible(true); m_dockControls->raise(); if (m_commandEdit) { m_commandEdit->setFocus(); m_commandEdit->selectAll(); } }
    }
}

void MainWindow::onCommandDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(index);
    if (s.isValid()) {
        QString cmd = m_commandModel->item(s.row(), 0)->text();
        m_commandEdit->setText(cmd);
        runCommand();
    }
}

void MainWindow::runCommand() {
    const QString cmdText = m_commandEdit->text().trimmed();
    if (cmdText.isEmpty()) return;
    bool safeMode = m_settings.value("safeMode", false).toBool();
    if (safeMode && isDestructiveCommand(cmdText)) {
        QMessageBox::warning(this, "Safe mode", "Application is in Safe Mode. Destructive commands are blocked.");
        return;
    }
    if (isDestructiveCommand(cmdText)) {
        auto reply = QMessageBox::question(this, "Confirm", QString("Command looks destructive:\n%1\nContinue?").arg(cmdText), QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }
    if (m_inputHistory.isEmpty() || m_inputHistory.last() != cmdText) m_inputHistory.append(cmdText);
    m_inputHistoryIndex = -1;
    QString program;
    QStringList args;        
    if (m_isRootShell) {
        program = "/usr/bin/sudo";  
        args << "-n" << "bash" << "-c" << cmdText;            
        appendLog(QString(">>> root: %1").arg(cmdText), "#FF0000");
    } else {
        program = "/bin/bash";  
        args << "-c" << cmdText;
        appendLog(QString(">>> user: %1").arg(cmdText), "#FFE066");
    }    
    m_executor->runSystemCommand(program, args);
}

void MainWindow::stopCommand() {
    if (m_executor) {
        m_executor->stop();
        appendLog("Process stopped by user.", "#FFAA66");
    }
}

void MainWindow::onOutput(const QString &text) {
    const QStringList lines = text.split('\n');
    for (const QString &l : lines) if (!l.trimmed().isEmpty()) appendLog(l.trimmed(), "#A9FFAC");
}

void MainWindow::onError(const QString &text) {
    const QStringList lines = text.split('\n');
    for (const QString &l : lines) if (!l.trimmed().isEmpty()) appendLog(QString("!!! %1").arg(l.trimmed()), "#FF6565");
    logErrorToFile(text);
}

void MainWindow::onProcessStarted() {
    appendLog("System command started.", "#8ECAE6");
}

void MainWindow::addCommand() {
    bool ok;
    QString cmd = QInputDialog::getText(this, "Add command", "Command:", QLineEdit::Normal, "", &ok);
    if (!ok || cmd.isEmpty()) return;
    QString desc = QInputDialog::getText(this, "Add command", "Description:", QLineEdit::Normal, "", &ok);
    if (!ok) return;
    QString category = m_categoryList->currentItem() ? m_categoryList->currentItem()->text() : QString();
    if (category.isEmpty()) { QMessageBox::warning(this, "No category", "Select a category first."); return; }
    m_commands[category].append({cmd, desc});
    populateCommandList(category);
    saveCommands();
}

void MainWindow::editCommand() {
    QModelIndex idx = m_commandView->currentIndex();
    if (!idx.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(idx);
    if (!s.isValid()) return;
    QString cmd = m_commandModel->item(s.row(), 0)->text();
    QString desc = m_commandModel->item(s.row(), 1)->text();
    bool ok;
    QString ncmd = QInputDialog::getText(this, "Edit command", "Command:", QLineEdit::Normal, cmd, &ok);
    if (!ok) return;
    QString ndesc = QInputDialog::getText(this, "Edit command", "Description:", QLineEdit::Normal, desc, &ok);
    if (!ok) return;
    QString category = m_categoryList->currentItem() ? m_categoryList->currentItem()->text() : QString();
    if (category.isEmpty()) return;
    auto &vec = m_commands[category];
    for (int i=0;i<vec.size();++i) {
        if (vec[i].command == cmd && vec[i].description == desc) { vec[i].command = ncmd; vec[i].description = ndesc; break; }
    }
    populateCommandList(category);
    saveCommands();
}

void MainWindow::removeCommand() {
    QModelIndex idx = m_commandView->currentIndex();
    if (!idx.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(idx);
    if (!s.isValid()) return;
    QString cmd = m_commandModel->item(s.row(), 0)->text();
    QString category = m_categoryList->currentItem() ? m_categoryList->currentItem()->text() : QString();
    if (category.isEmpty()) return;
    auto &vec = m_commands[category];
    for (int i=0;i<vec.size();++i) {
        if (vec[i].command == cmd) { vec.removeAt(i); break; }
    }
    populateCommandList(category);
    saveCommands();
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    appendLog(QString("Process finished. Exit code: %1").arg(exitCode), "#BDBDBD");
    if (exitCode != 0) appendLog(QString("Command finished with error code: %1").arg(exitCode), "#FF6565");
}

void MainWindow::appendLog(const QString &text, const QString &color) {
    QString line = text;
    if (!color.isEmpty()) m_log->setTextColor(QColor(color)); else m_log->setTextColor(QColor("#F0F0F0"));
    m_log->append(line);
    m_log->setTextColor(QColor("#F0F0F0"));
}

void MainWindow::logErrorToFile(const QString &text) {
    const QString logDir = "/usr/local/log";
    QDir d(logDir);
    if (!d.exists()) QDir().mkpath(logDir);
    QString logFile = d.filePath("shoot_commands.log");
    QFile f(logFile);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << " - " << text << "\n";
        f.close();
    }
}

bool MainWindow::isDestructiveCommand(const QString &cmd) {
    QString c = cmd.toLower();
    return c.contains("rm ") || c.contains("wipe") || c.contains("format") || c.contains("dd ") || c.contains("flashall");
}

void MainWindow::navigateHistory(int direction) {
    if (m_inputHistory.isEmpty()) {
        return;
    }
    if (m_inputHistoryIndex == -1) {
        if (direction == -1) {
            m_inputHistoryIndex = m_inputHistory.size() - 1;
        } else {
            return;
        }
    } else {
        m_inputHistoryIndex += direction;
        if (m_inputHistoryIndex < 0) {
            m_inputHistoryIndex = 0;
            return;
        }
        if (m_inputHistoryIndex >= m_inputHistory.size()) {
            m_commandEdit->clear();
            m_inputHistoryIndex = -1;
            return;
        }
    }
    m_commandEdit->setText(m_inputHistory.at(m_inputHistoryIndex));
    m_commandEdit->selectAll();
}

void MainWindow::restoreDefaultLayout() {
    m_settings.remove("geometry");
    m_settings.remove("windowState");
    addDockWidget(Qt::TopDockWidgetArea, m_dockControls);
    addDockWidget(Qt::TopDockWidgetArea, m_dockWorkflow);
    splitDockWidget(m_dockControls, m_dockWorkflow, Qt::Horizontal);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCategories);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCommands);
    splitDockWidget(m_dockCategories, m_dockCommands, Qt::Horizontal); 
    addDockWidget(Qt::BottomDockWidgetArea, m_dockLog);
    saveWindowStateToSettings();
}

void MainWindow::showSettingsDialog() {
    SettingsDialog dlg(this);
    dlg.setSafeMode(m_settings.value("safeMode", false).toBool());
    if (dlg.exec() == QDialog::Accepted) {
        m_settings.setValue("safeMode", dlg.safeMode());
    }
}

void MainWindow::restoreWindowStateFromSettings() {
    if (m_settings.contains("geometry")) restoreGeometry(m_settings.value("geometry").toByteArray());
    if (m_settings.contains("windowState")) restoreState(m_settings.value("windowState").toByteArray());
    m_isRootShell = m_settings.value("isRootShell", false).toBool();
    if (m_rootToggle) m_rootToggle->setChecked(m_isRootShell);
}

void MainWindow::saveWindowStateToSettings() {
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("windowState", saveState());
    m_settings.setValue("isRootShell", m_isRootShell);
}

QModelIndex MainWindow::currentCommandModelIndex() const {
    QModelIndex idx = m_commandView->currentIndex();
    if (!idx.isValid()) return QModelIndex();
    return m_commandProxy->mapToSource(idx);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if ((obj == m_commandView || obj == m_categoryList || obj == m_commandEdit) && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (obj == m_commandEdit) {
            if (ke->key() == Qt::Key_Up) {
                navigateHistory(-1);
                return true;
            }
            if (ke->key() == Qt::Key_Down) {
                navigateHistory(1);
                return true;
            }
        }        
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (obj == m_categoryList) {
                if (m_commandModel->rowCount() > 0) {
                    QModelIndex firstIndex = m_commandView->model()->index(0, 0);
                    if (firstIndex.isValid()) {
                        m_commandView->setCurrentIndex(firstIndex);
                        m_commandView->setFocus();
                        return true;
                    }
                }
                if (m_commandEdit) {
                    m_commandEdit->setFocus();
                    return true;
                }
            }
            if (obj == m_commandView) {
                onCommandDoubleClicked(m_commandView->currentIndex());
                return true;
            }
            if (obj == m_commandEdit) {
                return false;
            }
        }
        return false;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveWindowStateToSettings();
    QMainWindow::closeEvent(event);
}

#include "mainwindow.moc"
