#include "SysexIdentity.h"
#include "SysexMessage.h"
#include "SysexIdTable.h"

#include <QVariant>

class SysexIdentityPrivate {
public:
    SysexIdentityPrivate() {}
    QVariantList manufacturerId;
    QList<int> manufacturerIdRaw;
    QString manufacturerName;
    QVariantList familyId;
    QList<int> familyIdRaw;
    QString familyName;
    QVariantList modelId;
    QList<int> modelIdRaw;
    QString modelName;
    QVariantList versionId;
    QList<int> versionIdRaw;
    QString versionName;
};

SysexIdentity::SysexIdentity(const SysexMessage* identityResponse, QObject* parent)
    : QObject(parent)
    , d(new SysexIdentityPrivate)
{
    const QList<int> messageBytes{identityResponse->bytesRaw()};
    // Skip the first four bytes, which are the message type identification
    // bytes (real/non-real time, channel, subid and subid2), which have been
    // checked by SysexHelper already before constructing the object. We
    // do not guarantee non-crashy behaviour for home-brewed versions of this
    // class's constructor.
    int position{4};
    // Pull out the manufacturer (1 or 3 bytes, depending on the first byte's value)
    int manufacturerIdByteCount{1};
    if (messageBytes[position] == 0x00) {
        manufacturerIdByteCount = 3;
    }
    for (int i = 0; i < manufacturerIdByteCount; ++i) {
        d->manufacturerIdRaw << messageBytes[position];
        d->manufacturerId << QVariant::fromValue<int>(messageBytes[position]);
        ++position;
    }
    d->manufacturerName = SysexIdTable::manufacturerNameFromId(d->manufacturerIdRaw);
    // Get the family ID out (2 bytes)
    for (int i = 0; i < 2; ++i) {
        d->familyIdRaw << messageBytes[position];
        d->familyId << QVariant::fromValue<int>(messageBytes[position]);
        ++position;
    }
    // Get the model ID out (2 bytes)
    for (int i = 0; i < 2; ++i) {
        d->modelIdRaw << messageBytes[position];
        d->modelId << QVariant::fromValue<int>(messageBytes[position]);
        ++position;
    }
    // And finally, the version (4 bytes)
    for (int i = 0; i < 4; ++i) {
        d->versionIdRaw << messageBytes[position];
        d->versionId << QVariant::fromValue<int>(messageBytes[position]);
        ++position;
    }
}

SysexIdentity::~SysexIdentity()
{
    delete d;
}

QVariantList SysexIdentity::manufacturerId() const
{
    return d->manufacturerId;
}

QList<int> SysexIdentity::manufacturerIdRaw() const
{
    return d->manufacturerIdRaw;
}

QString SysexIdentity::manufacturerName() const
{
    return d->manufacturerName;
}

QVariantList SysexIdentity::familyId() const
{
    return d->familyId;
}

QList<int> SysexIdentity::familyIdRaw() const
{
    return d->familyIdRaw;
}

QString SysexIdentity::familyName() const
{
    return d->familyName;
}

QVariantList SysexIdentity::modelId() const
{
    return d->modelId;
}

QList<int> SysexIdentity::modelIdRaw() const
{
    return d->modelIdRaw;
}

QString SysexIdentity::modelName() const
{
    return d->modelName;
}

QVariantList SysexIdentity::versionId() const
{
    return d->versionId;
}

QList<int> SysexIdentity::versionIdRaw() const
{
    return d->versionIdRaw;
}

QString SysexIdentity::versionName() const
{
    return d->versionName;
}
