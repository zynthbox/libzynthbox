#pragma once

#include <QObject>
#include <QString>


class SndCategoryInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER m_name CONSTANT)
    Q_PROPERTY(QString value MEMBER m_value CONSTANT)
    Q_PROPERTY(int myFileCount MEMBER m_myFileCount NOTIFY myFileCountChanged)
    Q_PROPERTY(int communityFileCount MEMBER m_communityFileCount NOTIFY communityFileCountChanged)
    
public:
    explicit SndCategoryInfo(
        QString name,
        QString value,
        QObject *parent = nullptr
    )
        : QObject(parent)
        , m_name(name)
        , m_value(value)
        , m_myFileCount(0)
        , m_communityFileCount(0)
    {}
    
    QString m_name;
    QString m_value;
    int m_myFileCount;
    int m_communityFileCount;

    Q_INVOKABLE void setMyFileCount(int fileCount) {
        if (m_myFileCount != fileCount) {
            m_myFileCount = fileCount;
            Q_EMIT myFileCountChanged();
        }
    }

    Q_INVOKABLE void setCommunityFileCount(int fileCount) {
        if (m_communityFileCount != fileCount) {
            m_communityFileCount = fileCount;
            Q_EMIT communityFileCountChanged();
        }
    }

signals:
    void myFileCountChanged();
    void communityFileCountChanged();
};
Q_DECLARE_METATYPE(SndCategoryInfo*)
