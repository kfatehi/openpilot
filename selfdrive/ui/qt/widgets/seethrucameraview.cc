#include "selfdrive/ui/qt/widgets/seethrucameraview.h"

#include <QDebug>

SeethruCameraViewWidget::SeethruCameraViewWidget(QWidget* parent) : sm({"driverState"}), CameraViewWidget(VISION_STREAM_RGB_BACK, true, parent) {
    // start a timer to poll for driverstate
    QTimer *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &SeethruCameraViewWidget::handleDriverState);
    t->start(10);
    qDebug() << "SeethruCamera started";
}

void SeethruCameraViewWidget::handleDriverState() {
    sm.update(0);
    cereal::DriverState::Reader driver_state = sm["driverState"].getDriverState();
    bool face_detected = driver_state.getFaceProb() > 0.5;
    if (face_detected) {
        auto fxy_list = driver_state.getFacePosition();
        float face_x = fxy_list[0];
        float face_y = fxy_list[1];
        qDebug() << "face detected at x:" << face_x << " y: " << face_y;
    } else {
        qDebug() << "no face detected";
    }
}