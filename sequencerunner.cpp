#include "sequencerunner.h"
#include "commandexecutor.h"
#include "nlohmann/json.hpp"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

SequenceRunner::SequenceRunner(CommandExecutor *executor, QObject *parent)
    : QObject(parent), m_executor(executor) {
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &SequenceRunner::onDelayTimeout);
    connect(m_executor, &CommandExecutor::finished, this, &SequenceRunner::onCommandFinished);}

WorkflowCmd SequenceRunner::parseCommandFromJson(const QJsonObject &obj) {
    WorkflowCmd cmd;
    cmd.command = obj.value("command").toString();
    cmd.delayAfterMs = obj.value("delayAfterMs").toInt(1000);
    cmd.runAsRoot = obj.value("runAsRoot").toBool(false);
    cmd.stopOnError = obj.value("stopOnError").toBool(true);
    return cmd;}

bool SequenceRunner::loadWorkflow(const QString &filePath, bool clearExisting) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit logMessage(QString("Cannot open file: %1").arg(filePath), "#F44336");
        return false;}
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    file.close();
    if (!doc.isArray()) {
        emit logMessage("Invalid JSON file: Root element is not an array.", "#F44336");
        return false;}
    if (clearExisting) {
        m_commands.clear();}    
    QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            m_commands.append(parseCommandFromJson(value.toObject()));}}    
    emit logMessage(QString("Loaded %1 commands. Total commands: %2.").arg(array.count()).arg(m_commands.count()), "#BDBDBD");
    return true;}

QStringList SequenceRunner::getCommandsAsText() const {
    QStringList result;
    for (const WorkflowCmd &cmd : m_commands) {
        QString line = cmd.command;        
        QString details;
        if (cmd.delayAfterMs > 0) {
            details += QString(" (Delay: %1ms)").arg(cmd.delayAfterMs);}
        if (cmd.runAsRoot) {
            details += " (ROOT)";}
        if (!details.isEmpty()) {
            line += details;}
        result.append(line);}
    return result;}

void SequenceRunner::startSequence() {
    if (m_isRunning) {
        emit logMessage("Sequence is already running.", "#FFAA66");
        return;}
    if (m_commands.isEmpty()) {
        emit logMessage("No commands loaded. Please load a workflow file.", "#F44336");
        return;}
    m_currentIndex = 0;
    m_isRunning = true;
    emit sequenceStarted();
    executeNextCommand();}

void SequenceRunner::stopSequence(bool forcedStop) {
    if (m_isRunning) {
        m_executor->stop();        
        finishSequence(false);        
        if (forcedStop) {
            emit logMessage("--- SEQUENCE FORCED STOP ---", "#F44336");}        
    } else if (forcedStop && m_isInterval) {
        m_isInterval = false;
        emit logMessage("Interval mode was OFF, but flag was reset to OFF.", "#BDBDBD");
    } else {
        emit logMessage("Sequence is not running.", "#BDBDBD");}}

void SequenceRunner::finishSequence(bool success) {
    m_isRunning = false; 
    m_delayTimer.stop();
    emit sequenceFinished(success);
    if (success) {
        emit logMessage("--- WORKFLOW SEQUENCE FINISHED SUCCESSFULLY ---", "#4CAF50");        
        if (m_isInterval) {
             emit logMessage(QString("Sequence finished. Scheduling restart in %1 seconds.").arg(m_intervalValueS), "#00BCD4");
             emit scheduleRestart(m_intervalValueS);
        } else {
             emit logMessage("Sequence finished. Interval mode OFF.", "#BDBDBD");}
    } else {
        if (m_isInterval) {
            m_isInterval = false;}
        emit logMessage("--- WORKFLOW SEQUENCE TERMINATED DUE TO ERROR ---", "#F44336");}}

void SequenceRunner::executeNextCommand() {
    if (!m_isRunning) return;
    if (m_currentIndex >= m_commands.count()) {
        finishSequence(true);
        return;}
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
        emit logMessage(QString(">>> user: %1").arg(currentCmd.command), "#FFE066");}
    m_executor->runSystemCommand(program, args);}

void SequenceRunner::onCommandFinished(int exitCode, QProcess::ExitStatus) {
    if (!m_isRunning) return;    
    const WorkflowCmd &currentCmd = m_commands.at(m_currentIndex);
    if (exitCode != 0 && currentCmd.stopOnError) {
        emit logMessage(QString("Workflow stopped: Command failed with code %1. (stopOnError is true)").arg(exitCode), "#F44336");
        finishSequence(false);
        return;}
    m_currentIndex++;    
    if (m_currentIndex < m_commands.count()) {
        if (currentCmd.delayAfterMs > 0) {
            emit logMessage(QString("Waiting for %1 ms before next command...").arg(currentCmd.delayAfterMs), "#FFC107");
            m_delayTimer.setInterval(currentCmd.delayAfterMs);
            m_delayTimer.start();
        } else {
            executeNextCommand();}
    } else {
        finishSequence(true);}}

void SequenceRunner::onDelayTimeout() {
    if (m_isRunning) {
        emit logMessage("Delay finished. Running next command.", "#00BCD4");
        executeNextCommand();}}

void SequenceRunner::setIntervalToggle(bool toggle) {
    m_isInterval = toggle;
    emit logMessage(QString("Interval mode set to: %1").arg(toggle ? "ON" : "OFF"), "#BDBDBD");}

void SequenceRunner::setIntervalValue(int seconds) {
    m_intervalValueS = seconds;
    emit logMessage(QString("Interval value set to: %1 seconds.").arg(seconds), "#BDBDBD");}
