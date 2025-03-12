#pragma once

#include <QObject>
#include <QString>
#include <QStringList>


class SndFileInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER m_name CONSTANT)
    Q_PROPERTY(QString origin MEMBER m_origin CONSTANT)
    Q_PROPERTY(QString category MEMBER m_category CONSTANT)
    Q_PROPERTY(QStringList synthSlotsData MEMBER m_synthSlotsData CONSTANT)
    Q_PROPERTY(QStringList sampleSlotsData MEMBER m_sampleSlotsData CONSTANT)
    Q_PROPERTY(QStringList fxSlotsData MEMBER m_fxSlotsData CONSTANT)
    
public:
    explicit SndFileInfo(
        QString name,
        QString origin,
        QString category,
        QStringList synthSlotsData,
        QStringList sampleSlotsData,
        QStringList fxSlotsData,
        QObject *parent = nullptr
    )
        : QObject(parent)
        , m_name(name)
        , m_origin(origin)
        , m_category(category)
        , m_synthSlotsData(synthSlotsData)
        , m_sampleSlotsData(sampleSlotsData)
        , m_fxSlotsData(fxSlotsData)
    {}
    
    QString m_name;
    /**
     * @brief origin Store the origin of the sound file
     * origin can be either `my-sounds` or `community-sounds`
     */
    QString m_origin;
    QString m_category;
    QStringList m_synthSlotsData;
    QStringList m_sampleSlotsData;
    QStringList m_fxSlotsData;
};
