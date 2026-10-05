#include "diagnosewidget.h"

#include "keyboardcontroller.h"
#include "core/lightmount_device.h"
#include "core/lightmount_map.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>

DiagnoseWidget::DiagnoseWidget(KeyboardController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    auto* layout = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout();
    auto* title = new QLabel(i18n("Keyboard diagnostics"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    topRow->addWidget(title);
    topRow->addStretch();

    refreshButton_ = new QPushButton(
        QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Refresh"), this);
    topRow->addWidget(refreshButton_);
    layout->addLayout(topRow);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    summaryLabel_->setWordWrap(true);
    layout->addWidget(summaryLabel_);

    auto* cliLabel = new QLabel(i18n("Diagnostic CLI output:"), this);
    layout->addWidget(cliLabel);

    output_ = new QPlainTextEdit(this);
    output_->setReadOnly(true);
    output_->setLineWrapMode(QPlainTextEdit::NoWrap);
    output_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(output_, 1);

    connect(refreshButton_, &QPushButton::clicked,
            this, &DiagnoseWidget::refreshDiagnostics);
    connect(controller_, &KeyboardController::connectionChanged,
            this, [this](bool, const QString&) { refreshDiagnostics(); });

    refreshDiagnostics();
}

QString DiagnoseWidget::knownInformation() const {
    if(!controller_->isConnected()) {
        return i18n("No supported keyboard is connected.");
    }

    QStringList lines;
    lines << i18n("Model: %1", controller_->modelName().isEmpty()
                                   ? i18n("Unknown")
                                   : controller_->modelName());
    lines << i18n("Device node: %1", controller_->devicePath());

    if(controller_->usesBeQuietLightMount()) {
        lines << i18n("Manufacturer: be quiet!")
              << i18n("Series: Light Mount")
              << i18n("Protocol: be quiet! Mount vendor HID")
              << QStringLiteral("VID:PID: %1:%2")
                     .arg(krgb::LightMountDevice::kVendorId, 4, 16, QLatin1Char('0'))
                     .arg(krgb::LightMountDevice::kProductId, 4, 16, QLatin1Char('0'))
                     .toUpper()
              << i18n("Vendor HID interface: 2")
              << i18n("HID LampArray interface: 3")
              << i18n("Addressable RGB elements: 165")
              << i18n("Top bar: %1 LEDs", static_cast<int>(krgb::lightmount::kTopBarCount))
              << i18n("3D Media Wheel: 1 LED")
              << i18n("Keyboard: 109 key LEDs")
              << i18n("Left strip: %1 LEDs", static_cast<int>(krgb::lightmount::kLeftStripCount))
              << i18n("Right strip: %1 LEDs", static_cast<int>(krgb::lightmount::kRightStripCount))
              << i18n("Firmware version: not yet exposed by the Light Mount diagnostic CLI");
    } else if(controller_->usesLogitechHIDPP20()) {
        lines << i18n("Protocol: Logitech HID++ 2.0")
              << i18n("USB identity, firmware, feature set and capabilities are read below from krgb-cli logitech info.");
    } else {
        lines << i18n("Protocol: Alienware keyboard lighting")
              << i18n("USB identity and firmware information are read below from krgb-cli info where available.");
    }

    return lines.join(QLatin1Char('\n'));
}

QString DiagnoseWidget::cliProgram() const {
    const QString sibling = QCoreApplication::applicationDirPath() +
                            QStringLiteral("/krgb-cli");
    if(QFileInfo::exists(sibling)) {
        return sibling;
    }
    return QStringLiteral("krgb-cli");
}

QStringList DiagnoseWidget::cliArguments() const {
    if(controller_->usesBeQuietLightMount()) {
        return {QStringLiteral("lightmount"), QStringLiteral("info")};
    }
    if(controller_->usesLogitechHIDPP20()) {
        return {QStringLiteral("logitech"), QStringLiteral("info")};
    }
    return {QStringLiteral("info")};
}

void DiagnoseWidget::refreshDiagnostics() {
    summaryLabel_->setText(knownInformation());

    if(!controller_->isConnected()) {
        output_->setPlainText(i18n("No diagnostic query was run because no supported keyboard is connected."));
        return;
    }

    refreshButton_->setEnabled(false);
    output_->setPlainText(i18n("Reading diagnostic information…"));

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(cliProgram(), cliArguments());

    if(!process.waitForStarted(2000)) {
        output_->setPlainText(
            i18n("Could not start krgb-cli. Expected it next to the GUI executable or in PATH."));
        refreshButton_->setEnabled(true);
        return;
    }

    if(!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished();
        output_->setPlainText(i18n("Diagnostic query timed out."));
        refreshButton_->setEnabled(true);
        return;
    }

    QString text = QString::fromLocal8Bit(process.readAll()).trimmed();
    if(text.isEmpty()) {
        text = i18n("The diagnostic CLI returned no output (exit code %1).",
                    process.exitCode());
    } else if(process.exitCode() != 0) {
        text += i18n("\n\nExit code: %1", process.exitCode());
    }

    output_->setPlainText(text);
    refreshButton_->setEnabled(true);
}
