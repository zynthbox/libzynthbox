#pragma once

#include <QObject>
#include <QString>


class SndCategoryInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER m_name CONSTANT)
    Q_PROPERTY(QString value MEMBER m_value CONSTANT)
    Q_PROPERTY(int fileCount MEMBER m_fileCount NOTIFY fileCountChanged)
    
public:
    explicit SndCategoryInfo(
        QString name,
        QString value,
        QObject *parent = nullptr
    )
        : QObject(parent)
        , m_name(name)
        , m_value(value)
        , m_fileCount(0)
    {}
    
    QString m_name;
    QString m_value;
    int m_fileCount;

    Q_INVOKABLE void setFileCount(int fileCount) {
        if (m_fileCount != fileCount) {
            m_fileCount = fileCount;
            Q_EMIT fileCountChanged();
        }
    }

signals:
    void fileCountChanged();
};
Q_DECLARE_METATYPE(SndCategoryInfo*)
