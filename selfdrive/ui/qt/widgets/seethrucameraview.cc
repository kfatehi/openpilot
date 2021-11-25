#include "selfdrive/ui/qt/widgets/seethrucameraview.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include "selfdrive/common/params.h"
#include "selfdrive/common/util.h"
#include "selfdrive/ui/qt/util.h"

SeethruCameraViewWidget::SeethruCameraViewWidget(QWidget* parent) : sm({"driverState"}), CameraViewWidget(VISION_STREAM_RGB_WIDE, true, parent) {
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
        qDebug() << "face detected at x:" << face_x << " y: " << face_y;
    } else {
        // qDebug() << "no face detected";
        face_x = 0;
        face_y = 0;
    }
    adjustTransform();
}

void SeethruCameraViewWidget::adjustTransform() {
    mat4 device_transform = {{
        1.0,  0.0, 0.0, 0.0,
        0.0,  1.0, 0.0, 0.0,
        0.0,  0.0, 1.0, 0.0,
        0.0,  0.0, 0.0, 1.0,
    }};

#ifdef LEARNING_TRANSFORM

    QString transform_json = QString::fromStdString(util::read_file("/dev/shm/transform.json"));
    /* To learn more about this transform, we can create the file as follows:
    {
        "transform": [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0, 0.0, 1.0
        ]
    }
    */

    QJsonDocument json_d = QJsonDocument::fromJson(transform_json.toUtf8());
    QJsonArray json_a = json_d["transform"].toArray();

    mat4 frame_transform = {{
        (float)json_a.at(0).toDouble(),
        (float)json_a.at(1).toDouble(),
        (float)json_a.at(2).toDouble(),
        (float)json_a.at(3).toDouble(),
        (float)json_a.at(4).toDouble(),
        (float)json_a.at(5).toDouble(),
        (float)json_a.at(6).toDouble(),
        (float)json_a.at(7).toDouble(),
        (float)json_a.at(8).toDouble(),
        (float)json_a.at(9).toDouble(),
        (float)json_a.at(10).toDouble(),
        (float)json_a.at(11).toDouble(),
        (float)json_a.at(12).toDouble(),
        (float)json_a.at(13).toDouble(),
        (float)json_a.at(14).toDouble(),
        (float)json_a.at(15).toDouble()
    }};
    /*
    And we learn that the positions in the matrix do the following:
                                                                        
    [                                                                       
    stretchX(in,1,out),warp(top-left,0,top-right),0,translateY(left,0,right)
    rotate(left-up,0,left-down),stretchY(in,1,out),0,translateX(down,0,up)  
    0,0,0,0                                                                 
    tiltX(left-out,0,right-out),tiltY(top-in,0,top-out),?,zoom(in,1,out)    
    ]

    The question still remains: what transformations should be made based on the face position?

    The paper here https://dan.andersen.name/files/Andersen-ISMAR-2016.pdf uses depth information both
    on the driver/user and the environment. We don't have depth info here (although we could probably get it?)
    can we still achieve the illusion to some extent w/ only face position?
    */
#else
    float zoom = 0.3;
    float translateY = 0.0 + face_y;
    float translateX = 0.0 + face_x;
    
    mat4 frame_transform = {{
        1.0,  0.0, 0.0, translateY,
        0.0,  1.0, 0.0, translateX,
        0.0,  0.0, 1.0, 0.0,
        0.0,  0.0, 0.0, zoom,
    }};
#endif
    frame_mat = matmul(device_transform, frame_transform);
}