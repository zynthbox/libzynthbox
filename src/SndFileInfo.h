#pragma once

#include "AudioTagHelper.h"

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QStringList>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>


class SndFileInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString fileIdentifier READ fileIdentifier CONSTANT)
    Q_PROPERTY(QString fileIdentifierBase64Encoded READ fileIdentifierBase64Encoded CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString origin READ origin CONSTANT)
    Q_PROPERTY(QString category READ category CONSTANT)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(QStringList synthSlotsData READ synthSlotsData CONSTANT)
    Q_PROPERTY(QStringList sampleSlotsData READ sampleSlotsData CONSTANT)
    Q_PROPERTY(QStringList fxSlotsData READ fxSlotsData CONSTANT)
    Q_PROPERTY(QString synthFxSnapshot READ synthFxSnapshot CONSTANT)
    Q_PROPERTY(QString sampleSnapshot READ sampleSnapshot CONSTANT)
    
public:
    explicit SndFileInfo(
        QString fileIdentifier,
        QString name,
        QString origin,
        QString category,
        QObject *parent = nullptr
        )
        : QObject(parent)
        , m_fileIdentifier(fileIdentifier)
        , m_fileIdentifierBase64Encoded(fileIdentifier.toUtf8().toBase64(QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals))
        , m_name(name)
        , m_origin(origin)
        , m_category(category)
    {}

    explicit SndFileInfo(
        QByteArray fileIdentifierBase64Encoded,
        QString name,
        QString origin,
        QString category,
        QObject *parent = nullptr
        )
        : QObject(parent)
        , m_fileIdentifierBase64Encoded(fileIdentifierBase64Encoded)
        , m_fileIdentifier(QByteArray::fromBase64(fileIdentifierBase64Encoded, QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals))
        , m_name(name)
        , m_origin(origin)
        , m_category(category)
    {}

    Q_INVOKABLE QString fileIdentifier() {
        return m_fileIdentifier;
    }
    Q_INVOKABLE QString fileIdentifierBase64Encoded() {
        return m_fileIdentifierBase64Encoded;
    }
    Q_INVOKABLE QString name() {
        return m_name;
    }
    Q_INVOKABLE QString origin() {
        return m_origin;
    }
    Q_INVOKABLE QString category() {
        return m_category;
    }
    Q_INVOKABLE QString filePath() {
        return m_baseSoundsDir + m_fileIdentifier;
    }
    Q_INVOKABLE QStringList synthSlotsData() {
        fetchAndParseMetadata();
        return m_synthSlotsData;
    }
    Q_INVOKABLE QStringList sampleSlotsData() {
        fetchAndParseMetadata();
        return m_sampleSlotsData;
    }
    Q_INVOKABLE QStringList fxSlotsData() {
        fetchAndParseMetadata();
        return m_fxSlotsData;
    }
    Q_INVOKABLE QString synthFxSnapshot() {
        fetchAndParseMetadata();
        return m_synthFxSnapshot;
    }
    Q_INVOKABLE QString sampleSnapshot() {
        fetchAndParseMetadata();
        return m_sampleSnapshot;
    }

private:
    /**
     * @brief m_fileIdentifier is the unique string for a file that has the sound origin and username
     * For example, if a user named `user1` has a sound file named `sound1.snd` then the fileIdentifier
     * would be the relative path `community-sounds/user1/sound1.snd`. This fileIdentifier will be base64 encoded
     * and used as the symlink file name so when checking if a file is already processed, a snd file can be mapped
     * to its symlink file without keeping any database.
     */
    QString m_fileIdentifier;
    QString m_fileIdentifierBase64Encoded;
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
    QString m_synthFxSnapshot;
    QString m_sampleSnapshot;
    QMap<QString, QString> m_metadata;
    QString m_baseSoundsDir{"/zynthian/zynthian-my-data/sounds/"};

    /**
     * @brief Fetch metadata from the snd file, parse it and set the respective variables.
     * This method is safe to be called multiple times as it will make sure to fetch and parse
     * the metadata only the first time and will ignore any subsequent calls
     */
    void fetchAndParseMetadata() {
        if (m_metadata.isEmpty()) {
            m_metadata = AudioTagHelper::instance()->readWavMetadata(filePath());
            if (m_metadata.contains("ZYNTHBOX_SOUND_SYNTH_SLOTS_DATA")) {
                m_synthSlotsData.clear();
                auto slotsDataVariantList = QJsonDocument::fromJson(m_metadata["ZYNTHBOX_SOUND_SYNTH_SLOTS_DATA"].toUtf8()).array().toVariantList();
                for (int i = 0; i < slotsDataVariantList.size(); ++i) {
                    m_synthSlotsData << slotsDataVariantList.value(i).toString();
                }
            }
            if (m_metadata.contains("ZYNTHBOX_SOUND_SAMPLE_SLOTS_DATA")) {
                m_sampleSlotsData.clear();
                auto slotsDataVariantList = QJsonDocument::fromJson(m_metadata["ZYNTHBOX_SOUND_SAMPLE_SLOTS_DATA"].toUtf8()).array().toVariantList();
                for (int i = 0; i < slotsDataVariantList.size(); ++i) {
                    m_sampleSlotsData << slotsDataVariantList.value(i).toString();
                }
            }
            if (m_metadata.contains("ZYNTHBOX_SOUND_FX_SLOTS_DATA")) {
                m_fxSlotsData.clear();
                auto slotsDataVariantList = QJsonDocument::fromJson(m_metadata["ZYNTHBOX_SOUND_FX_SLOTS_DATA"].toUtf8()).array().toVariantList();
                for (int i = 0; i < slotsDataVariantList.size(); ++i) {
                    m_fxSlotsData << slotsDataVariantList.value(i).toString();
                }
            }
            if (m_metadata.contains("ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT")) {
                m_synthFxSnapshot = m_metadata["ZYNTHBOX_SOUND_SYNTH_FX_SNAPSHOT"];
            }
            if (m_metadata.contains("ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT")) {
                m_sampleSnapshot = m_metadata["ZYNTHBOX_SOUND_SAMPLE_SNAPSHOT"];
            }
        }
    }
};
