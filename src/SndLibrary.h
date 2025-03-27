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
    Q_PROPERTY(QObject* model READ model CONSTANT)
    Q_PROPERTY(QObject* sourceModel READ sourceModel CONSTANT)
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
     * @brief Process snd files to create an index of snd files by category. This method will handle all the changes to snd files as required
     * when processing an snd file. Indexing location can be set by setting the ENV variable `ZYNTHBOX_SND_INDEX_PATH`
     * - If the elements in the sources are newly added, the method will index them by categories and create symlinks.
     * - If the elements in the sources are removed, the method will remove them from the index and delete the symlinks
     * @param sources Sources can be a list of snd files or a list of directories or a cobination of both
     * If any element in the sources list is a snd file it will process it and index it by category.
     * If any element in the sources list is a directory then it will process all the snd files in that directory and index it by category
     */
    Q_INVOKABLE void processSndFiles(const QStringList sources);
    /**
     * @brief Filter snd files by origin
     * @param origin Accepted values : "my-sounds" or "community-sounds"
     */
    Q_INVOKABLE void setOriginFilter(const QString origin);
    /**
     * @brief Filter snd files by category
     * @param category One of the category from SndLibrary::categories to display snd files from that specific category
     */
    Q_INVOKABLE void setCategoryFilter(const QString category);
    /**
     * @brief Getter for categories property
     * @return A QMap<QString, SndCategoryInfo*> where the key is the category id
     */
    QVariantMap categories();
    /**
     * \brief Getter for the library's model
     * @return An instance of SndLibraryModel
     */
    QObject *model() const;

    /**
     * @brief Getter to retreive the source QAbstractListModel
     * @return SndLibraryModel instance
     */
    SndLibraryModel *sourceModel();

    /**
     * @brief Getter for snd index base dir
     */
    QString sndIndexPath();

    /**
     * \brief Emitted whenever a sound file has been added to the model
     * @note Only connect to this using a queued connection, as otherwise it will slow down snd file scanning
     * @param fileIdentifier The file identifier string for the newly discovered file
     */
    Q_SIGNAL void sndFileAdded(const QString &fileIdentifier);

    /**
     * @brief Update an snd file's category
     * This method will update the category metadata of the snd file, re-index the snd file and update the model
     * @param sndFile SndFileInfo instance of the snd file to be updated
     * @param newCategory New category of the snd file
     */
    Q_INVOKABLE void updateSndFileCategory(SndFileInfo *sndFile, QString newCategory);

private:
    explicit SndLibrary(QObject *parent = nullptr);
    SndLibraryModel *m_soundsModel{nullptr};
    QSortFilterProxyModel *m_soundsByOriginModel{nullptr};
    QSortFilterProxyModel *m_soundsByCategoryModel{nullptr};
    QSortFilterProxyModel *m_soundsByNameModel{nullptr};
    QJsonObject m_pluginsObj;
    QVariantMap m_categories;
    QTimer *m_updateAllFilesCountTimer{nullptr};
    QTimer *m_sortModelByNameTimer{nullptr};
    QString m_sndIndexPath;
    QMap<QString, QStringList*> *m_sndIndexLookupTable{nullptr};

    /**
     * @brief Process single snd files to create an index of snd files by category. This method is meant to be used internally by SndLibrary::processSndFiles
     * Indexing location can be set by setting the ENV variable `ZYNTHBOX_SND_INDEX_PATH`
     * - If the elements in the sources are newly added, the method will index them by categories and create symlinks.
     * - If the elements in the sources are removed, the method will remove them from the index and delete the symlinks
     * @param sources Sources can be a list of snd files or a list of directories or a cobination of both
     * If any element in the sources list is a snd file it will process it and index it by category.
     * If any element in the sources list is a directory then it will process all the snd files in that directory and index it by category
     */
    void processSndFile(const QString source);

    /**
     * @brief Refresh the lookup table that will be used to check if an snd file is already processed or not
     * - To generate the lookup table, recursively find all symlinks from path set in the ENV variable `ZYNTHBOX_SND_INDEX_PATH`
     * - Create an entry for each file identifier(name of the symlink file) and the value is a QStringList
     * - The value will contain all the categories that the snd file is associated to. There can be only 1 zynthbox category
     *   associated to it (0-99) and more than 0 user defined category associated to the snd file
     */
    void refreshSndIndexLookupTable();
};
