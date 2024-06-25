#pragma once

#include <QObject>

/**
 * \brief A way to start, stop, and interact with external processes which have a call/output command-line style interface
 * @note As this uses the QProcess asynchronous API primarily, it requires a Qt event loop to be running (such as a QCoreApplication)
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
     * \brief The standard output received after the most recent call to call() or send()
     */
    Q_PROPERTY(QString standardOutput READ standardOutput NOTIFY standardOutputChanged)
    /**
     * \brief The standard error output received after the most recent call to call() or send()
     */
    Q_PROPERTY(QString standardError READ standardError NOTIFY standardErrorChanged)
public:
    explicit ProcessWrapper(QObject *parent = nullptr);
    ~ProcessWrapper() override;

    /**
     * \brief Start a new process with the given executable, with the optional parameters sent along
     * @note If there is another process already active, it will be unceremoniously killed before
     * launching the new one. If you need a graceful shutdown, call stop first with a long timeout to
     * ensure this happens.
     * @param executable An executable as QProcess understands it
     * @param parameters Optionally, parameters to be sent along to the executable (also as QProcess understands it)
     */
    Q_INVOKABLE void start(const QString& executable, const QStringList &parameters = {});

    /**
     * \brief Stops the process, and will kill the process if it takes too long to shut down
     * @note To wait forever, set the timeout to very, very long
     * @param timeout The number of milliseconds to wait for the process to shut down
     */
    Q_INVOKABLE void stop(const int &timeout = 1000);

    /**
     * \brief Send an instruction to the process, and block until the function returns some data, or optionally until a given timeout and/or expected output
     * @param function The instruction to send to the process
     * @param expectedOutput Some output to wait for (a regular expression)
     * @param timeout The amount of time to wait in milliseconds
     * @see waitForOutput(QString, int)
     * @return The resulting output from that call
     */
    Q_INVOKABLE QString call(const QByteArray &function, const QString &expectedOutput = {}, const int timeout = -1);
    /**
     * \brief Send some data to the process in a non-blocking manner
     * @param data The data to send to the process
     */
    Q_INVOKABLE void send(const QByteArray &data);
    /**
     * \brief Helper funcation which is same as ProcessWrapper::send but expects data argument to be QString
     * @param data The data to send to the process
     */
    Q_INVOKABLE void send(const QString data);
    /**
     * \brief Helper funcation which is same as ProcessWrapper::send but expects data argument to be QString and appends a newline
     * @param data The data to send to the process
     */
    Q_INVOKABLE void sendLine(QString data);

    enum WaitForOutputResult {
        WaitForOutputSuccess,
        WaitForOutputFailure,
        WaitForOutputTimeout
    };
    Q_ENUM(WaitForOutputResult)
    /**
     * \brief Wait for standard output to contain the given expected output
     * @param expectedOutput The output you expect (a regular expression)
     * @param timeout The amount of time to wait in milliseconds
     * @return What the outcome of the function call was (success, failure, or timeout)
     */
    Q_INVOKABLE WaitForOutputResult waitForOutput(const QString &expectedOutput, const int timeout = -1);

    QString standardOutput() const;
    QString standardError() const;

    // Ouch not cool hack: https://forum.qt.io/topic/130255/shiboken-signals-don-t-work
// Core message (by vberlier): Turns out Shiboken shouldn't do anything for signals and let PySide setup the signals using the MOC data. Shiboken generates bindings for signals as if they were plain methods and shadows the actual signals.
#ifndef PYSIDE_BINDINGS_H
    /**
     * \brief Emitted when there is any output written to standard output by the process
     * @param output All output sent by the process since the most recent call to clearStandardOutput()
     */
    Q_SIGNAL void standardOutputChanged(const QString &output);
    /**
     * \brief Emitted when there is any output written to standard error by the process
     * @param output The output sent by the process
     */
    Q_SIGNAL void standardErrorChanged(const QString &output);
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
