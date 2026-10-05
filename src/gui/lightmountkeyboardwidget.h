#pragma once

#include <QWidget>

class KeyboardController;
class KeyboardWidget;
class KColorButton;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

class LightMountKeyboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit LightMountKeyboardWidget(KeyboardController* controller,
                                      QWidget* parent = nullptr);

public Q_SLOTS:
    void loadCurrentProfile();

private Q_SLOTS:
    void updateControls();
    void applyAndSave();
    void paintSelection();

private:
    bool isPerKeyMode() const;
    bool isRainbowMode() const;
    int selectedEffectCode() const;
    void setDirectionRows(bool cardinal, bool rotational);
    void setSliderText(QLabel* label, int value);

    KeyboardController* controller_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QComboBox* colorModeCombo_ = nullptr;
    QComboBox* directionCombo_ = nullptr;
    KColorButton* primaryColor_ = nullptr;
    KColorButton* secondaryColor_ = nullptr;
    QSlider* speedSlider_ = nullptr;
    QLabel* speedValue_ = nullptr;
    QSlider* brightnessSlider_ = nullptr;
    QLabel* brightnessValue_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    KeyboardWidget* keyboard_ = nullptr;
    QLabel* selectionLabel_ = nullptr;
    bool loading_ = false;
};
