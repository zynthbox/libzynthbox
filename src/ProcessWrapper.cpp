#include "ProcessWrapper.h"

#include <QDebug>
#include <QProcess>
#include <QTimer>

class ProcessWrapper::Private {
public:
    Private(ProcessWrapper *q)
        : q(q)
    {
        processKiller.setSingleShot(true);
        processKiller.callOnTimeout([this](){
            if (process) {
                process->kill();
            }
        });
    }
    ProcessWrapper *q{nullptr};
    QString executable;
    QStringList parameters;
    bool autoRestart{true};
    int autoRestartLimit{10};
    int autoRestartCount{0};
    bool performRestart{false}; // Set to the value of autoRestart when start() is called, and explicitly to false when stop() is called
    ProcessState state{NotRunningState};
    QProcess *process{nullptr};
    QTimer processKiller;
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
        processKiller.stop();
        process->deleteLater();
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
    QString standardError;
    void handleReadyReadError() {
        if (process) {
            standardError = QString::fromLocal8Bit(process->readAllStandardError());
            Q_EMIT q->standardError(standardError);
        }
    }
    QString standardOutput;
    void handleReadyReadOutput() {
        if (process) {
            standardOutput = QString::fromLocal8Bit(process->readAllStandardOutput());
            Q_EMIT q->standardOutput(standardOutput);
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
    delete d;
}

void ProcessWrapper::start(const QString& executable, const QStringList& parameters)
{
    if (d->process) {
        stop(0); // If we've already got a process going on, let's ensure that it's shut down (not gracefully, as documented, but immediately)
    }
    d->state = StartingState;
    d->process = new QProcess(this);
    resetAutoRestartCount();
    d->performRestart = d->autoRestart;
    connect(d->process, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError &error){ d->handleError(error); });
    connect(d->process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](const int &exitCode, const QProcess::ExitStatus &exitStatus){ d->handleFinished(exitCode, exitStatus); });
    connect(d->process, &QProcess::stateChanged, this, [this](const QProcess::ProcessState &newState){ d->handleStateChange(newState); });
    connect(d->process, &QProcess::readyReadStandardOutput, this, [this](){ d->handleReadyReadOutput(); });
    connect(d->process, &QProcess::readyReadStandardError, this, [this](){ d->handleReadyReadError(); });
    d->executable = executable;
    d->parameters = parameters;
    d->process->setProgram(executable);
    d->process->setArguments(parameters);
    d->process->startDetached();
}

void ProcessWrapper::stop(const int& timeout)
{
    if (d->process) {
        d->state = StoppingState;
        Q_EMIT stateChanged();
        // TODO Request stop and then handle that happening
        d->performRestart = false;
        d->process->terminate();
        d->processKiller.start(timeout);
    }
}

QString ProcessWrapper::call(const QByteArray& function)
{
    if (d->process) {
        d->process->write(function);
        qDebug() << Q_FUNC_INFO << "Wrote" << function << "and now waiting for that to be acknowledged";
        d->process->waitForBytesWritten();
        qDebug() << Q_FUNC_INFO << "Waited for the function to be written, now waiting for ready read";
        d->process->waitForReadyRead();
        qDebug() << Q_FUNC_INFO << "Waited for ready read and now have the following standard output:" << d->standardOutput;
        return d->standardOutput;
    }
    return {};
}

void ProcessWrapper::send(const QByteArray& data)
{
    if (d->process) {
        d->process->write(data);
    }
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
