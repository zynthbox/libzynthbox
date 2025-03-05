#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QList>
#include <QStringList>


class SndFile : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER m_name CONSTANT)
    Q_PROPERTY(QString origin MEMBER m_origin CONSTANT)
    Q_PROPERTY(QString category MEMBER m_category CONSTANT)
    Q_PROPERTY(QStringList synthSlotsData MEMBER m_synthSlotsData CONSTANT)
    Q_PROPERTY(QStringList sampleSlotsData MEMBER m_sampleSlotsData CONSTANT)
    Q_PROPERTY(QStringList fxSlotsData MEMBER m_fxSlotsData CONSTANT)

public:
    explicit SndFile(
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


class SndLibraryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SndLibraryModel(QObject *parent);

    enum Roles {
        NameRole = Qt::UserRole + 1,
        OriginRole,
        CategoryRole,
        SoundRole
    };

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    /**
     * @brief Re-read the statistics file and re-populate the sounds model
     */
    void refresh();
private:
    QList<SndFile*> m_sounds;
};
