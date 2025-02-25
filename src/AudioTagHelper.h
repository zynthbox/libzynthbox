#pragma once

#include <QObject>
#include <QCoreApplication>
#include <QMap>

class AudioTagHelper : public QObject
{
    Q_OBJECT
public:
    static AudioTagHelper* instance() {
        static AudioTagHelper* instance{nullptr};
        if (!instance) {
            instance = new AudioTagHelper(qApp);
        }
        return instance;
    };
    explicit AudioTagHelper(QObject * parent = nullptr);
    ~AudioTagHelper() override;

    /**
     * \brief Retrieve all RIFF tags found in the given file
     * @param filepath The file that you wish to read all the RIFF tags from
     * @return A map of the tags (key is the tag name, value is the tag's value), or an empty hash if the file could not be accessed, or did not exist
     */
    const QMap<QString, QString> readWavMetadata(const QString &filepath);

    /**
     * \brief Write the given set of tags to the file at the given location
     * @param filepath The name of the file you wish to write the tags to (if the file doesn't exist, the function will fail silently)
     * @param metadata A map containing all the tags you wish to save to the file (key is the name of the tag, value is the tag's value)
     */
    void saveWavMetadata(const QString &filepath, const QMap<QString, QString> &metadata);
};
