#pragma once

#include <QCoreApplication>
#include <QObject>

#include <jack/jack.h>

/**
 * \brief A way to handle series of connections and disconnections of jack ports, ensuring that only the final action for two ports is carried out\
 *
 * The general method of use for this class is:
 *
 * - Access the global instance
 * - Request any disconnections and connections you desire through the functions provided
 * - Actually perform the final list of connection and disconnection calls using the commit() function
 */
class JackConnectionHandler : public QObject {
    Q_OBJECT
public:
    static JackConnectionHandler* instance() {
        static JackConnectionHandler* instance{nullptr};
        if (!instance) {
            instance = new JackConnectionHandler(qApp);
        }
        return instance;
    };
    explicit JackConnectionHandler(QObject *parent = nullptr);
    ~JackConnectionHandler() override;

    /**
     * \brief Called during plugin initialisation
     */
    void setJackClient(jack_client_t *jackClient) const;

    /**
     * \brief Whether or not the two given ports are connected, given a call to commit()
     * This function will check the state of the ports as though the current list of requests had been committed
     * @param first The first of the two ports you wish to check connection status for
     * @param second The second of the two ports you wish to check connection status for
     * @return True if the two ports are connected, false if not
     */
    Q_INVOKABLE bool isConnected(const QString &first, const QString &second);

    /**
     * \brief A list of names of all ports which are connected to the given port, given a call to commit()
     * This function will check the state of the ports as though the current list of requests had been committed
     * @param portName The port you wish to retrieve connections for
     * @return The list of all ports which connected to the given port
     */
    Q_INVOKABLE QVariantList getAllConnections(const QString &portName);

    /**
     * \brief Request that the two jack ports with the given names are connected
     * @note We will not attempt to discern whether the two ports are of compatible types, you will need to ensure that
     * @param first The fully qualified name of the first port
     * @param second The fully qualified name of the port to be connected to the first port
     */
    Q_INVOKABLE void connectPorts(const QString &first, const QString &second);

    /**
     * \brief Request that all ports connected to the given port are disconnected
     * @param portName The fully qualified name of the port you wish to remove all connections from
     */
    Q_INVOKABLE void disconnectAll(const QString &portName);

    /**
     * \brief Request that the two jack ports with the given names are disconnected
     * @note We will not attempt to discern whether the two ports are of compatible types, you will need to ensure that
     * @param first The fully qualified name of the first port
     * @param second The fully qualified name of the port to be disconnected from the first port
     */
    Q_INVOKABLE void disconnectPorts(const QString &first, const QString &second);

    /**
     * \brief Commit all the connections and disconnections which have been requested since the last time this function was called
     */
    Q_INVOKABLE void commit();

    /**
     * \brief Abort the connection attempts which have been requested since the most recent call to either commit() or clear()
     */
    Q_INVOKABLE void clear();
private:
    class Private;
    Private *d{nullptr};
};
