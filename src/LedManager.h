#pragma once

#include <QtQml>
#include <QObject>
#include <QColor>

class LedManager : public QObject
{
    Q_OBJECT
    QML_ATTACHED(LedManager)
    Q_PROPERTY(bool test READ test WRITE setTest NOTIFY testChanged)

public:
    LedManager(QObject *parent = nullptr);

    static LedManager *qmlAttachedProperties(QObject *object)
    {
        return new LedManager(object);
    }

    bool test() const;
    void setTest(bool test);

signals:
    void testChanged();

private:
    bool m_test{false};
};
