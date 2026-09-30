#pragma once

#include <KMainWindow>
#include <QVector>

#include "settings.h"

class QActionGroup;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QMenu;
class QPushButton;
class QSlider;
class QTabWidget;
class QWidget;
class KColorButton;
class KStatusNotifierItem;
class KeyboardController;
class KeyboardWidget;
class CaseController;
class ZoneGridWidget;

class MainWindow : public KMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(KeyboardController* controller, CaseController* caseController,
                        QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void onApply();
    void onOff();
    void onModeChanged();
    void onBrightnessChanged(int value);
    void onAutostartToggled(bool checked);
    void onTrayAutostartToggled(bool checked);
    void onConnectionChanged(bool connected, const QString& path);
    void onError(const QString& message);

    // Profiles
    void onProfileSelected(int index);
    void onNewProfile();
    void onRenameProfile();
    void onDeleteProfile();

    // Per-key editor
    void onPaintSelection();
    void onOffSelection();
    void onFillAll();
    void onPerKeyChanged();

    // Case lighting
    void onCaseApply();
    void onCaseOff();
    void onCaseAvailabilityChanged(bool available);
    void onCaseZoneApply();
    void onCaseZoneIdentify();

private:
    struct ModeEntry {
        QString name;
        int     value;          // krgb::Mode value
        bool    solid;          // route through applySolid()
        bool    rainbow;        // route through applyRainbow() (per-key static rainbow)
        bool    perkey;         // route through applyPerKey() (custom per-key editor)
        bool    zones;          // route through applyZones() (custom zone editor)
        bool    usesColor;
        bool    usesSpeed;
        bool    usesDirection;
    };

    void             buildUi();
    QWidget*         buildKeyboardPage();
    QWidget*         buildCasePage();
    void             saveZoneLayout();
    void             loadZoneLayout();
    void             setupTray();
    void             populateModes();
    void             loadProfileIntoUi(const LightingSettings& s);
    void             switchToProfile(const QString& name, bool apply);
    void             refreshProfileCombo();
    void             rebuildProfilesMenu();
    LightingSettings currentSettings() const;
    void             writeAutostartEntry(const QString& path, const QString& name, const QString& args);
    QString          autostartFilePath() const;
    QString          trayAutostartFilePath() const;

    KeyboardController* controller_;
    CaseController*     caseController_;
    QVector<ModeEntry>  modes_;
    bool                loading_ = false;

    QComboBox*    profileCombo_     = nullptr;
    QPushButton*  newProfileBtn_    = nullptr;
    QPushButton*  renameProfileBtn_ = nullptr;
    QPushButton*  deleteProfileBtn_ = nullptr;

    QLabel*       statusLabel_      = nullptr;
    KColorButton* colorButton_      = nullptr;
    QComboBox*    modeCombo_        = nullptr;
    QComboBox*    speedCombo_       = nullptr;
    QComboBox*    directionCombo_   = nullptr;
    QSlider*      brightnessSlider_ = nullptr;
    QLabel*       brightnessValue_  = nullptr;
    QCheckBox*    autostartCheck_     = nullptr;
    QCheckBox*    trayAutostartCheck_ = nullptr;
    QPushButton*  applyButton_      = nullptr;
    QPushButton*  offButton_        = nullptr;

    QWidget*        perKeyPanel_    = nullptr;
    KeyboardWidget* keyboardWidget_ = nullptr;
    QLabel*         selectionLabel_ = nullptr;

    QWidget*               keyboardZonePanel_ = nullptr;
    QVector<KColorButton*>  keyboardZoneButtons_;

    QTabWidget*     tabs_               = nullptr;
    QWidget*        casePage_           = nullptr;
    KColorButton*   caseColorButton_    = nullptr;
    QPushButton*    caseApplyButton_    = nullptr;
    QPushButton*    caseOffButton_      = nullptr;
    QCheckBox*      caseArrangeCheck_   = nullptr;
    ZoneGridWidget* zoneGrid_           = nullptr;
    QLabel*         zoneSelectionLabel_ = nullptr;
    bool            casePerZone_        = false;

    KStatusNotifierItem* tray_         = nullptr;
    QMenu*               profilesMenu_ = nullptr;
    QActionGroup*        profileGroup_ = nullptr;
};
