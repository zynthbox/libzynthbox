#include "ProcessWrapper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

#include <kptydevice.h>
#include <kptyprocess.h>

class ProcessWrapper::Private {
public:
    Private(ProcessWrapper *q)
        : q(q)
    {
    }
    ProcessWrapper *q{nullptr};
    QString executable;
    QStringList parameters;
    bool autoRestart{true};
    int autoRestartLimit{10};
    int autoRestartCount{0};
    bool performRestart{false}; // Set to the value of autoRestart when start() is called, and explicitly to false when stop() is called
    ProcessState state{NotRunningState};
    KPtyProcess *process{nullptr};
    void handleStateChange(const QProcess::ProcessState &newState) {
        ProcessWrapper::ProcessState updatedState{RunningState};
        if (process) {
            switch(newState) {
                case QProcess::NotRunning:
                    updatedState = ProcessWrapper::NotRunningState;
                    break;
                case QProcess::Starting:
                    updatedState = ProcessWrapper::StartingState;
                    break;
                case QProcess::Running:
                    updatedState = ProcessWrapper::RunningState;
                    break;
                default:
                    break;
            }
        } else {
            updatedState = ProcessWrapper::NotRunningState;
        }
        if (state != updatedState) {
            state = updatedState;
            Q_EMIT q->stateChanged();
        }
    }
    void handleFinished(const int &/*exitCode*/, const QProcess::ExitStatus &exitStatus) {
        // qDebug() << Q_FUNC_INFO << "Process has exited";
        process = nullptr;
        state = ProcessWrapper::NotRunningState;
        Q_EMIT q->stateChanged();
        if (exitStatus == QProcess::CrashExit) {
            if (performRestart && autoRestartCount < autoRestartLimit) {
                ++autoRestartCount;
                q->start(executable, parameters);
            }
        }
    }
    void handleError(const QProcess::ProcessError &error) {
        qDebug() << Q_FUNC_INFO << process << "reported error" << error;
    }

    bool blockingCallInProgress{false};
    bool dataReceivedAfterBlockingWrite{true};
    QString standardError;
    void handleReadyReadError() {
        if (process) {
            const QString newData{QString::fromLocal8Bit(process->readAllStandardError())};
            if (newData.isEmpty() == false) {
                standardError.append(newData);
                dataReceivedAfterBlockingWrite = true;
                Q_EMIT q->standardErrorChanged(standardError);
                Q_EMIT q->standardErrorReceived(newData);
            }
        }
    }
    QString standardOutput;
    void handleReadyReadOutput() {
        if (process) {
            const QString newData{QString::fromLocal8Bit(process->pty()->readAll())};
            if (newData.isEmpty() == false) {
                standardOutput.append(newData);
                dataReceivedAfterBlockingWrite = true;
                Q_EMIT q->standardOutputChanged(standardOutput);
                Q_EMIT q->standardOutputReceived(newData);
            }
        }
    }
};

ProcessWrapper::ProcessWrapper(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

ProcessWrapper::~ProcessWrapper()
{
    if (d->process) {
        stop(100);
    }
    delete d;
}

void ProcessWrapper::start(const QString& executable, const QStringList& parameters)
{
    if (d->process) {
        stop(0); // If we've already got a process going on, let's ensure that it's shut down (not gracefully, as documented, but immediately)
    }
    d->state = StartingState;
    d->process = new KPtyProcess(this);
    d->process->setOutputChannelMode(KPtyProcess::OnlyStderrChannel);
    d->process->setPtyChannels(KPtyProcess::StdinChannel | KPtyProcess::StdoutChannel);
    d->process->pty()->setEcho(false);
    resetAutoRestartCount();
    d->performRestart = d->autoRestart;
    connect(d->process, &KPtyProcess::errorOccurred, this, [this](const QProcess::ProcessError &error){ d->handleError(error); });
    connect(d->process, QOverload<int, QProcess::ExitStatus>::of(&KPtyProcess::finished), this, [this](const int &exitCode, const QProcess::ExitStatus &exitStatus){ d->handleFinished(exitCode, exitStatus); });
    connect(d->process, &QProcess::stateChanged, this, [this](const QProcess::ProcessState &newState){ d->handleStateChange(newState); });
    connect(d->process->pty(), &KPtyDevice::readyRead, this, [this]() { d->handleReadyReadOutput(); });
    connect(d->process, &KPtyProcess::readyReadStandardError, this, [this](){ d->handleReadyReadError(); });
    d->executable = executable;
    d->parameters = parameters;
    d->process->setProgram(executable, parameters);
    d->process->setNextOpenMode(QIODevice::ReadWrite | QIODevice::Unbuffered);
    d->process->start();
}

void ProcessWrapper::stop(const int& timeout)
{
    if (d->process) {
        d->state = StoppingState;
        Q_EMIT stateChanged();
        d->performRestart = false;
        d->process->terminate();
        if (d->process->waitForFinished(timeout) == false) {
            d->process->kill();
            if (d->process->waitForFinished(timeout) == false) {
                qDebug() << Q_FUNC_INFO << "Failed to shut down" << d->process << d->parameters << "within" << timeout << "milliseconds";
            }
        }
    }
}

QString ProcessWrapper::call(const QByteArray& function, const QString &expectedOutput, const int timeout)
{
    if (d->process) {
        d->blockingCallInProgress = true;
        d->dataReceivedAfterBlockingWrite = false;
        d->standardOutput = QString("\n");
        Q_EMIT standardOutputChanged(d->standardOutput);
        d->standardError = QString("\n");
        Q_EMIT standardErrorChanged(d->standardError);
        // Not emitting the received signals (as nothing has been received yet...)
        // qDebug() << Q_FUNC_INFO << "Writing" << function << "to the process";
        d->process->pty()->write(function);
        // qDebug() << Q_FUNC_INFO << "Write completed, now waiting for that to be acknowledged";
        // d->process->waitForBytesWritten();
        // can't use waitForReadyRead as we're capturing the data elsewhere which breaks that call
        if (expectedOutput.isEmpty()) {
            // qDebug() << Q_FUNC_INFO << "Function was written, now waiting for output or the timeout" << timeout;
            qint64 startTime = QDateTime::currentMSecsSinceEpoch();
            while (d->dataReceivedAfterBlockingWrite == false) {
                if (timeout > -1 && (QDateTime::currentMSecsSinceEpoch() - startTime) > timeout) {
                    break;
                }
                qApp->processEvents();
            }
        } else {
            // qDebug() << Q_FUNC_INFO << "Function was written, now waiting the output" << expectedOutput << "or the timeout" << timeout;
            WaitForOutputResult result = waitForOutput(expectedOutput, timeout);
            Q_UNUSED(result)
            // qDebug() << Q_FUNC_INFO << "Waited for the output" << expectedOutput << "for" << timeout << "milliseconds, with the result being" << result;
        }
        d->dataReceivedAfterBlockingWrite = true; // just in case we've bailed out
        // qDebug() << Q_FUNC_INFO << "Waited (or timed out) and now have the following standard output:\n" << d->standardOutput;
        // qDebug() << Q_FUNC_INFO << "And the following standard error:\n" << d->standardError;
        d->blockingCallInProgress = false;
        return d->standardOutput;
    }
    return {};
}

void ProcessWrapper::send(const QByteArray& data)
{
    if (d->process) {
        d->standardOutput = QString("\n");
        Q_EMIT standardOutputChanged(d->standardOutput);
        d->standardError = QString("\n");
        Q_EMIT standardOutputChanged(d->standardOutput);
        // Not emitting the received signals (as nothing has been received yet...)
        d->process->pty()->write(data);
    }
}

void ProcessWrapper::send(const QString data)
{
    send(data.toUtf8());
}

void ProcessWrapper::sendLine(QString data)
{
    if (!data.endsWith("\n")) {
        data += "\n";
    }
    send(data);
}

ProcessWrapper::WaitForOutputResult ProcessWrapper::waitForOutput(const QString& expectedOutput, const int timeout)
{
    WaitForOutputResult result{ProcessWrapper::WaitForOutputFailure};
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    QRegularExpression regularExpectedOutput{expectedOutput};
    while (true) {
        d->handleReadyReadError();
        d->handleReadyReadOutput();
        if (timeout > -1 && (QDateTime::currentMSecsSinceEpoch() - startTime) > timeout) {
            result = ProcessWrapper::WaitForOutputTimeout;
            break;
        }
        QRegularExpressionMatch match = regularExpectedOutput.match(d->standardOutput);
        if (match.hasMatch()) {
            result = ProcessWrapper::WaitForOutputSuccess;
            break;
        }
        qApp->processEvents();
    }
    return result;
}

QString ProcessWrapper::standardOutput() const
{
    return d->standardOutput;
}

QString ProcessWrapper::standardError() const
{
    return d->standardError;
}

ProcessWrapper::ProcessState ProcessWrapper::state() const
{
    return d->state;
}


bool ProcessWrapper::autoRestart() const
{
    return d->autoRestart;
}

void ProcessWrapper::setAutoRestart(const bool& autoRestart)
{
    if (d->autoRestart != autoRestart) {
        d->autoRestart = autoRestart;
        Q_EMIT autoRestartChanged();
    }
}

bool ProcessWrapper::autoRestartLimit() const
{
    return d->autoRestartLimit;
}

void ProcessWrapper::setAutoRestartLimit(const int& autoRestartLimit)
{
    if (d->autoRestartLimit != autoRestartLimit) {
        d->autoRestartLimit = autoRestartLimit;
        Q_EMIT autoRestartLimitChanged();
    }
}

int ProcessWrapper::autoRestartCount() const
{
    return d->autoRestartCount;
}

void ProcessWrapper::resetAutoRestartCount()
{
    if (d->autoRestartCount > 0) {
        d->autoRestartCount = 0;
        Q_EMIT autoRestartCountChanged();
    }
}

QObject * ProcessWrapper::internalProcess() const
{
    return d->process;
}
