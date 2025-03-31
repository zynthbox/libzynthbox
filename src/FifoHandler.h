#pragma once

#include <QObject>

class FifoHandlerPrivate;
/**
 * \brief A wrapper for access to a fifo file object
 * On construction, you pass a fifo object (which must exist), and
 * the direction of the communication (either reading from, or writing
 * to the fifo).
 *
 * For reader direction instances, you will need to manually call the
 * start() function to actually begin reading incoming data. This is
 * to allow you to set up the signal handler for received(QString), to
 * ensure you don't end up potentially missing some initial data.
 *
 * Once constructed, you can send text to the fifo via the send(QString)
 * slot for that purpose.
 *
 * Once started, the reader and writer hold the file open until the
 * instance is deleted again.
 *
 * Note that this class is not exposed to QML. This could potentially
 * be done later on, but for now, it's not intended that this should
 * be used for UI-level scripting.
 */
class FifoHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filepath READ filepath CONSTANT)
public:
    enum Direction {
        ReadingDirection, ///< Setting this will cause FifoHandler to read from the given fifo file (see received(QString))
        WritingDirection, ///< Setting this will cause FifoHandler to write to the given fifo file (see send(QString))
    };
    /**
     * \brief Constructs a new FifoHandler for the given file path and direction
     * @param filepath The fifo (which must already exist) to handle
     * @param direction The direction of communication to handle
     */
    explicit FifoHandler(const QString &filepath, const Direction &direction, QObject *parent = nullptr);
    ~FifoHandler() override;

    /**
     * \brief Sends the given string to the fifo
     * @note To avoid any possible confusion: This will only do anything for WritingDirection instances
     * If the fifo did not exist on startup, calling this function will attempt to start the behind-the-scenes thread first.
     * If the fifo does not exist upon calling this function, the data will be sent out on the next call to send
     * @param data The string to write to the fifo
     * @param autoAppendNewline Set this to false to not automatically append a newline to the data if there isn't one already
     */
    Q_SLOT void send(const QString &data, const bool &autoAppendNewline = true);

    /**
     * \brief Start the behind-the-scenes thread
     * This is required for readers, but if the fifo is not created until after startup, you can also call this once the fifo has been created
     */
    void start();

    /**
     * \brief Emitted once a newline has been reached on the fifo (remember to call start())
     * @note To avoid any possible confusion: This will only be emitted for ReadingDirection instances
     * @param data The input string from the previous newline, until the most recent newline (the data will not include either newline)
     */
    Q_SIGNAL void received(const QString &data);

    QString filepath() const;
private:
    FifoHandlerPrivate *d{nullptr};
};
