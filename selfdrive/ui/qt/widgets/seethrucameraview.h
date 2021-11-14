#pragma once

#include "selfdrive/ui/qt/widgets/cameraview.h"

class SeethruCameraViewWidget : public CameraViewWidget {
  Q_OBJECT

public:
//   using QOpenGLWidget::QOpenGLWidget;
  explicit SeethruCameraViewWidget(QWidget* parent = nullptr);
//   ~SeethruCameraViewWidget();
//   void setStreamType(VisionStreamType type);
//   void setBackgroundColor(QColor color);

// signals:
//   void clicked();
//   void frameUpdated();

// protected:
//   void paintGL() override;
//   void resizeGL(int w, int h) override;
//   void initializeGL() override;
//   void hideEvent(QHideEvent *event) override;
//   void mouseReleaseEvent(QMouseEvent *event) override;
//   void updateFrameMat(int w, int h);
//   std::unique_ptr<VisionIpcClient> vipc_client;

protected slots:
  void handleClick();
  void handleFrameUpdate();
  void handleDriverState();

private:
  SubMaster sm;

//   bool zoomed_view;
//   VisionBuf *latest_frame = nullptr;
//   GLuint frame_vao, frame_vbo, frame_ibo;
//   mat4 frame_mat;
//   std::unique_ptr<EGLImageTexture> texture[UI_BUF_COUNT];
//   QOpenGLShaderProgram *program;

//   VisionStreamType stream_type;
//   QColor bg = QColor("#000000");
};
