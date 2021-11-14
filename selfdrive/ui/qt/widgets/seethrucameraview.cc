#include "selfdrive/ui/qt/widgets/seethrucameraview.h"

#include <QDebug>

SeethruCameraViewWidget::SeethruCameraViewWidget(QWidget* parent) : CameraViewWidget(VISION_STREAM_RGB_BACK, true, parent) {

//   QTimer *t = new QTimer(this);
    connect(this, &CameraViewWidget::frameUpdated, this, &SeethruCameraViewWidget::handleFrameUpdate);
    connect(this, &CameraViewWidget::clicked, this, &SeethruCameraViewWidget::handleClick);
//   t->start(10);
    qDebug() << "SeethruCameraViewWidget constructed";
}


void SeethruCameraViewWidget::handleClick() {
      qDebug() << "SeethruCameraViewWidget handleClick";

}
void SeethruCameraViewWidget::handleFrameUpdate() {
    qDebug() << "SeethruCameraViewWidget handleFrameUpdate";

}
