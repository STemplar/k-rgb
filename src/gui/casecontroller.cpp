#include "casecontroller.h"

#include <QTimer>
#include <KLocalizedString>

CaseController::CaseController(QObject* parent) : QObject(parent) {
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(3000);
    connect(pollTimer_, &QTimer::timeout, this, &CaseController::refresh);
    pollTimer_->start();
    refresh();
}

CaseController::~CaseController() = default;

QString CaseController::description() const {
    if(!available_) {
        return i18n("No case controller");
    }
    return i18np("AW-ELC controller, %1 zone", "AW-ELC controller, %1 zones", zoneCount_);
}

void CaseController::refresh() {
    const bool nowAvailable = !krgb::AlienFXDevice::findDevicePath().empty();
    if(nowAvailable != available_) {
        available_ = nowAvailable;
        if(available_) {
            ensureOpen();  // populate zoneCount_ so the editor knows the size
        } else if(device_.isOpen()) {
            device_.close();
            zoneCount_ = 0;
        }
        Q_EMIT availabilityChanged(available_);
    }
}

bool CaseController::ensureOpen() {
    if(device_.isOpen()) {
        return true;
    }
    std::string err;
    if(!device_.open(&err)) {
        Q_EMIT error(QString::fromStdString(err));
        return false;
    }
    zoneCount_ = device_.zoneCount();
    return true;
}

bool CaseController::applySolid(const QColor& color) {
    if(!ensureOpen()) {
        return false;
    }
    if(!device_.setSolid(color.red(), color.green(), color.blue())) {
        Q_EMIT error(i18n("Failed to set case colour."));
        return false;
    }
    return true;
}

bool CaseController::applyZones(const QHash<int, QColor>& zoneColors) {
    if(!ensureOpen()) {
        return false;
    }
    std::vector<krgb::AlienFXDevice::ZoneColor> colors(zoneCount_, {0, 0, 0});
    for(auto it = zoneColors.cbegin(); it != zoneColors.cend(); ++it) {
        if(it.key() >= 0 && it.key() < zoneCount_) {
            const QColor& c = it.value();
            colors[it.key()] = {static_cast<std::uint8_t>(c.red()),
                                static_cast<std::uint8_t>(c.green()),
                                static_cast<std::uint8_t>(c.blue())};
        }
    }
    if(!device_.setZoneColors(colors)) {
        Q_EMIT error(i18n("Failed to set per-zone case colours."));
        return false;
    }
    return true;
}

bool CaseController::identify(const QList<int>& zones) {
    QHash<int, QColor> map;
    for(int z : zones) {
        map.insert(z, QColor(255, 255, 255));
    }
    return applyZones(map);  // selected zones white, the rest off
}

bool CaseController::applyOff() {
    if(!ensureOpen()) {
        return false;
    }
    if(!device_.setOff()) {
        Q_EMIT error(i18n("Failed to turn case lighting off."));
        return false;
    }
    return true;
}
