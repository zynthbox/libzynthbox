#include "LedManager.h"

class LedManager::Private
{
public:
    Private() {}

    QMap<ZynthboxBasics::Button, QColor> buttonColors;
};

LedManager::LedManager(QObject *parent)
    : QObject(parent)
    , d(new Private()) {
}

LedManager::~LedManager() {
    delete d;
}

QColor LedManager::buttonMenuColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonMenu);
}
void LedManager::setButtonMenuColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonMenu, color);
}

QColor LedManager::buttonNum1Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonNum1);
}
void LedManager::setButtonNum1Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonNum1, color);
}

QColor LedManager::buttonNum2Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonNum2);
}
void LedManager::setButtonNum2Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonNum2, color);
}

QColor LedManager::buttonNum3Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonNum3);
}
void LedManager::setButtonNum3Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonNum3, color);
}

QColor LedManager::buttonNum4Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonNum4);
}
void LedManager::setButtonNum4Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonNum4, color);
}

QColor LedManager::buttonNum5Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonNum5);
}
void LedManager::setButtonNum5Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonNum5, color);
}

QColor LedManager::buttonStarColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStar);
}
void LedManager::setButtonStarColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStar, color);
}

QColor LedManager::buttonModeColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonMode);
}
void LedManager::setButtonModeColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonMode, color);
}

QColor LedManager::buttonStep1Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep1);
}
void LedManager::setButtonStep1Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep1, color);
}

QColor LedManager::buttonStep2Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep2);
}
void LedManager::setButtonStep2Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep2, color);
}

QColor LedManager::buttonStep3Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep3);
}
void LedManager::setButtonStep3Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep3, color);
}

QColor LedManager::buttonStep4Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep4);
}
void LedManager::setButtonStep4Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep4, color);
}

QColor LedManager::buttonStep5Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep5);
}
void LedManager::setButtonStep5Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep5, color);
}

QColor LedManager::buttonStep6Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep6);
}
void LedManager::setButtonStep6Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep6, color);
}

QColor LedManager::buttonStep7Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep7);
}
void LedManager::setButtonStep7Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep7, color);
}

QColor LedManager::buttonStep8Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep8);
}
void LedManager::setButtonStep8Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep8, color);
}

QColor LedManager::buttonStep9Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep9);
}
void LedManager::setButtonStep9Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep9, color);
}

QColor LedManager::buttonStep10Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep10);
}
void LedManager::setButtonStep10Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep10, color);
}

QColor LedManager::buttonStep11Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep11);
}
void LedManager::setButtonStep11Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep11, color);
}

QColor LedManager::buttonStep12Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep12);
}
void LedManager::setButtonStep12Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep12, color);
}

QColor LedManager::buttonStep13Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep13);
}
void LedManager::setButtonStep13Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep13, color);
}

QColor LedManager::buttonStep14Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep14);
}
void LedManager::setButtonStep14Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep14, color);
}

QColor LedManager::buttonStep15Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep15);
}
void LedManager::setButtonStep15Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep15, color);
}

QColor LedManager::buttonStep16Color() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStep16);
}
void LedManager::setButtonStep16Color(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStep16, color);
}

QColor LedManager::buttonAltColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonAlt);
}
void LedManager::setButtonAltColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonAlt, color);
}

QColor LedManager::buttonRecordColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonRecord);
}
void LedManager::setButtonRecordColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonRecord, color);
}

QColor LedManager::buttonPlayColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonPlay);
}
void LedManager::setButtonPlayColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonPlay, color);
}

QColor LedManager::buttonMetronomeColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonMetronome);
}
void LedManager::setButtonMetronomeColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonMetronome, color);
}

QColor LedManager::buttonStopColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonStop);
}
void LedManager::setButtonStopColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonStop, color);
}

QColor LedManager::buttonBackColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonBack);
}
void LedManager::setButtonBackColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonBack, color);
}

QColor LedManager::buttonUpColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonUp);
}
void LedManager::setButtonUpColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonUp, color);
}

QColor LedManager::buttonSelectColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonSelect);
}
void LedManager::setButtonSelectColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonSelect, color);
}

QColor LedManager::buttonLeftColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonLeft);
}
void LedManager::setButtonLeftColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonLeft, color);
}

QColor LedManager::buttonDownColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonDown);
}
void LedManager::setButtonDownColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonDown, color);
}

QColor LedManager::buttonRightColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonRight);
}
void LedManager::setButtonRightColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonRight, color);
}

QColor LedManager::buttonGlobalColor() const {
    return d->buttonColors.value(ZynthboxBasics::Button::ButtonGlobal);
}
void LedManager::setButtonGlobalColor(QColor &color) {
    setButtonColor(ZynthboxBasics::Button::ButtonGlobal, color);
}

void LedManager::setButtonColor(ZynthboxBasics::Button button, QColor color) {
    if (d->buttonColors.value(button) != color) {
        if (color.red() > 0 || color.green() > 0 || color.blue() > 0 || color.alpha() > 0) {
            d->buttonColors.insert(button, color);
        } else {
            d->buttonColors.remove(button);
        }
        switch (button) {
            case ZynthboxBasics::Button::ButtonMenu:
                Q_EMIT buttonMenuColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonNum1:
                Q_EMIT buttonNum1ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonNum2:
                Q_EMIT buttonNum2ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonNum3:
                Q_EMIT buttonNum3ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonNum4:
                Q_EMIT buttonNum4ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonNum5:
                Q_EMIT buttonNum5ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStar:
                Q_EMIT buttonStarColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonMode:
                Q_EMIT buttonModeColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep1:
                Q_EMIT buttonStep1ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep2:
                Q_EMIT buttonStep2ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep3:
                Q_EMIT buttonStep3ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep4:
                Q_EMIT buttonStep4ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep5:
                Q_EMIT buttonStep5ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep6:
                Q_EMIT buttonStep6ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep7:
                Q_EMIT buttonStep7ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep8:
                Q_EMIT buttonStep8ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep9:
                Q_EMIT buttonStep9ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep10:
                Q_EMIT buttonStep10ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep11:
                Q_EMIT buttonStep11ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep12:
                Q_EMIT buttonStep12ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep13:
                Q_EMIT buttonStep13ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep14:
                Q_EMIT buttonStep14ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep15:
                Q_EMIT buttonStep15ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStep16:
                Q_EMIT buttonStep16ColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonAlt:
                Q_EMIT buttonAltColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonRecord:
                Q_EMIT buttonRecordColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonPlay:
                Q_EMIT buttonPlayColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonMetronome:
                Q_EMIT buttonMetronomeColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonStop:
                Q_EMIT buttonStopColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonBack:
                Q_EMIT buttonBackColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonUp:
                Q_EMIT buttonUpColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonSelect:
                Q_EMIT buttonSelectColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonLeft:
                Q_EMIT buttonLeftColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonDown:
                Q_EMIT buttonDownColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonRight:
                Q_EMIT buttonRightColorChanged();
                break;
            case ZynthboxBasics::Button::ButtonGlobal:
                Q_EMIT buttonGlobalColorChanged();
                break;
            default:
                break;
        }
        Q_EMIT ledColorChanged(button, color);
    }
}

QVariantMap LedManager::ledColors() const {
    QVariantMap colors;
    for (auto it = d->buttonColors.constBegin(); it != d->buttonColors.constEnd(); ++it) {
        colors.insert(QString::number(static_cast<int>(ZynthboxBasics::instance()->buttonId(it.key()))), it.value());
    }
    return colors;
}

void LedManager::clearAllLedColors() {
    if (!d->buttonColors.isEmpty()) {
        d->buttonColors.clear();
        Q_EMIT buttonMenuColorChanged();
        Q_EMIT buttonNum1ColorChanged();
        Q_EMIT buttonNum2ColorChanged();
        Q_EMIT buttonNum3ColorChanged();
        Q_EMIT buttonNum4ColorChanged();
        Q_EMIT buttonNum5ColorChanged();
        Q_EMIT buttonStarColorChanged();
        Q_EMIT buttonModeColorChanged();
        Q_EMIT buttonStep1ColorChanged();
        Q_EMIT buttonStep2ColorChanged();
        Q_EMIT buttonStep3ColorChanged();
        Q_EMIT buttonStep4ColorChanged();
        Q_EMIT buttonStep5ColorChanged();
        Q_EMIT buttonStep6ColorChanged();
        Q_EMIT buttonStep7ColorChanged();
        Q_EMIT buttonStep8ColorChanged();
        Q_EMIT buttonStep9ColorChanged();
        Q_EMIT buttonStep10ColorChanged();
        Q_EMIT buttonStep11ColorChanged();
        Q_EMIT buttonStep12ColorChanged();
        Q_EMIT buttonStep13ColorChanged();
        Q_EMIT buttonStep14ColorChanged();
        Q_EMIT buttonStep15ColorChanged();
        Q_EMIT buttonStep16ColorChanged();
        Q_EMIT buttonAltColorChanged();
        Q_EMIT buttonRecordColorChanged();
        Q_EMIT buttonPlayColorChanged();
        Q_EMIT buttonMetronomeColorChanged();
        Q_EMIT buttonStopColorChanged();
        Q_EMIT buttonBackColorChanged();
        Q_EMIT buttonUpColorChanged();
        Q_EMIT buttonSelectColorChanged();
        Q_EMIT buttonLeftColorChanged();
        Q_EMIT buttonDownColorChanged();
        Q_EMIT buttonRightColorChanged();
        Q_EMIT buttonGlobalColorChanged();

        QColor noColor;
        Q_EMIT ledColorChanged(ZynthboxBasics::Button::ButtonInvalid, noColor);
    }
}