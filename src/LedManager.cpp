#include "LedManager.h"


LedManager::LedManager(QObject *parent)
    : QObject(parent)
{
}

bool LedManager::test() const
{
    return m_test;
}

void LedManager::setTest(bool test)
{
    m_test = test;
    Q_EMIT testChanged();
}
