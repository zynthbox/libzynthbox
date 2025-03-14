#pragma once

#include "SndLibraryModel.h"
#include "SndCategoryInfo.h"
#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QSortFilterProxyModel>
#include <QJsonObject>
#include <QVariantList>
#include <QMap>
#include <QVariantMap>
#include <QTimer>


class SndLibraryModel;


/**
 * @brief The SndLibrary class provides helper methods to manage, index and lookup `.snd` files
 */
class SndLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* model MEMBER m_soundsByNameModel CONSTANT)
    Q_PROPERTY(QVariantMap categories READ categories CONSTANT)

public:
    static SndLibrary* instance() {
        static SndLibrary* instance{nullptr};
        if (!instance) {
            instance = new SndLibrary(qApp);
        }
        return instance;
    }
    // Delete the methods we dont want to avoid having copies of the singleton class
    SndLibrary(SndLibrary const&) = delete;
    void operator=(SndLibrary const&) = delete;

    /**
     * @brief serializeTo Create a json statistics file with metadata of the snd files from sourceDir
     * @param sourceDir Parent directory from where snd files will be searched
     * @param outputFile Path to json file where the statistics json will be written to
     *
     * The output json will be in the following format :
     * <code>
     * {
     *   "<category>": {
     *     "count": "<number of files in category>"
     *     "files": {
     *       "<file name>": {
     *         "synthSlotsData": [<array of 5 strings>],
     *         "sampleSlotsData": [<array of 5 strings>],
     *         "fxSlotsData": [<array of 5 strings>]
     *       },
     *       ...
     *     }
     *   },
     *   ...
     * }
     * </code>
     */
    Q_INVOKABLE void serializeTo(const QString sourceDir, const QString origin, const QString outputFile);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setOriginFilter(const QString origin);
    Q_INVOKABLE void setCategoryFilter(const QString category);
    /**
     * @brief addSndFile Extract information from the snd file and add the snd file info to statistics file and model
     * @param filepath Path of the snd file to be added
     * @return Returns if the action was successful
     */
    Q_INVOKABLE void addSndFiles(const QStringList sndFilepaths, const QString origin, const QString statsFilepath);
    /**
     * @brief removeSndFile Remove the snd file info from statistics file and model
     * @param filepath Path of the snd file to be removed
     * @return Returns if the action was successful
     */
    Q_INVOKABLE bool removeSndFile(const QString filepath, const QString origin);
    /**
     * @brief extractSndFileInfo Read metadata from a snd file and extract the information to a SndFileInfo
     * @param filepath Path to the snd file
     * @return Returns a SndFileInfo* object if it was correctly able to extract the information otherwise returns a nullptr
     */
    Q_INVOKABLE SndFileInfo* extractSndFileInfo(const QString filepath, const QString origin);

    QVariantMap categories();

private:
    explicit SndLibrary(QObject *parent = nullptr);
    SndLibraryModel *m_soundsModel{nullptr};
    QSortFilterProxyModel *m_soundsByOriginModel{nullptr};
    QSortFilterProxyModel *m_soundsByCategoryModel{nullptr};
    QSortFilterProxyModel *m_soundsByNameModel{nullptr};
    QJsonObject m_pluginsObj;
    QVariantMap m_categories;
    QTimer *m_updateAllFilesCountTimer{nullptr};
};
