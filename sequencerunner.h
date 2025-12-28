#pragma once

#include <QProcess>
#include <QObject>
#include <QList>
#include <QTimer>
#include <QJsonObject>

class CommandExecutor;

struct WorkflowCmd {
    QString command;
    int delayAfterMs = 0;
    bool runAsRoot = false;
    bool stopOnError = true;
};

class SequenceRunner : public QObject {
    Q_OBJECT
public:
    explicit SequenceRunner(CommandExecutor *executor, QObject *parent = nullptr);
    bool loadWorkflow(const QString &filePath, bool clearExisting = true);
    QStringList getCommandsAsText() const;
    void startSequence();
    void stopSequence(bool forcedStop = true);
    void setIntervalToggle(bool toggle);
    void setIntervalValue(int seconds);
    bool isRunning() const { return m_isRunning; }

signals:
    void sequenceStarted();
    void sequenceFinished(bool success);
    void scheduleRestart(int intervalSeconds); 
    void commandExecuting(const QString &cmd, int index, int total);
    void logMessage(const QString &text, const QString &color);

private slots:
    void onCommandFinished(int exitCode, QProcess::ExitStatus exitStatus); 
    void onDelayTimeout();

private:
    CommandExecutor *m_executor;
    QList<WorkflowCmd> m_commands;
    QTimer m_delayTimer;
    int m_currentIndex = 0;
    bool m_isRunning = false;
    bool m_isInterval = false;
    int m_intervalValueS = 60;   
    void finishSequence(bool success); 
    void executeNextCommand();
    WorkflowCmd parseCommandFromJson(const QJsonObject &obj);
};
