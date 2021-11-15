#include "selfdrive/ui/qt/widgets/seethrucameraview.h"

#include <QDebug>

SeethruCameraViewWidget::SeethruCameraViewWidget(QWidget* parent) : sm({"driverState"}), CameraViewWidget(VISION_STREAM_RGB_BACK, true, parent) {
    // start a timer to poll for driverstate
    QTimer *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &SeethruCameraViewWidget::handleDriverState);
    t->start(10);
    qDebug() << "SeethruCamera started";
    face_detected = false;
}

void SeethruCameraViewWidget::handleDriverState() {
    sm.update(0);
    cereal::DriverState::Reader driver_state = sm["driverState"].getDriverState();
    face_detected = driver_state.getFaceProb() > 0.6;
    if (face_detected) {
        auto fxy_list = driver_state.getFacePosition();
        face_x = fxy_list[0];
        face_y = fxy_list[1];
        // qDebug() << "face detected at x:" << face_x << " y: " << face_y;
    } else {
        // qDebug() << "no face detected";
        
    }
}

void SeethruCameraViewWidget::resizeGL(int w, int h) {
    qDebug() << "resizeGL override" << w << h << face_x << face_y;
    updateFrameMat((int)(w/face_x), (int)(h/face_y));
}