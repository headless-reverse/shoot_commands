#pragma once

#include <QProcess>
#include <QObject>
#include <QList>
#include <QTimer>
#include <QJsonObject>

class CommandExecutor;

// Struktura reprezentująca pojedynczą komendę w sekwencji
struct WorkflowCmd {
    QString command;
    int delayAfterMs = 0; // Opóźnienie po wykonaniu tej komendy (w ms)
    bool runAsRoot = false;
    bool stopOnError = true;
};

class SequenceRunner : public QObject {
    Q_OBJECT
public:
    explicit SequenceRunner(CommandExecutor *executor, QObject *parent = nullptr);
    bool loadWorkflow(const QString &filePath);
    void startSequence();
    void stopSequence();
    bool isRunning() const { return m_isRunning; }

signals:
    void sequenceStarted();
    void sequenceFinished(bool success);
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

    void executeNextCommand();
    WorkflowCmd parseCommandFromJson(const QJsonObject &obj);
};
