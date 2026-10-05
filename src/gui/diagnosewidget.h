#pragma once

#include <QWidget>

class KeyboardController;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class DiagnoseWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiagnoseWidget(KeyboardController* controller, QWidget* parent = nullptr);

public Q_SLOTS:
    void refreshDiagnostics();

private:
    QString knownInformation() const;
    QString cliProgram() const;
    QStringList cliArguments() const;

    KeyboardController* controller_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QPlainTextEdit* output_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
};
