#include "JackConnectionHandler.h"

#include <QDebug>

struct JackConnectionHandlerConnection {
    QString first;
    QString second;
    jack_port_t *firstPort{nullptr};
    jack_port_t *secondPort{nullptr};
    bool connect{true}; // Set this to false to perform a disconnection
};

class JackConnectionHandler::Private {
public:
    Private() {}
    QList<JackConnectionHandlerConnection*> connections;
    jack_client_t *client{nullptr};

    void createEntry(const QString &first, const QString &second, bool connect) {
        bool foundExistingEntry{false};
        for (JackConnectionHandlerConnection *connection : qAsConst(connections)) {
            if ((connection->first == first && connection->second == second) || (connection->first == first && connection->second == second)) {
                connection->connect = connect;
                foundExistingEntry = true;
                break;
            }
        }
        if (foundExistingEntry == false) {
            JackConnectionHandlerConnection *newEntry{new JackConnectionHandlerConnection};
            jack_port_t *firstPort = jack_port_by_name(client, first.toUtf8());
            jack_port_t *secondPort = jack_port_by_name(client, second.toUtf8());
            int firstPortFlags = jack_port_flags(firstPort);
            // Since jack_connect requires the ports to be in the order output -> input, let's make sure we handle the ugly case, so we can be safe at runtime
            if (firstPortFlags & JackPortFlags::JackPortIsOutput) {
                newEntry->first = first;
                newEntry->second = second;
                newEntry->firstPort = firstPort;
                newEntry->secondPort = secondPort;
            } else {
                newEntry->first = second;
                newEntry->second = first;
                newEntry->firstPort = secondPort;
                newEntry->secondPort = firstPort;
            }
            newEntry->connect = connect;
            connections.append(newEntry);
        }
    }
};

JackConnectionHandler::JackConnectionHandler(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

JackConnectionHandler::~JackConnectionHandler()
{
    qDeleteAll(d->connections);
    delete d;
}

void JackConnectionHandler::setJackClient(jack_client_t* jackClient) const
{
    d->client = jackClient;
}

bool JackConnectionHandler::isConnected(const QString& first, const QString& second)
{
    // qDebug() << Q_FUNC_INFO << first << second;
    bool foundConnection{false};
    bool foundExistingEntry{false};
    for (JackConnectionHandlerConnection *connection : qAsConst(d->connections)) {
        if ((connection->first == first && connection->second == second) || (connection->first == first && connection->second == second)) {
            foundConnection = connection->connect;
            foundExistingEntry = true;
            break;
        }
    }
    if (foundExistingEntry == false) {
        jack_port_t *port = jack_port_by_name(d->client, first.toUtf8());
        const char **connectedPortNames = jack_port_get_all_connections(d->client, port);
        if (connectedPortNames != nullptr) {
            for (int portIndex = 0; connectedPortNames[portIndex]; ++portIndex) {
                if (second == QString::fromUtf8(connectedPortNames[portIndex])) {
                    foundConnection = true;
                    break;
                }
            }
            jack_free(connectedPortNames);
        }
    }
    return foundConnection;
}

QVariantList JackConnectionHandler::getAllConnections(const QString& portName)
{
    // qDebug() << Q_FUNC_INFO << portName;
    QStringList connectedPorts;
    jack_port_t *port = jack_port_by_name(d->client, portName.toUtf8());
    const char **connectedPortNames = jack_port_get_all_connections(d->client, port);
    if (connectedPortNames != nullptr) {
        for (int portIndex = 0; connectedPortNames[portIndex]; ++portIndex) {
            connectedPorts << QString::fromUtf8(connectedPortNames[portIndex]);
        }
        jack_free(connectedPortNames);
    }
    for (JackConnectionHandlerConnection *connection : qAsConst(d->connections)) {
        if (connection->first == portName || connection->second == portName) {
            if (connection->connect) {
                if (connection->first == portName) {
                    if (connectedPorts.contains(connection->second) == false) {
                        connectedPorts.append(connection->second);
                    }
                } else {
                    if (connectedPorts.contains(connection->first) == false) {
                        connectedPorts.append(connection->first);
                    }
                }
            } else {
                if (connection->first == portName) {
                    connectedPorts.removeAll(connection->second);
                } else {
                    connectedPorts.removeAll(connection->first);
                }
            }
        }
    }
    QVariantList result;
    for (const QString &string : qAsConst(connectedPorts)) {
        result.append(string);
    }
    return result;
}

void JackConnectionHandler::connectPorts(const QString& first, const QString& second)
{
    // qDebug() << Q_FUNC_INFO << first << second;
    d->createEntry(first, second, true);
}

void JackConnectionHandler::disconnectAll(const QString& portName)
{
    // qDebug() << Q_FUNC_INFO << portName;
    // First find all connections involving this port and change them to disconnections (because they won't have been committed yet)
    for (JackConnectionHandlerConnection *connection : qAsConst(d->connections)) {
        if (connection->first == portName || connection->second == portName) {
            connection->connect = false;
        }
    }
    // Now look up all existing connections, and call our disconnect on the combo
    jack_port_t *port = jack_port_by_name(d->client, portName.toUtf8());
    const char **connectedPortNames = jack_port_get_all_connections(d->client, port);
    if (connectedPortNames != nullptr) {
        for (int portIndex = 0; connectedPortNames[portIndex]; ++portIndex) {
            d->createEntry(portName, QString::fromUtf8(connectedPortNames[portIndex]), false);
        }
        jack_free(connectedPortNames);
    }
}

void JackConnectionHandler::disconnectPorts(const QString& first, const QString& second)
{
    // qDebug() << Q_FUNC_INFO << first << second;
    d->createEntry(first, second, false);
}

void JackConnectionHandler::commit()
{
    // qDebug() << Q_FUNC_INFO;
    for (JackConnectionHandlerConnection *connection : qAsConst(d->connections)) {
        if (connection->firstPort && connection->secondPort) {
            if (connection->connect) {
                int result = jack_connect(d->client, connection->first.toUtf8(), connection->second.toUtf8());
                if (result == 0 || result == EEXIST) {
                    // all is well
                } else {
                    qWarning() << Q_FUNC_INFO << "Attempted to connect" << connection->first << "to" << connection->second << "and got the error" << result;
                }
            } else {
                int result = jack_disconnect(d->client, connection->first.toUtf8(), connection->second.toUtf8());
                if (result == 0 || result == -1) {
                    // all is well (-1 is "no connection found", which we will accept as a successful result, as we are after the result, not the action)
                } else {
                    qWarning() << Q_FUNC_INFO << "Attempted to disconnect" << connection->first << "from" << connection->second << "and got the error" << result;
                }
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Attempted to perform a connection action on one or more ports which don't exist:" << connection->first << connection->firstPort << connection->second << connection->secondPort;
        }
    }
    qDeleteAll(d->connections);
    d->connections.clear();
}

void JackConnectionHandler::clear()
{
    qDebug() << Q_FUNC_INFO;
    qDeleteAll(d->connections);
    d->connections.clear();
}
