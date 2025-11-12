#include "sequencerunner.h"
#include "commandexecutor.h"
#include "nlohmann/json.hpp"

#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

SequenceRunner::SequenceRunner(CommandExecutor *executor, QObject *parent)
    : QObject(parent), m_executor(executor) {    
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &SequenceRunner::onDelayTimeout);    
    connect(m_executor, &CommandExecutor::finished, this, &SequenceRunner::onCommandFinished);
}

WorkflowCmd SequenceRunner::parseCommandFromJson(const QJsonObject &obj) {
    WorkflowCmd cmd;
    cmd.command = obj.value("command").toString();
    cmd.delayAfterMs = obj.value("delayAfterMs").toInt(1000);
    cmd.runAsRoot = obj.value("runAsRoot").toBool(false);
    cmd.stopOnError = obj.value("stopOnError").toBool(true);
    return cmd;
}

bool SequenceRunner::loadWorkflow(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit logMessage(QString("Cannot open file: %1").arg(filePath), "#F44336");
        return false;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    file.close();

    if (!doc.isArray()) {
        emit logMessage("Invalid workflow file: Root element must be a JSON Array.", "#F44336");
        return false;
    }

    m_commands.clear();
    QJsonArray array = doc.array();

    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            m_commands.append(parseCommandFromJson(value.toObject()));
        }
    }
    
    emit logMessage(QString("Loaded %1 commands into workflow queue.").arg(m_commands.count()), "#2196F3");
    return true;
}

void SequenceRunner::startSequence() {
    if (m_isRunning) {
        emit logMessage("Workflow is already running.", "#FFAA66");
        return;
    }
    if (m_commands.isEmpty()) {
        emit logMessage("Workflow queue is empty. Load a file first.", "#F44336");
        return;
    }
    m_currentIndex = 0;
    m_isRunning = true;
    emit sequenceStarted();
    executeNextCommand();
}

void SequenceRunner::stopSequence() {
    if (!m_isRunning) {
        emit logMessage("Workflow is not running.", "#BDBDBD");
        return;
    }    
    m_executor->stop(); 
    m_delayTimer.stop();    
    m_isRunning = false;
    emit sequenceFinished(false);
}

void SequenceRunner::executeNextCommand() {
    if (!m_isRunning || m_currentIndex >= m_commands.count()) {
        m_isRunning = false;
        if (m_currentIndex >= m_commands.count()) {
             emit sequenceFinished(true);
        }
        return;
    }
    const WorkflowCmd &currentCmd = m_commands.at(m_currentIndex);    
    emit commandExecuting(currentCmd.command, m_currentIndex, m_commands.count());
    QString program;
    QStringList args;    
    if (currentCmd.runAsRoot) {
        program = "/usr/bin/sudo";  
        args << "-n" << "bash" << "-c" << currentCmd.command;           
        emit logMessage(QString(">>> root: %1").arg(currentCmd.command), "#FF0000");
    } else {
        program = "/bin/bash";  
        args << "-c" << currentCmd.command;
        emit logMessage(QString(">>> user: %1").arg(currentCmd.command), "#FFE066");
    }
    m_executor->runSystemCommand(program, args);
}

void SequenceRunner::onCommandFinished(int exitCode, QProcess::ExitStatus) {
    if (!m_isRunning) return;

    const WorkflowCmd &currentCmd = m_commands.at(m_currentIndex);
    if (exitCode != 0 && currentCmd.stopOnError) {
        emit logMessage(QString("Workflow stopped: Command failed with code %1. (stopOnError is true)").arg(exitCode), "#F44336");
        m_isRunning = false;
        emit sequenceFinished(false);
        return;
    }    
    m_currentIndex++;    
    if (m_currentIndex < m_commands.count() && currentCmd.delayAfterMs > 0) {
        emit logMessage(QString("Waiting for %1 ms before next command...").arg(currentCmd.delayAfterMs), "#FFC107");
        m_delayTimer.setInterval(currentCmd.delayAfterMs);
        m_delayTimer.start();
    } else {
        executeNextCommand();
    }
}

void SequenceRunner::onDelayTimeout() {
    if (m_isRunning) {
        executeNextCommand();
    }
}

#include "sequencerunner.moc"
