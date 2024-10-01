#include "MidiRouterFilter.h"

#include "MidiRouterFilterEntry.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

MidiRouterFilter::MidiRouterFilter(MidiRouterDevice* parent)
    : QObject(parent)
{
    connect(this, &MidiRouterFilter::entriesChanged, &MidiRouterFilter::entriesDataChanged);
}

MidiRouterFilter::~MidiRouterFilter()
{
}

const MidiRouterFilterEntry * MidiRouterFilter::match(const jack_midi_event_t& event) const
{
    for (const MidiRouterFilterEntry *entry : qAsConst(m_entries)) {
        if (entry->match(event)) {
            // ##################################
            // EARLY RETURN - to retain constness
            // ##################################
            return entry;
            break;
        }
    }
    return nullptr;
}

const MidiRouterFilterEntry * MidiRouterFilter::matchCommand(const CUIAHelper::Event& cuiaEvent, const ZynthboxBasics::Track& track, const ZynthboxBasics::Slot& slot, const int& value)
{
    for (const MidiRouterFilterEntry *entry : qAsConst(m_entries)) {
        if (entry->matchCommand(cuiaEvent, track, slot, value)) {
            // ##################################
            // EARLY RETURN - to retain constness
            // ##################################
            return entry;
            break;
        }
    }
    return nullptr;
}

QString MidiRouterFilter::serialize() const
{
    QJsonDocument document;
    QJsonArray filterEntries;
    for (const MidiRouterFilterEntry *entry : qAsConst(m_entries)) {
        QJsonObject entryObject;
        entryObject.insert("targetTrack", entry->targetTrack());
        entryObject.insert("originTrack", entry->originTrack());
        entryObject.insert("originSlot", entry->originSlot());
        entryObject.insert("requiredBytes", entry->requiredBytes());
        entryObject.insert("requireRange", entry->requireRange());
        entryObject.insert("byte1Minimum", entry->byte1Minimum());
        entryObject.insert("byte1Maximum", entry->byte1Maximum());
        entryObject.insert("byte2Minimum", entry->byte2Minimum());
        entryObject.insert("byte2Maximum", entry->byte2Maximum());
        entryObject.insert("byte3Minimum", entry->byte3Minimum());
        entryObject.insert("byte3Maximum", entry->byte3Maximum());
        entryObject.insert("cuiaEvent", entry->cuiaEvent());
        entryObject.insert("valueMinimum", entry->valueMinimum());
        entryObject.insert("valueMaximum", entry->valueMaximum());
        QJsonArray entryRules;
        for (const MidiRouterFilterEntryRewriter *rewriter : entry->rewriteRules()) {
            QJsonObject ruleObject;
            ruleObject.insert("type", rewriter->type());
            ruleObject.insert("byteSize", rewriter->byteSize());
            ruleObject.insert("bytes", QJsonArray{rewriter->byte1(), rewriter->byte2(), rewriter->byte3()});
            ruleObject.insert("bytesAddChannel", QJsonArray{rewriter->byte1AddChannel(), rewriter->byte2AddChannel(), rewriter->byte3AddChannel()});
            ruleObject.insert("cuiaEvent", rewriter->cuiaEvent());
            ruleObject.insert("cuiaTrack", rewriter->cuiaTrack());
            ruleObject.insert("cuiaSlot", rewriter->cuiaTrack());
            ruleObject.insert("cuiaValue", rewriter->cuiaValue());
            entryRules.append(ruleObject);
        }
        entryObject.insert("entries", entryRules);
        filterEntries.append(entryObject);
    }
    document.setArray(filterEntries);
    return document.toJson();
}

bool MidiRouterFilter::deserialize(const QString& json)
{
    bool result{false};
    // Rather than clearing the old list, create new list, fill that, and swap it in
    QList<MidiRouterFilterEntry*> newEntries;
    if (json.length() > 0) {
        QJsonParseError error;
        QJsonDocument document{QJsonDocument::fromJson(json.toUtf8(), &error)};
        if (error.error == QJsonParseError::NoError) {
            if (document.isArray()) {
                QJsonArray filterEntries = document.array();
                for (QJsonValueRef entryValueRef : filterEntries) {
                    if (entryValueRef.isObject()) {
                        const QJsonObject entryObject = entryValueRef.toObject();
                        MidiRouterFilterEntry *entry = new MidiRouterFilterEntry(qobject_cast<MidiRouterDevice*>(parent()), this);
                        connect(entry, &MidiRouterFilterEntry::descripionChanged, this, &MidiRouterFilter::entriesDataChanged);
                        entry->setTargetTrack(entryObject.value("targetTrack").toVariant().value<ZynthboxBasics::Track>());
                        entry->setOriginTrack(entryObject.value("originTrack").toVariant().value<ZynthboxBasics::Track>());
                        entry->setOriginSlot(entryObject.value("targetSlot").toVariant().value<ZynthboxBasics::Slot>());
                        entry->setRequiredBytes(entryObject.value("requiredBytes").toVariant().value<int>());
                        entry->setRequireRange(entryObject.value("requireRange").toVariant().value<bool>());
                        entry->setByte1Minimum(entryObject.value("byte1Minimum").toVariant().value<int>());
                        entry->setByte1Maximum(entryObject.value("byte1Maximum").toVariant().value<int>());
                        entry->setByte2Minimum(entryObject.value("byte2Minimum").toVariant().value<int>());
                        entry->setByte2Maximum(entryObject.value("byte2Maximum").toVariant().value<int>());
                        entry->setByte3Minimum(entryObject.value("byte3Minimum").toVariant().value<int>());
                        entry->setByte3Maximum(entryObject.value("byte3Maximum").toVariant().value<int>());
                        entry->setCuiaEvent(entryObject.value("cuiaEvent").toVariant().value<CUIAHelper::Event>());
                        entry->setValueMinimum(entryObject.value("valueMinimum").toVariant().value<int>());
                        entry->setValueMaximum(entryObject.value("valueMaximum").toVariant().value<int>());
                        QJsonValue rewriteRulesValue = entryObject.value("entries");
                        if (rewriteRulesValue.isArray()) {
                            QJsonArray rewriteRules = rewriteRulesValue.toArray();
                            for (QJsonValueRef rewriteRuleRef : rewriteRules) {
                                if (rewriteRuleRef.isObject()) {
                                    QJsonObject ruleObject = rewriteRuleRef.toObject();
                                    MidiRouterFilterEntryRewriter* rewriter = entry->addRewriteRule();
                                    rewriter->setType(ruleObject.value("type").toVariant().value<MidiRouterFilterEntryRewriter::RuleType>());
                                    rewriter->setByteSize(ruleObject.value("byteSize").toVariant().value<MidiRouterFilterEntryRewriter::EventSize>());
                                    QJsonArray bytesArray = ruleObject.value("bytes").toArray();
                                    if (bytesArray.count() == 3) {
                                        rewriter->setByte1(bytesArray[0].toVariant().value<MidiRouterFilterEntryRewriter::EventByte>());
                                        rewriter->setByte2(bytesArray[1].toVariant().value<MidiRouterFilterEntryRewriter::EventByte>());
                                        rewriter->setByte3(bytesArray[2].toVariant().value<MidiRouterFilterEntryRewriter::EventByte>());
                                    } else {
                                        qWarning() << Q_FUNC_INFO << "The bytes array for a rewrite rule did not contain exactly three (3) elements. It contained" << bytesArray.count() << "elements. This will be ignored, but is a problem.";
                                    }
                                    QJsonArray bytesAddChannelArray = ruleObject.value("bytesAddChannel").toArray();
                                    if (bytesAddChannelArray.count() == 3) {
                                        rewriter->setByte1AddChannel(bytesAddChannelArray[0].toVariant().value<bool>());
                                        rewriter->setByte2AddChannel(bytesAddChannelArray[1].toVariant().value<bool>());
                                        rewriter->setByte3AddChannel(bytesAddChannelArray[2].toVariant().value<bool>());
                                    } else {
                                        qWarning() << Q_FUNC_INFO << "The bytesAddChannel array for a rewrite rule did not contain exactly three (3) elements. It contained" << bytesAddChannelArray.count() << "elements. This will be ignored, but is a problem.";
                                    }
                                    rewriter->setCuiaEvent(ruleObject.value("cuiaEvent").toVariant().value<CUIAHelper::Event>());
                                    rewriter->setCuiaTrack(ruleObject.value("cuiaTrack").toVariant().value<ZynthboxBasics::Track>());
                                    rewriter->setCuiaSlot(ruleObject.value("cuiaSlot").toVariant().value<ZynthboxBasics::Slot>());
                                    rewriter->setCuiaValue(ruleObject.value("cuiaValue").toVariant().value<MidiRouterFilterEntryRewriter::ValueSpecifier>());
                                } else {
                                    qWarning() << Q_FUNC_INFO << "A rewrite rule was not an object. This will be ignored, but is a problem.";
                                }
                            }
                        } else {
                            qWarning() << Q_FUNC_INFO << "The list of rewrite rules was not an array. This will be ignored, but is a problem.";
                        }
                        newEntries.append(entry);
                    } else {
                        qWarning() << Q_FUNC_INFO << "A filter entry was not an object. This will be ignored, but is a problem.";
                    }
                }
                result = true;
            } else {
                qWarning() << Q_FUNC_INFO << "The json passed to the function is not an array as expected. The data was:\n" << json;
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Got an error while attempting to parse what we assume is a json string:" << error.errorString() << "The data was:\n" << json;
        }
    } else {
        // If the thing is empty, then it's still fine and we'll say we deserialised fine
        result = true;
    }
    QList<MidiRouterFilterEntry*> oldEntries = m_entries;
    m_entries = newEntries;
    Q_EMIT entriesChanged();
    QTimer::singleShot(1000, this, [oldEntries](){ qDeleteAll(oldEntries); });
    return result;
}

QList<MidiRouterFilterEntry *> MidiRouterFilter::entries() const
{
    return m_entries;
}

MidiRouterFilterEntry * MidiRouterFilter::createEntry(const int& index)
{
    MidiRouterFilterEntry *entry = new MidiRouterFilterEntry(qobject_cast<MidiRouterDevice*>(parent()), this);
    connect(entry, &MidiRouterFilterEntry::descripionChanged, this, &MidiRouterFilter::entriesDataChanged);
    // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
    auto tempList = m_entries;
    if (-1 < index && index < tempList.count()) {
        tempList.insert(index, entry);
    } else {
        tempList.append(entry);
    }
    m_entries = tempList;
    Q_EMIT entriesChanged();
    return entry;
}

void MidiRouterFilter::deleteEntry(const int& index)
{
    if (-1 < index && index < m_entries.count()) {
        // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
        auto tempList = m_entries;
        MidiRouterFilterEntry *deletedEntry = tempList.takeAt(index);
        m_entries = tempList;
        Q_EMIT entriesChanged();
        QTimer::singleShot(1000, this, [deletedEntry](){ deletedEntry->deleteLater(); });
    }
}

int MidiRouterFilter::indexOf(MidiRouterFilterEntry* entry) const
{
    return m_entries.indexOf(entry);
}

void MidiRouterFilter::swap(MidiRouterFilterEntry* swapThis, MidiRouterFilterEntry* withThis)
{
    const int firstPosition{m_entries.indexOf(swapThis)};
    const int secondPosition{m_entries.indexOf(withThis)};
    if (firstPosition > -1 && secondPosition > -1) {
        // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
        auto tempList = m_entries;
        tempList.swapItemsAt(firstPosition, secondPosition);
        m_entries = tempList;
        Q_EMIT entriesChanged();
    }
}

MidiRouterFilter::Direction MidiRouterFilter::direction() const
{
    return m_direction;
}

void MidiRouterFilter::setDirection(const Direction& direction)
{
    if (m_direction != direction) {
        m_direction = direction;
        Q_EMIT directionChanged();
    }
}
