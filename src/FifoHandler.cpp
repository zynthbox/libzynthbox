#include "FifoHandler.h"

#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include <stdlib.h>
#include <fstream>
#include <string>
#include <iostream>

class FifoHandlerPrivate : public QThread {
    Q_OBJECT
public:
    FifoHandlerPrivate(const QString& filepath, const FifoHandler::Direction& direction, FifoHandler *q)
        : QThread(q)
        , q(q)
        , filepath(filepath)
        , direction(direction)
    {
        if (direction == FifoHandler::ReadingDirection) {
            setObjectName(QString("FifoHandler Reading %1").arg(filepath));
        } else {
            setObjectName(QString("FifoHandler Writing %1").arg(filepath));
        }
    }
    FifoHandler *q{nullptr};
    QString filepath;
    FifoHandler::Direction direction;

    void run() override {
        if (QDir().exists(filepath)) {
            if (direction == FifoHandler::ReadingDirection) {
                connect(this, &FifoHandlerPrivate::received, q, &FifoHandler::received, Qt::QueuedConnection);
                runReader();
            } else {
                runWriter();
            }
        } else {
            qDebug() << Q_FUNC_INFO << "The fifo file does not exist:" << filepath;
        }
    }

    Q_SIGNAL void received(const QString &data);
    void runReader() {
        FILE *incomingFile;
        incomingFile = fopen(filepath.toLatin1().constData(), "r");
        int character{0};
        QString incomingData;
        incomingData.reserve(8192);
        while((character = getc(incomingFile)) != EOF) {
            if (character == '\n') {
                Q_EMIT received(incomingData);
                incomingData.clear();
                incomingData.reserve(8192);
            } else {
                incomingData.append(character);
            }
        }
        fclose(incomingFile);
    }

    bool stop{false};
    QString writerData;
    QWaitCondition waitToWrite;
    QMutex writeMutex;
    void runWriter() {
        std::ofstream outputFile;
        outputFile.open(filepath.toLatin1().constData(), std::ios::out);
        if (outputFile.is_open()) {
            while (stop == false) {
                writeMutex.lock();
                waitToWrite.wait(&writeMutex);
                // qDebug() << Q_FUNC_INFO << "Writing to output:" << writerData;
                outputFile << writerData.toUtf8().constData();
                outputFile.flush();
                writerData.clear();
                writeMutex.unlock();
            }
            outputFile.close();
        } else {
            qWarning() << Q_FUNC_INFO << "Cannot open fifo for writing:" << filepath;
        }
    }
    Q_SLOT void send(const QString &data) {
        writeMutex.lock();
        writerData.append(data);
        waitToWrite.wakeAll();
        writeMutex.unlock();
    }
};

FifoHandler::FifoHandler(const QString& filepath, const Direction& direction, QObject* parent)
    : QObject(parent)
    , d(new FifoHandlerPrivate(filepath, direction, this))
{
    if (direction == WritingDirection) {
        d->start();
    }
}

FifoHandler::~FifoHandler()
{
    d->stop = true;
    d->quit();
    d->wait(200);
    delete d;
}

void FifoHandler::send(const QString& data, const bool& autoAppendNewline)
{
    if (d->isRunning() == false) {
        d->start();
    }
    // qDebug() << Q_FUNC_INFO << "Queueing up for sending:" << data;
    if (autoAppendNewline && data.endsWith("\n") == false) {
        QMetaObject::invokeMethod(d, "send", Q_ARG(QString, QString::fromUtf8("%1\n").arg(data)));
    } else {
        QMetaObject::invokeMethod(d, "send", Q_ARG(QString, data));
    }
}

void FifoHandler::start()
{
    if (d->isRunning() == false) {
        d->start();
    }
}


QString FifoHandler::filepath() const
{
    return d->filepath;
}

#include "FifoHandler.moc"
