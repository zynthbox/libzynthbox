#ifndef PROCESSWRAPPER_H
#define PROCESSWRAPPER_H

#include <QObject>
#include <QVariantMap>

class ProcessWrapper;
class ProcessWrapperTransaction : public QObject {
    Q_OBJECT
    Q_PROPERTY(TransactionState state READ state NOTIFY stateChanged)
    /**
     * \brief Whether to automatically release the transaction once completed
     * @note Setting this to true after the operation has completed will immediately release the transaction
     * @default false
     */
    Q_PROPERTY(bool autoRelease READ autoRelease WRITE setAutoRelease NOTIFY autoReleaseChanged)
public:
    explicit ProcessWrapperTransaction(const quint64 &transactionId, const QString &command, const QString &expectedEnd, ProcessWrapper *parent = nullptr);
    ~ProcessWrapperTransaction() override;

    enum TransactionState {
        ///@< The command has not yet been called and is waiting for its turn
        WaitingToStartState,
        ///@< The command is currently running, and the process is attempting to perform the requested action
        RunningState,
        ///@< The command has completed successfully (you would need to introspect the output to determine success or failure)
        CompletedState,
    };

    /**
     * \brief The transaction ID assigned to this by its creator
     * @note When created by ProcessWrapper, the IDs are assigned in sequential, increasing order,
     * meaning they can be used for comparison purposes for out-of-order instruction handling
     */
    Q_INVOKABLE const quint64 &transactionId() const;
    /**
     * \brief The command this transaction represents
     */
    Q_INVOKABLE const QString &command() const;
    /**
     * \brief The text to look for as the end of the command
     * This will usually be the command prompt set on the process, but might be any string
     */
    Q_INVOKABLE const QString &expectedEnd() const;

    /**
     * \brief The current state of this transaction
     * @return The current state of this transaction
     */
    Q_INVOKABLE TransactionState state() const;
    Q_SIGNAL void stateChanged() const;
    /**
     * \brief This will return when the given state is reached
     * @param state The transaction state you want to wait for (this is commonly the completed state, so that's our default)
     */
    Q_INVOKABLE void waitForState(const TransactionState &state = CompletedState) const;
    /**
     * \brief The output sent to standard output since the command was initiated
     */
    Q_INVOKABLE QString standardOutput() const;
    Q_SIGNAL void standardOutputChanged();
    /**
     * \brief The output sent to standard error since the command was initiated
     */
    Q_INVOKABLE QString standardError() const;
    Q_SIGNAL void standardErrorChanged();

    bool autoRelease() const;
    Q_INVOKABLE void setAutoRelease(const bool &autoRelease);
    Q_SIGNAL void autoReleaseChanged();
    /**
     * \brief Removes the object from the parent ProcessWrapper, and queues it for deletion
     */
    Q_INVOKABLE void release();

    enum StreamType {
        StandardOutputStream,
        StandardErrorStream,
    };
    /**
     * \brief Whether or not the current standard output contained by this transaction ends with the expected end
     * @return True if the current standard output data ends with the expected end
     */
    bool hasExpectedEnd(const StreamType &stream) const;
    /**
     * \brief Removes the function from the start of the standard output, and everything from the first occurrence of the expected end
     * @return The left over data from the end of the expected end and forward
     */
    QByteArray removeCommandPromptFromStandardOutput(const StreamType &stream) const;
protected:
    void setState(const TransactionState &state);
    void setStandardOutput(const QString &standardOut);
    void appendStandardOutput(const QByteArray &standardOut);
    void setStandardError(const QString &standardError);
    void appendStandardError(const QByteArray &standardError);
private:
    friend class ProcessWrapper;
    class Private;
    Private *d{nullptr};
};

/**
 * \brief A way to start, stop, and interact with external processes which have a call/output command-line style interface
 * @note As this uses the QProcess asynchronous API primarily, it requires a Qt event loop to be running (such as a QCoreApplication)
 *
 * Using the transaction based process handling is done by first setting the command prompt using setCommandPrompt(QString), which
 * will be the string that is used to detect when a command has completed. As the name implies, this essentially means that you will
 * be operating using a serial command prompt style interface, where each command is sent out when the command prompt is detected,
 * signalling the process is ready for more commands. If commands are sent before the process is ready, your instruction will be
 * queued up and sent to the process in the order of submission. You are not guaranteed that the commands will return in order, if
 * you somehow manage to send them from other threads.
 *
 * See also ProcessWrapperExample.py for an example of how to use the python bindings
 */
class ProcessWrapper : public QObject {
    Q_OBJECT
    /**
     * \brief What the current state of the process is (running, stopped, in the process of starting, or in the process of stopping)
     */
    Q_PROPERTY(ProcessState state READ state NOTIFY stateChanged)
    /**
     * \brief Whether the process will be automatically restarted on crashes
     * @default true
     */
    Q_PROPERTY(bool autoRestart READ autoRestart WRITE setAutoRestart NOTIFY autoRestartChanged)
    /**
     * \brief How many times the process will be restarted automatically before stopping
     * If the limit is reached, autoRestart will be set to false and the signal "autoRestartFailed" signal emitted
     * @default 10
     */
    Q_PROPERTY(int autoRestartLimit READ autoRestartLimit WRITE setAutoRestartLimit NOTIFY autoRestartLimitChanged)
    /**
     * \brief The number of times since the most recent explicit start call (or manual reset) the process has been restarted
     */
    Q_PROPERTY(int autoRestartCount READ autoRestartCount RESET resetAutoRestartCount NOTIFY autoRestartCountChanged)
    /**
     * \brief The QProcess instance used internally (in case you need to do something more fun with it)
     * @note Please don't delete this - technically you can, but yeah, don't do that please
     */
    Q_PROPERTY(QObject* internalProcess READ internalProcess NOTIFY internalProcessChanged)

    /**
     * \brief All standard output received since process start, however many complete lines fit into 1MiB, or at least one line
     */
    Q_PROPERTY(QString standardOutput READ standardOutput NOTIFY standardOutputChanged)
    /**
     * \brief All standard error output received since process start, however many complete lines fit into 1MiB, or at least one line
     */
    Q_PROPERTY(QString standardError READ standardError NOTIFY standardErrorChanged)

    /**
     * \brief A list of the most recent 10,000 transactions this object has been asked to initiate
     * We "only" keep track of 10,000 transactions and will ask transactions older than that to be
     * deleted. It is a number picked essentially out thin air, and if we discover something more
     * clever is required, we can do that as well, but this should do us to ensure we both have a
     * decent number of potential commands asked of us, without losing results too fast, and also
     * not just filling up the memory with ancient command nobody needs any longer.
     * @note You can manually clear out a transaction once you're done with its data using transaction->release()
     */
    Q_PROPERTY(QList<QObject*> transactions READ transactions NOTIFY transactionsChanged)
public:
    /**
     * \brief Constructs a new ProcessWrapper instance
     * @see setCommandPrompt(QString)
     * @return A ProcessWrapper instance
     */
    explicit ProcessWrapper(QObject *parent = nullptr);
    ~ProcessWrapper() override;

    /**
     * \brief Start a new process with the given executable, with the optional parameters sent along
     * @note If there is another process already active, it will be unceremoniously killed before
     * launching the new one. If you need a graceful shutdown, call stop first with a long timeout to
     * ensure this happens.
     * @param executable An executable as QProcess understands it
     * @param parameters Optionally, parameters to be sent along to the executable (also as QProcess understands it)
     * @param environment Optionally, the environment variables that this process should be given (if none are passed, or an empty list is, the current process environment is inherited)
     * @return A transaction which, when marked as Completed, indicates that the process has been started successfully
     */
    Q_INVOKABLE ProcessWrapperTransaction *start(const QString& executable, const QStringList &parameters = {}, const QVariantMap &environment = {});

    /**
     * \brief Stops the process, and will kill the process if it takes too long to shut down
     * @note To wait forever, set the timeout to very, very long
     * @param timeout The number of milliseconds to wait for the process to shut down
     */
    Q_INVOKABLE void stop(const int &timeout = 1000);

    /**
     * \brief This sets the command prompt used by the transaction based functionality to perform its operations
     * A command is considered completed when the command prompt is encountered in standard output
     * @param commandPrompt The command prompt string used to determine when a command has completed
     */
    Q_INVOKABLE void setCommandPrompt(const QString &commandPrompt);
    /**
     * \brief Starts the "function" command, and returns the transaction object once completed
     * @note This function cal return a nullptr, if the process is not running
     * @param function The command or instruction to send to the process
     * @param expectedEnd If set, we will use this string as the end of the command's output, instead of the command prompt
     * @param timeout How long to wait before forcing a return, in milliseconds (-1 being infinite)
     * @return The transaction for the call (if we returned before completion due to a timeout, the transaction object will be updated once the function does complete)
     */
    Q_INVOKABLE ProcessWrapperTransaction *call(const QString &function, const QString &expectedEnd = {}, const int timeout = -1);
    /**
     * \brief Starts the "function" command, and returns the transaction object immediately
     * @note This function cal return a nullptr, if the process is not running
     * @param function The command or instruction to send to the process
     * @param expectedEnd If set, we will use this string as the end of the command's output, instead of the command prompt
     * @return The transaction for the call (the transaction call may be in any state upon returning)
     */
    Q_INVOKABLE ProcessWrapperTransaction *send(const QString &function, const QString &expectedEnd = {});

    QList<QObject*> transactions() const;
    Q_SIGNAL void transactionsChanged();
    /**
     * \brief Removes the given transaction from the transactions list and marks it for deletion
     */
    void releaseTransaction(ProcessWrapperTransaction* transaction);

    QString standardOutput() const;
    QString standardError() const;

    // Ouch not cool hack: https://forum.qt.io/topic/130255/shiboken-signals-don-t-work
// Core message (by vberlier): Turns out Shiboken shouldn't do anything for signals and let PySide setup the signals using the MOC data. Shiboken generates bindings for signals as if they were plain methods and shadows the actual signals.
#ifndef PYSIDE_BINDINGS_H
    /**
     * \brief Emitted when there is any output written to standard output by the process
     * @param output All output sent by the process since the most recent call to send() or call()
     */
    Q_SIGNAL void standardOutputChanged(const QString &output);
    /**
     * \brief Emitted when there is any output written to standard error by the process
     * @param output The output sent by the process since the most recent call to send() or call()
     */
    Q_SIGNAL void standardErrorChanged(const QString &output);
    /**
     * \brief Emitted when output is written to standard output by the process
     * @param output The output received from the process
     */
    Q_SIGNAL void standardOutputReceived(const QByteArray &output);
    /**
     * \brief Emitted when output is written to standard error by the process
     * @param output The output received from the process
     */
    Q_SIGNAL void standardErrorReceived(const QByteArray &output);
#endif

    enum ProcessState {
        NotRunningState,
        StartingState,
        RunningState,
        StoppingState,
    };
    Q_ENUM(ProcessState)

    ProcessState state() const;
    Q_SIGNAL void stateChanged();

    /**
     * \brief Emitted when the automatic restart has failed too many times
     * @param description A geek-readable description of how the failure occurred, not for UI consumption
     * @see autoRestartLimit
     */
    Q_SIGNAL void autoRestartFailed(const QString &description);

    bool autoRestart() const;
    void setAutoRestart(const bool &autoRestart);
    Q_SIGNAL void autoRestartChanged();

    bool autoRestartLimit() const;
    void setAutoRestartLimit(const int &autoRestartLimit);
    Q_SIGNAL void autoRestartLimitChanged();

    int autoRestartCount() const;
    void resetAutoRestartCount();
    Q_SIGNAL void autoRestartCountChanged();

    QObject* internalProcess() const;
    Q_SIGNAL void internalProcessChanged();
private:
    class Private;
    Private *d{nullptr};
};
Q_DECLARE_METATYPE(ProcessWrapper::ProcessState)

#endif//PROCESSWRAPPER_H
