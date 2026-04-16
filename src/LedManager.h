#pragma once

#include <QtQml>
#include <QObject>
#include <QColor>
#include <QMap>
#include <QVariant>
#include "ZynthboxBasics.h"


class LedManager : public QObject
{
    Q_OBJECT
    QML_ATTACHED(LedManager)

    /**
     * /brief LED colors for the buttons on the Zynthbox
     */
    Q_PROPERTY(QColor buttonMenuColor READ buttonMenuColor WRITE setButtonMenuColor NOTIFY buttonMenuColorChanged)
    Q_PROPERTY(QColor buttonNum1Color READ buttonNum1Color WRITE setButtonNum1Color NOTIFY buttonNum1ColorChanged)
    Q_PROPERTY(QColor buttonNum2Color READ buttonNum2Color WRITE setButtonNum2Color NOTIFY buttonNum2ColorChanged)
    Q_PROPERTY(QColor buttonNum3Color READ buttonNum3Color WRITE setButtonNum3Color NOTIFY buttonNum3ColorChanged)
    Q_PROPERTY(QColor buttonNum4Color READ buttonNum4Color WRITE setButtonNum4Color NOTIFY buttonNum4ColorChanged)
    Q_PROPERTY(QColor buttonNum5Color READ buttonNum5Color WRITE setButtonNum5Color NOTIFY buttonNum5ColorChanged)
    Q_PROPERTY(QColor buttonStarColor READ buttonStarColor WRITE setButtonStarColor NOTIFY buttonStarColorChanged)
    Q_PROPERTY(QColor buttonModeColor READ buttonModeColor WRITE setButtonModeColor NOTIFY buttonModeColorChanged)
    Q_PROPERTY(QColor buttonStep1Color READ buttonStep1Color WRITE setButtonStep1Color NOTIFY buttonStep1ColorChanged)
    Q_PROPERTY(QColor buttonStep2Color READ buttonStep2Color WRITE setButtonStep2Color NOTIFY buttonStep2ColorChanged)
    Q_PROPERTY(QColor buttonStep3Color READ buttonStep3Color WRITE setButtonStep3Color NOTIFY buttonStep3ColorChanged)
    Q_PROPERTY(QColor buttonStep4Color READ buttonStep4Color WRITE setButtonStep4Color NOTIFY buttonStep4ColorChanged)
    Q_PROPERTY(QColor buttonStep5Color READ buttonStep5Color WRITE setButtonStep5Color NOTIFY buttonStep5ColorChanged)
    Q_PROPERTY(QColor buttonStep6Color READ buttonStep6Color WRITE setButtonStep6Color NOTIFY buttonStep6ColorChanged)
    Q_PROPERTY(QColor buttonStep7Color READ buttonStep7Color WRITE setButtonStep7Color NOTIFY buttonStep7ColorChanged)
    Q_PROPERTY(QColor buttonStep8Color READ buttonStep8Color WRITE setButtonStep8Color NOTIFY buttonStep8ColorChanged)
    Q_PROPERTY(QColor buttonStep9Color READ buttonStep9Color WRITE setButtonStep9Color NOTIFY buttonStep9ColorChanged)
    Q_PROPERTY(QColor buttonStep10Color READ buttonStep10Color WRITE setButtonStep10Color NOTIFY buttonStep10ColorChanged)
    Q_PROPERTY(QColor buttonStep11Color READ buttonStep11Color WRITE setButtonStep11Color NOTIFY buttonStep11ColorChanged)
    Q_PROPERTY(QColor buttonStep12Color READ buttonStep12Color WRITE setButtonStep12Color NOTIFY buttonStep12ColorChanged)
    Q_PROPERTY(QColor buttonStep13Color READ buttonStep13Color WRITE setButtonStep13Color NOTIFY buttonStep13ColorChanged)
    Q_PROPERTY(QColor buttonStep14Color READ buttonStep14Color WRITE setButtonStep14Color NOTIFY buttonStep14ColorChanged)
    Q_PROPERTY(QColor buttonStep15Color READ buttonStep15Color WRITE setButtonStep15Color NOTIFY buttonStep15ColorChanged)
    Q_PROPERTY(QColor buttonStep16Color READ buttonStep16Color WRITE setButtonStep16Color NOTIFY buttonStep16ColorChanged)
    Q_PROPERTY(QColor buttonAltColor READ buttonAltColor WRITE setButtonAltColor NOTIFY buttonAltColorChanged)
    Q_PROPERTY(QColor buttonRecordColor READ buttonRecordColor WRITE setButtonRecordColor NOTIFY buttonRecordColorChanged)
    Q_PROPERTY(QColor buttonPlayColor READ buttonPlayColor WRITE setButtonPlayColor NOTIFY buttonPlayColorChanged)
    Q_PROPERTY(QColor buttonMetronomeColor READ buttonMetronomeColor WRITE setButtonMetronomeColor NOTIFY buttonMetronomeColorChanged)
    Q_PROPERTY(QColor buttonStopColor READ buttonStopColor WRITE setButtonStopColor NOTIFY buttonStopColorChanged)
    Q_PROPERTY(QColor buttonBackColor READ buttonBackColor WRITE setButtonBackColor NOTIFY buttonBackColorChanged)
    Q_PROPERTY(QColor buttonUpColor READ buttonUpColor WRITE setButtonUpColor NOTIFY buttonUpColorChanged)
    Q_PROPERTY(QColor buttonSelectColor READ buttonSelectColor WRITE setButtonSelectColor NOTIFY buttonSelectColorChanged)
    Q_PROPERTY(QColor buttonLeftColor READ buttonLeftColor WRITE setButtonLeftColor NOTIFY buttonLeftColorChanged)
    Q_PROPERTY(QColor buttonDownColor READ buttonDownColor WRITE setButtonDownColor NOTIFY buttonDownColorChanged)
    Q_PROPERTY(QColor buttonRightColor READ buttonRightColor WRITE setButtonRightColor NOTIFY buttonRightColorChanged)
    Q_PROPERTY(QColor buttonGlobalColor READ buttonGlobalColor WRITE setButtonGlobalColor NOTIFY buttonGlobalColorChanged)

public:
    LedManager(QObject *parent = nullptr);
    ~LedManager() override;

    static LedManager *qmlAttachedProperties(QObject *object) {
        return new LedManager(object);
    }

    QColor buttonMenuColor() const;
    void setButtonMenuColor(QColor &color);
    Q_SIGNAL void buttonMenuColorChanged();

    QColor buttonNum1Color() const;
    void setButtonNum1Color(QColor &color);
    Q_SIGNAL void buttonNum1ColorChanged();

    QColor buttonNum2Color() const;
    void setButtonNum2Color(QColor &color);
    Q_SIGNAL void buttonNum2ColorChanged();

    QColor buttonNum3Color() const;
    void setButtonNum3Color(QColor &color);
    Q_SIGNAL void buttonNum3ColorChanged();

    QColor buttonNum4Color() const;
    void setButtonNum4Color(QColor &color);
    Q_SIGNAL void buttonNum4ColorChanged();

    QColor buttonNum5Color() const;
    void setButtonNum5Color(QColor &color);
    Q_SIGNAL void buttonNum5ColorChanged();

    QColor buttonStarColor() const;
    void setButtonStarColor(QColor &color);
    Q_SIGNAL void buttonStarColorChanged();

    QColor buttonModeColor() const;
    void setButtonModeColor(QColor &color);
    Q_SIGNAL void buttonModeColorChanged();

    QColor buttonStep1Color() const;
    void setButtonStep1Color(QColor &color);
    Q_SIGNAL void buttonStep1ColorChanged();

    QColor buttonStep2Color() const;
    void setButtonStep2Color(QColor &color);
    Q_SIGNAL void buttonStep2ColorChanged();

    QColor buttonStep3Color() const;
    void setButtonStep3Color(QColor &color);
    Q_SIGNAL void buttonStep3ColorChanged();

    QColor buttonStep4Color() const;
    void setButtonStep4Color(QColor &color);
    Q_SIGNAL void buttonStep4ColorChanged();

    QColor buttonStep5Color() const;
    void setButtonStep5Color(QColor &color);
    Q_SIGNAL void buttonStep5ColorChanged();

    QColor buttonStep6Color() const;
    void setButtonStep6Color(QColor &color);
    Q_SIGNAL void buttonStep6ColorChanged();

    QColor buttonStep7Color() const;
    void setButtonStep7Color(QColor &color);
    Q_SIGNAL void buttonStep7ColorChanged();

    QColor buttonStep8Color() const;
    void setButtonStep8Color(QColor &color);
    Q_SIGNAL void buttonStep8ColorChanged();

    QColor buttonStep9Color() const;
    void setButtonStep9Color(QColor &color);
    Q_SIGNAL void buttonStep9ColorChanged();

    QColor buttonStep10Color() const;
    void setButtonStep10Color(QColor &color);
    Q_SIGNAL void buttonStep10ColorChanged();

    QColor buttonStep11Color() const;
    void setButtonStep11Color(QColor &color);
    Q_SIGNAL void buttonStep11ColorChanged();

    QColor buttonStep12Color() const;
    void setButtonStep12Color(QColor &color);
    Q_SIGNAL void buttonStep12ColorChanged();

    QColor buttonStep13Color() const;
    void setButtonStep13Color(QColor &color);
    Q_SIGNAL void buttonStep13ColorChanged();

    QColor buttonStep14Color() const;
    void setButtonStep14Color(QColor &color);
    Q_SIGNAL void buttonStep14ColorChanged();

    QColor buttonStep15Color() const;
    void setButtonStep15Color(QColor &color);
    Q_SIGNAL void buttonStep15ColorChanged();

    QColor buttonStep16Color() const;
    void setButtonStep16Color(QColor &color);
    Q_SIGNAL void buttonStep16ColorChanged();

    QColor buttonAltColor() const;
    void setButtonAltColor(QColor &color);
    Q_SIGNAL void buttonAltColorChanged();

    QColor buttonRecordColor() const;
    void setButtonRecordColor(QColor &color);
    Q_SIGNAL void buttonRecordColorChanged();

    QColor buttonPlayColor() const;
    void setButtonPlayColor(QColor &color);
    Q_SIGNAL void buttonPlayColorChanged();

    QColor buttonMetronomeColor() const;
    void setButtonMetronomeColor(QColor &color);
    Q_SIGNAL void buttonMetronomeColorChanged();

    QColor buttonStopColor() const;
    void setButtonStopColor(QColor &color);
    Q_SIGNAL void buttonStopColorChanged();

    QColor buttonBackColor() const;
    void setButtonBackColor(QColor &color);
    Q_SIGNAL void buttonBackColorChanged();

    QColor buttonUpColor() const;
    void setButtonUpColor(QColor &color);
    Q_SIGNAL void buttonUpColorChanged();

    QColor buttonSelectColor() const;
    void setButtonSelectColor(QColor &color);
    Q_SIGNAL void buttonSelectColorChanged();

    QColor buttonLeftColor() const;
    void setButtonLeftColor(QColor &color);
    Q_SIGNAL void buttonLeftColorChanged();

    QColor buttonDownColor() const;
    void setButtonDownColor(QColor &color);
    Q_SIGNAL void buttonDownColorChanged();

    QColor buttonRightColor() const;
    void setButtonRightColor(QColor &color);
    Q_SIGNAL void buttonRightColorChanged();

    QColor buttonGlobalColor() const;
    void setButtonGlobalColor(QColor &color);
    Q_SIGNAL void buttonGlobalColorChanged();

    /**
     * /brief Signal emitted when the color of a button LED changes
     * Handle this signal to get notified when any of the button LED colors change.
     * 
     * @param button The button whose LED color changed
     * @param color The new color of the button's LED
     */
    Q_SIGNAL void ledColorChanged(ZynthboxBasics::Button button, QColor &color);

    /**
     * /brief Resets the color of a specific button LED
     * 
     * @param button The button whose LED color should be reset to its default value
     */
    Q_INVOKABLE void resetColor(ZynthboxBasics::Button button);
    /**
     * /brief Resets the colors of all button LEDs
     */
    Q_INVOKABLE void resetAllColors();
    /**
     * /brief Gets the current colors of all button LEDs as a QVariantMap
     * The keys of the map are button identifiers (as strings) and the values are the corresponding LED colors (as QColor).
     */
    Q_INVOKABLE QVariantMap ledColors() const;

private:
    class Private;
    Private *d;
};
