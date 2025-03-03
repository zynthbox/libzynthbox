#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>


/**
 * @brief The SndLibrary class provides helper methods to manage, index and lookup `.snd` files
 */
class SndLibrary : public QObject
{
    Q_OBJECT
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
    Q_INVOKABLE void serializeTo(const QString sourceDir, const QString outputFile);
private:
    explicit SndLibrary(QObject *parent = nullptr);
};
