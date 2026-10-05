#include "diagnosewidget.h"

#include "keyboardcontroller.h"

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
    auto* title = new QLabel(i18n("Device diagnostics"), this);
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

    const QString model = controller_->modelName().isEmpty()
        ? i18n("Unknown") : controller_->modelName();

    if(controller_->usesBeQuietLightMount()) {
        return i18n("%1 — be quiet! Light Mount diagnostics are read from the vendor HID and HID LampArray interfaces.",
                    model);
    }
    if(controller_->usesLogitechHIDPP20()) {
        return i18n("%1 — diagnostics are discovered from the device's HID++ 2.0 Feature Set.",
                    model);
    }
    return i18n("%1 — device diagnostics.", model);
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
