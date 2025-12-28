#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "systemcmd.h"
#include <QMainWindow>
#include <QMap>
#include <QVector>
#include <QString>
#include <QProcess>
#include <QSettings>
#include <QCheckBox>
#include <QTimer>
#include <QDateTime>
#include "nlohmann/json.hpp"

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QTextEdit;
class QDockWidget;
class QTreeView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QPushButton;
class QSpinBox;
class CommandExecutor;
class QAction;
class LogDialog;
class SequenceRunner;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // UI Slots for Manual Commands
    void onCategoryChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onCommandSelected(const QModelIndex &current, const QModelIndex &previous);
    void onCommandDoubleClicked(const QModelIndex &index);
    void runCommand();
    void stopCommand();
    void addCommand();
    void editCommand();
    void removeCommand();
    void saveCommands();
    void loadCommands();    
    // Process Slots
    void onOutput(const QString &text);
    void onError(const QString &text);
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    // General UI Slots
    void restoreDefaultLayout();
    void showSettingsDialog();    
    // Manual Scheduler Slots
    void onScheduleButtonClicked();
    void executeScheduledCommand();
    // Workflow / Sequence Slots
    void loadWorkflowFile();
    void onSequenceStarted();
    void onSequenceFinished(bool success);
    void onWorkflowCommandExecuting(const QString &cmd, int index, int total);
    void handleWorkflowLog(const QString &text, const QString &color);
    void showLoadedWorkflow();
    void startIntervalSequence();
    void stopIntervalSequence();
    void updateTimerDisplay();

private:
    // Components
    QDockWidget *m_dockCategories = nullptr;
    QDockWidget *m_dockCommands = nullptr;
    QDockWidget *m_dockLog = nullptr;
    QDockWidget *m_dockControls = nullptr;    
    QDockWidget *m_dockWorkflow = nullptr;    
    QListWidget *m_categoryList = nullptr;
    QTreeView *m_commandView = nullptr;
    QStandardItemModel *m_commandModel = nullptr;
    QSortFilterProxyModel *m_commandProxy = nullptr;
    QLineEdit *m_commandEdit = nullptr;
    QTextEdit *m_log = nullptr;    
    // Control Buttons
    QPushButton *m_runBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QCheckBox *m_rootToggle = nullptr;    
    // Manual Scheduler Widgets
    QSpinBox *m_intervalSpinBox = nullptr;
    QCheckBox *m_periodicToggle = nullptr;
    QPushButton *m_scheduleBtn = nullptr;
    QTimer *m_commandTimer = nullptr;
    QString m_scheduledCommand;
    // Workflow Widgets & Logic
    SequenceRunner *m_sequenceRunner = nullptr;
    QStringList m_workflowQueue; 
    QTimer *m_sequenceIntervalTimer = nullptr; 
    QTimer *m_displayTimer = nullptr;          
    QDateTime m_nextSequenceTime;
    QCheckBox *m_sequenceIntervalToggle = nullptr;
    QSpinBox *m_sequenceIntervalSpinBox = nullptr;
    QLabel *m_sequenceTimerDisplay = nullptr;
    QPushButton *m_showJsonBtn = nullptr;
    // Data & Core
    QMap<QString, QVector<SystemCmd>> m_commands;
    CommandExecutor *m_executor = nullptr;
    QString m_jsonFile = QStringLiteral("shoot_commands.json");
    QStringList m_inputHistory;
    int m_inputHistoryIndex = -1;
    bool m_isRootShell = false;
    QSettings m_settings{"shoot_commands", "system_shell"};
    LogDialog *m_detachedLogDialog = nullptr;
    // Actions
    QAction *m_viewCategoriesAct = nullptr;
    QAction *m_viewCommandsAct = nullptr;
    QAction *m_viewLogAct = nullptr;
    QAction *m_viewControlsAct = nullptr;
    QAction *m_viewWorkflowAct = nullptr;
    // Helper Methods
    QWidget* createControlsWidget();
    QWidget* createWorkflowTabWidget(); 
    void setupWorkflowDock();
    void populateCategoryList();
    void populateCommandList(const QString &category);
    void appendLog(const QString &text, const QString &color = QString());
    void logErrorToFile(const QString &text);
    bool isDestructiveCommand(const QString &cmd);
    void ensureJsonPathLocal();
    void setupMenus();
    void navigateHistory(int direction);
    void restoreWindowStateFromSettings();
    void saveWindowStateToSettings();
    QModelIndex currentCommandModelIndex() const;
};

#endif // MAINWINDOW_H
