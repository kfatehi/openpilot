#include "selfdrive/ui/qt/widgets/seethrucameraview.h"

#include <QDebug>

#ifdef LEARNING_TRANSFORM

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>

#include "selfdrive/common/params.h"
#include "selfdrive/common/util.h"
#include "selfdrive/ui/qt/util.h"

#endif

SeethruCameraViewWidget::SeethruCameraViewWidget(QWidget* parent) : sm({"driverState"}), CameraViewWidget(VISION_STREAM_RGB_WIDE, true, parent) {
    // start a timer to poll for driverstate
    QTimer *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &SeethruCameraViewWidget::handleDriverState);
    t->start(0);
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

    -- cool this is pretty nice.

    Note for next time I work on this.....

    Realistically we can pretty much achieve this for the purpose that some people want it for, that is,
    to be able to see objects through the device from their vantage point in the car.

    If we provide a way to setup the initial transform, given the use case of driving a car, it's already in good shape and
    eliminates the need for depth perception on the driver or the roadway as we'll already be calibrated.

    Would be great to be able to put it in your car and then switch to this "see thru mode" and then use simple gestures to calibrate it:
    https://doc.qt.io/qt-5/gestures-overview.html
 
    Then, the face position as it changes, would alter the view slightly, perhaps enough to justify its usage, although
    I am skeptical if it would be anything more than a novelty at that point, given the coarse adjustment would have
    already been made and the jitter problem would not be worth the potential "fine" adjustment you would get out of it:

    There is a significant amount of "jitter" in the readout from the driver monitoring model. I'll
    have to make a video to demonstrate this later, as I don't think people realize there is so much of this, but effectively using
    this as an input to alter a projected view would be very nauseating. This could be mitigated by doing some kind of interpolation
    so that it does not jump from one predicted head position to another immediately but I don't know what the performance penalties are like
    as you start cranking the frequencies up AND try to clean the data AND try to represent the cleaned data in terms of updating the matrix.

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