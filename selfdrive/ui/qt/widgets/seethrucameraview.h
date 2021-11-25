#pragma once

#include "selfdrive/ui/qt/widgets/cameraview.h"

class SeethruCameraViewWidget : public CameraViewWidget {
  Q_OBJECT

public:
  explicit SeethruCameraViewWidget(QWidget* parent = nullptr);

protected:
  void adjustTransform();
protected slots:
  void handleDriverState();

private:
  SubMaster sm;
  float face_x;
  float face_y;
  bool face_detected;
};
