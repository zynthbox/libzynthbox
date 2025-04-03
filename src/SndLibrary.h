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
#include <QDir>


class SndLibraryModel;
class CategoryFilterProxyModel;


/**
 * @brief The SndLibrary class provides helper methods to manage, index and lookup `.snd` files
 */
class SndLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* model READ model CONSTANT)
    Q_PROPERTY(QObject* sourceModel READ sourceModel CONSTANT)
    Q_PROPERTY(QVariantMap categories READ categories CONSTANT)
    /**
     * @brief originFilter property will store which origin is currently selected and filter the model by selected origin
     * Accepted values : my-sounds, community-sounds, ""(Will display both my-sounds and community-sounds)
     * Default : ""(All Sounds)
     */
    Q_PROPERTY(QString originFilter READ originFilter WRITE setOriginFilter NOTIFY originFilterChanged)
    /**
     * @brief categoryFilter property will store which category is currently selectedand will filter the model by selected category
     * Accepted values : Category value
     * Default : "*"(All categories)
     */
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)

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
     * @brief Getter for categories property
     * @return A QMap<QString, SndCategoryInfo*> where the key is the category id
     */
    QVariantMap categories();
    /**
     * @brief Getter for the library's model
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
     * @brief Getter to get current originFilter
     * @return Current origin filter
     */
    QString originFilter();
    /**
     * @brief Getter to get current categoryFilter
     * @return Current category filter
     */
    QString categoryFilter();

    /**
     * @brief Setter to set origin filter
     * @param origin Accepted values : "my-sounds", "community-sounds" or ""(Will display all sounds from)
     */
    void setOriginFilter(const QString origin);
    /**
     * @brief Setter to set category filter
     * @param category One of the category from SndLibrary::categories to display snd files from that specific category
     */
    void setCategoryFilter(const QString category);

    /**
     * @brief Emitted whenever a sound file has been added to the model
     * @note Only connect to this using a queued connection, as otherwise it will slow down snd file scanning
     * @param fileIdentifier The file identifier string for the newly discovered file
     */
    Q_SIGNAL void sndFileAdded(const QString &fileIdentifier);
    /**
     * @brief Emitted when origin filter changes
     */
    Q_SIGNAL void originFilterChanged();
    /**
     * @brief Emitted when category filter changes
     */
    Q_SIGNAL void categoryFilterChanged();

    /**
     * @brief Process snd files to create an index of snd files by category. This method will handle all the changes to snd files as required
     * when processing an snd file. Indexing location can be set by setting the ENV variable `ZYNTHBOX_SND_INDEX_PATH`
     * - If the elements in the sources are newly added, the method will index them by categories and create symlinks.
     * - If the elements in the sources are removed, the method will remove them from the index and delete the symlinks
     * @param sources Sources can be a list of snd files absolute paths or a list of directories or a cobination of both
     * If any element in the sources list is a snd file it will process it and index it by category.
     * If any element in the sources list is a directory then it will process all the snd files in that directory and index it by category
     */
    Q_INVOKABLE void processSndFiles(const QStringList sources);
    /**
     * @brief Update an snd file's category
     * This method will update the category metadata of the snd file, re-index the snd file and update the model
     * @param sndFile SndFileInfo instance of the snd file to be updated
     * @param newCategory New category of the snd file
     */
    Q_INVOKABLE void updateSndFileCategory(SndFileInfo *sndFile, QString newCategory);
    /**
     * @brief Add snd file to "Best Of" category index
     * @param absolutePath Absolute path of the snd file to add to "Best Of"
     */
    Q_INVOKABLE void addToBestOf(QString absolutePath);
    /**
     * @brief Overloaded method to allow adding snd file to "Best Of" by its SndFileInfo instance
     * @param sndFileInfo SndFileInfo instance reference of the snd file to add to "Best Of"
     */
    Q_INVOKABLE void addToBestOf(SndFileInfo *sndFileInfo);
    /**
     * @brief Remove snd file from "Best Of" category index
     * @param absolutePath Absolute path of the snd file to add to "Best Of"
     */
    Q_INVOKABLE void removeFromBestOf(QString absolutePath);
    /**
     * @brief Overloaded method to allow removing snd file from "Best Of" by its SndFileInfo instance
     * @param sndFileInfo SndFileInfo instance reference of the snd file to remove from "Best Of"
     */
    Q_INVOKABLE void removeFromBestOf(SndFileInfo *sndFileInfo);

private:
    explicit SndLibrary(QObject *parent = nullptr);
    SndLibraryModel *m_soundsModel{nullptr};
    QSortFilterProxyModel *m_soundsByOriginModel{nullptr};
    CategoryFilterProxyModel *m_soundsByCategoryModel{nullptr};
    QSortFilterProxyModel *m_soundsByNameModel{nullptr};
    QJsonObject m_pluginsObj;
    QVariantMap m_categories;
    QTimer *m_updateAllFilesCountTimer{nullptr};
    QTimer *m_sortModelByNameTimer{nullptr};
    QString m_sndIndexPath;
    QMap<QString, QStringList*> *m_sndIndexLookupTable{nullptr};
    QDir m_baseSoundsDir{"/zynthian/zynthian-my-data/sounds/"};
    QString m_originFilter{""};
    QString m_categoryFilter{"*"};

    /**
     * @brief Process single snd files to create an index of snd files by category. This method is meant to be used internally by SndLibrary::processSndFiles
     * Indexing location can be set by setting the ENV variable `ZYNTHBOX_SND_INDEX_PATH`
     * - If the elements in the sources are newly added, the method will index them by categories and create symlinks.
     * - If the elements in the sources are removed, the method will remove them from the index and delete the symlinks
     * @param absolutePath Absolute filepath of the snd file to process
     */
    void processSndFile(const QString absolutePath);
    /**
     * @brief Overloaded method to allow forcing a category while processing an snd file instead of reading from metadata. This will allow
     * us to add/remove snd files to custom user categories like "Best Of" where the snd files do not have the user categories set in the metadata and
     * is passed from UI
     * @param absolutePath Absolute filepath of the snd file to process
     * @param category Forced category when indexing snd file. When this overloaded method is used, the passed category will be used instead of
     * reading from metadata. This will allow the indexer to index snd files to user created categories
     */
    void processSndFile(const QString absolutePath, const QString category);

    /**
     * @brief Refresh the lookup table that will be used to check if an snd file is already processed or not
     * - To generate the lookup table, recursively find all symlinks from path set in the ENV variable `ZYNTHBOX_SND_INDEX_PATH`
     * - Create an entry for each file identifier(name of the symlink file) and the value is a QStringList
     * - The value will contain all the categories that the snd file is associated to. There can be only 1 zynthbox category
     *   associated to it (0-99) and more than 0 user defined category associated to the snd file
     */
    void refreshSndIndexLookupTable();
};


class CategoryFilterProxyModel : public QSortFilterProxyModel {
public:
    CategoryFilterProxyModel(SndLibrary *parent);

private:
    SndLibrary *m_sndLibrary;

protected:
    /**
     * If category filter is set to "*", accept row if the sound is not from "Best Of" category.
     * "Best Of" will be displayed when "Best Of" button is checked. Hence when category filter is set to "100", then only accept row if the sound has category "100"
     * For other categories, accept row when sound that specifc selected category
     */
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};
