#include "selfdrive/ui/qt/widgets/seethrucameraview.h"

#include <QDebug>

SeethruCameraViewWidget::SeethruCameraViewWidget(QWidget* parent) : sm({"driverState"}), CameraViewWidget(VISION_STREAM_RGB_BACK, true, parent) {

//   QTimer *t = new QTimer(this);
    connect(this, &CameraViewWidget::frameUpdated, this, &SeethruCameraViewWidget::handleFrameUpdate);
    connect(this, &CameraViewWidget::clicked, this, &SeethruCameraViewWidget::handleClick);
//   t->start(10);
    qDebug() << "SeethruCameraViewWidget constructed";
    sm.update(0);
}


void SeethruCameraViewWidget::handleClick() {
      qDebug() << "SeethruCameraViewWidget handleClick";
    sm.update(0);

    handleDriverState();
}
void SeethruCameraViewWidget::handleFrameUpdate() {
    qDebug() << "SeethruCameraViewWidget handleFrameUpdate";

}

void SeethruCameraViewWidget::handleDriverState() {
    cereal::DriverState::Reader driver_state = sm["driverState"].getDriverState();
    bool face_detected = driver_state.getFaceProb() > 0.5;
    if (face_detected) {
        auto fxy_list = driver_state.getFacePosition();
        float face_x = fxy_list[0];
        float face_y = fxy_list[1];
        qDebug() << "face detected, x:" << face_x << " y: " << face_y;
    }
}