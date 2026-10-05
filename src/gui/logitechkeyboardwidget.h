#pragma once

#include <QWidget>

#include <cstdint>
#include <vector>

class KColorButton;
class KeyboardController;
class QLabel;
class QComboBox;
class QPushButton;
class QSlider;
class QSpinBox;

class LogitechKeyboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogitechKeyboardWidget(KeyboardController* controller,
                                    QWidget* parent = nullptr);

    void refreshFromDevice();
    void loadCurrentProfile();

private:
    enum class EffectSource {
        None,
        ColorLed8070,
        RgbEffects8071,
    };

    struct EffectEntry {
        std::uint16_t id = 0;
        std::uint16_t capabilities = 0;
        std::uint16_t periodMs = 0;
    };

    QString effectName(std::uint16_t id) const;
    bool effectIsImplemented(std::uint16_t id) const;
    void rebuildEffectList();
    void updateControls();
    void applyAndSave();

    KeyboardController* controller_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    KColorButton* colorButton_ = nullptr;
    QSpinBox* periodSpin_ = nullptr;
    QComboBox* directionCombo_ = nullptr;
    QSlider* intensitySlider_ = nullptr;
    QLabel* intensityValue_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* offButton_ = nullptr;

    std::vector<EffectEntry> effects_;
    EffectSource effectSource_ = EffectSource::None;
    bool loading_ = false;
};
