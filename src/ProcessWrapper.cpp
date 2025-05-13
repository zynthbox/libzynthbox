#include "ProcessWrapper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QQueue>
#include <QRegularExpression>

#include <kptydevice.h>
#include <kptyprocess.h>

class ProcessWrapperTransaction::Private {
public:
    Private(ProcessWrapperTransaction *q)
        : q(q)
    {}
    ProcessWrapper *processWrapper{nullptr};
    ProcessWrapperTransaction* q{nullptr};
    quint64 transactionId{0};
    QString command;
    TransactionState state{WaitingToStartState};
    QByteArray standardOut;
    QByteArray standardError;
    bool autoRelease{false};
};

ProcessWrapperTransaction::ProcessWrapperTransaction(const quint64& transactionId, const QString& command, ProcessWrapper* parent)
    : QObject(parent)
    , d(new Private(this))
{
    d->processWrapper = parent;
    d->transactionId = transactionId;
    d->command = command;
}

ProcessWrapperTransaction::~ProcessWrapperTransaction()
{
    delete d;
}

const quint64 & ProcessWrapperTransaction::transactionId() const
{
    return d->transactionId;
}

const QString & ProcessWrapperTransaction::command() const
{
    return d->command;
}

ProcessWrapperTransaction::TransactionState ProcessWrapperTransaction::state() const
{
    return d->state;
}

void ProcessWrapperTransaction::setState(const TransactionState& state)
{
    if (d->state != state) {
        d->state = state;
        Q_EMIT stateChanged();
    }
}

void ProcessWrapperTransaction::waitForState(const TransactionState& state) const
{
    while (d->state != state) {
        qApp->processEvents(QEventLoop::AllEvents, 10);
    }
}

QString ProcessWrapperTransaction::standardOutput() const
{
    return d->standardOut;
}

void ProcessWrapperTransaction::setStandardOutput(const QString& standardOut)
{
    d->standardOut = standardOut.toUtf8();
    Q_EMIT standardOutputChanged();
}

void ProcessWrapperTransaction::appendStandardOutput(const QByteArray& standardOut)
{
    d->standardOut.append(standardOut);
    Q_EMIT standardOutputChanged();
}

QString ProcessWrapperTransaction::standardError() const
{
    return d->standardError;
}

void ProcessWrapperTransaction::setStandardError(const QString& standardError)
{
    d->standardError = standardError.toUtf8();
    Q_EMIT standardErrorChanged();
}

void ProcessWrapperTransaction::appendStandardError(const QByteArray& standardError)
{
    d->standardError.append(standardError);
    Q_EMIT standardErrorChanged();
}

bool ProcessWrapperTransaction::autoRelease() const
{
    return d->autoRelease;
}

void ProcessWrapperTransaction::setAutoRelease(const bool& autoRelease)
{
    if (d->autoRelease != autoRelease) {
        d->autoRelease = autoRelease;
        Q_EMIT autoReleaseChanged();
        if (d->state == CompletedState) {
            release();
        }
    }
}

void ProcessWrapperTransaction::release()
{
    d->processWrapper->releaseTransaction(this);
}

bool ProcessWrapperTransaction::hasCommandPrompt(const QString& commandPrompt) const
{
    return d->standardOut.contains(commandPrompt.toUtf8());
}

QByteArray ProcessWrapperTransaction::removeCommandPromptFromStandardOutput(const QString& commandPrompt) const
{
    const int commandPromptStart{d->standardOut.indexOf(commandPrompt.toUtf8())};
    // qDebug() << Q_FUNC_INFO << "Looking for" << commandPrompt << "which is supposedly at index" << commandPromptStart;
    QByteArray leftovers{d->standardOut.mid(commandPromptStart + commandPrompt.length())};
    d->standardOut.truncate(commandPromptStart - 1);
    d->standardOut = d->standardOut.mid(d->command.length());
    return leftovers;
}

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
                    // If we've got any transactions waiting to start, let's sort that out
                    if (currentTransaction && currentTransaction->state() == ProcessWrapperTransaction::WaitingToStartState) {
                        startTransaction(currentTransaction);
                    }
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

    quint64 nextTransactionId{0};
    QList<ProcessWrapperTransaction*> transactions;
    QList<QObject*> transactionsObjects;
    QList<ProcessWrapperTransaction*> transactionsToRelease;
    QQueue<ProcessWrapperTransaction*> waitingTransactions;
    ProcessWrapperTransaction* currentTransaction{nullptr};
    QString commandPrompt;

    void startTransaction(ProcessWrapperTransaction *transaction) {
        const QString &function{transaction->command()};
        transaction->setState(ProcessWrapperTransaction::RunningState);
        // qDebug() << Q_FUNC_INFO << "Starting transaction for function" << function;
        if (function == QLatin1String{"<initial startup>"}) {
            // This is our start command - don't do anything with that
        } else if (function.endsWith("\n")) {
            if (process->pty()->write(function.toUtf8()) == -1) {
                qWarning() << Q_FUNC_INFO << "Error occurred while writing function:" << function;
            }
        } else {
            if (process->pty()->write(QString("%1\n").arg(function).toUtf8()) == -1) {
                qWarning() << Q_FUNC_INFO << "Error occurred while writing function (with added newline):" << function;
            }
        }
    }
    ProcessWrapperTransaction *createTransaction(const QString &function) {
        ProcessWrapperTransaction *transaction = new ProcessWrapperTransaction(nextTransactionId, function, q);
        ++nextTransactionId;
        transactions << transaction;
        transactionsObjects << transaction;
        if (currentTransaction == nullptr) {
            currentTransaction = transaction;
            if (state == ProcessWrapper::RunningState) {
                // If we actually have a running process, and our new transaction is the current one, actually send the command there
                startTransaction(transaction);
            }
        } else {
            waitingTransactions.enqueue(transaction);
        }
        // Clean up after extremely long durations, only hanging on to the most recent 10k transactions
        while (transactions.count() > 10000) {
            ProcessWrapperTransaction *removeThisTransaction = transactions.takeFirst();
            removeThisTransaction->release();
        }
        return transaction;
    }

    QByteArray standardError;
    void handleReadyReadError() {
        if (process) {
            const QByteArray newData{process->readAllStandardError()};
            if (newData.isEmpty() == false) {
                if (currentTransaction) {
                    currentTransaction->appendStandardError(newData);
                }
                standardError.append(newData);
                // Ensure we only keep some reasonably large amount of global scrollback (a slightly odd logic here: chop at linebreaks, but ensure we keep up to 1MiB around, or at least one full line of output, if it's super crazy long)
                while (standardError.size() > 1048576) {
                    const int firstNewline{standardError.indexOf("\n")};
                    if (firstNewline == -1) {
                        // Just to be certain if we've got some kind of bonkers output that has 1048576 bytes on a single line
                        break;
                    }
                    standardError.remove(0, firstNewline + 1);
                }
                Q_EMIT q->standardErrorChanged(standardError);
                Q_EMIT q->standardErrorReceived(newData);
            }
        }
    }
    QByteArray standardOutput;
    void handleReadyReadOutput() {
        if (process) {
            while (true) {
                const QByteArray newData{process->pty()->read(1024)};
                if (newData.isEmpty()) {
                    // If there is no more data to read, don't try and keep going
                    break;
                } else {
                    if (currentTransaction) {
                        currentTransaction->appendStandardOutput(newData);
                        if (currentTransaction->hasCommandPrompt(commandPrompt)) {
                            // This means we've reached the end of a command, and the process is ready for its next input
                            // Consequently, we mark the current head command as completed
                            currentTransaction->setState(ProcessWrapperTransaction::CompletedState);
                            // Truncate the output at the position of the command prompt (as we don't want to include that in the output)
                            // If there's any leftovers, for now we just warn that out, but perhaps it wants to live on the transaction?
                            QByteArray leftovers = currentTransaction->removeCommandPromptFromStandardOutput(commandPrompt);
                            if (leftovers.isEmpty() == false) {
                                qWarning() << Q_FUNC_INFO << "Apparently we have more stuff, even though we've not asked for more?" << leftovers;
                            }
                            if (currentTransaction->autoRelease()) {
                                currentTransaction->release();
                            }
                            if (transactionsToRelease.contains(currentTransaction)) {
                                // If this transaction was marked for release, ensure that actually happens now that we're done with it
                                transactionsToRelease.removeAll(currentTransaction);
                                currentTransaction->release();
                            }
                            if (waitingTransactions.isEmpty()) {
                                // The queue is empty, so we don't have a current transaction
                                currentTransaction = nullptr;
                            } else {
                                // Pick the next transaction in the queue, if there is one, and start it
                                currentTransaction = waitingTransactions.dequeue();
                                startTransaction(currentTransaction);
                            }
                        }
                    }
                    // Finally, append to the existing standard output list, and emit the relevant signals
                    standardOutput.append(newData);
                    // Ensure we only keep some reasonably large amount of global scrollback (a slightly odd logic here: chop at linebreaks, but ensure we keep up to 1MiB around, or at least one full line of output, if it's super crazy long)
                    while (standardOutput.size() > 1048576) {
                        const int firstNewline{standardOutput.indexOf("\n")};
                        if (firstNewline == -1) {
                            // Just to be certain if we've got some kind of bonkers output that has 1,048,576 bytes on a single line
                            break;
                        }
                        standardOutput.remove(0, firstNewline + 1);
                    }
                    // qDebug() << Q_FUNC_INFO << executable << parameters << "\n" << QString(standardOutput);
                    Q_EMIT q->standardOutputChanged(standardOutput);
                    Q_EMIT q->standardOutputReceived(newData);
                }
                qApp->processEvents(QEventLoop::AllEvents, 10);
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

ProcessWrapperTransaction * ProcessWrapper::start(const QString& executable, const QStringList& parameters, const QVariantMap &environment)
{
    if (d->process) {
        stop(0); // If we've already got a process going on, let's ensure that it's shut down (not gracefully, as documented, but immediately)
    }
    d->state = StartingState;
    d->process = new KPtyProcess(this);
    d->process->setOutputChannelMode(KPtyProcess::OnlyStderrChannel);
    d->process->setCurrentReadChannel(QProcess::StandardError);
    d->process->setPtyChannels(KPtyProcess::StdinChannel | KPtyProcess::StdoutChannel);
    d->process->pty()->setEcho(true); // We need to echo the command, otherwise our logic for detecting command line prompts etc ends up not working correctly
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
    if (environment.isEmpty() == false) {
        QProcessEnvironment construct;
        for (auto it = environment.keyValueBegin(); it != environment.keyValueEnd(); ++it) {
            construct.insert(it->first, it->second.toString());
        }
        d->process->setProcessEnvironment(construct);
    }
    ProcessWrapperTransaction *initTransaction = d->createTransaction("<initial startup>");
    d->process->start();
    return initTransaction;
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
        d->process->deleteLater();
        d->process = nullptr;
        d->standardOutput = {};
        Q_EMIT standardOutputChanged(d->standardOutput);
        d->standardError = {};
        Q_EMIT standardErrorChanged(d->standardError);
    }
}

void ProcessWrapper::setCommandPrompt(const QString& commandPrompt)
{
    d->commandPrompt = commandPrompt;
}

ProcessWrapperTransaction * ProcessWrapper::call(const QString& function, const int timeout)
{
    ProcessWrapperTransaction* transaction{nullptr};
    if (d->commandPrompt.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "You did not set a command prompt before attempting to call the function" << function;
    } else {
        if (d->process) {
            transaction = d->createTransaction(function);
            qint64 startTime = QDateTime::currentMSecsSinceEpoch();
            while (transaction->state() != ProcessWrapperTransaction::CompletedState) {
                if (timeout > -1 && (QDateTime::currentMSecsSinceEpoch() - startTime) > timeout) {
                    break;
                }
                qApp->processEvents(QEventLoop::AllEvents, 10);
            }
        }
    }
    return transaction;
}

ProcessWrapperTransaction * ProcessWrapper::send(const QString& function)
{
    ProcessWrapperTransaction* transaction{nullptr};
    if (d->commandPrompt.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "You did not set a command prompt before attempting to send the instruction" << function;
    } else {
        if (d->process) {
            transaction = d->createTransaction(function);
        }
    }
    return transaction;
}

QList<QObject *> ProcessWrapper::transactions() const
{
    return d->transactionsObjects;
}

void ProcessWrapper::releaseTransaction(ProcessWrapperTransaction* transaction)
{
    if (transaction->state() == ProcessWrapperTransaction::CompletedState) {
        d->transactions.removeAll(transaction);
        d->transactionsObjects.removeAll(transaction);
        transaction->deleteLater();
    } else {
        d->transactionsToRelease << transaction;
    }
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
