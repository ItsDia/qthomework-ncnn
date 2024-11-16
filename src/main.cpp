#include "yolov8_pose.h"
#define ACCESS_MASK Windows_ACCESS_MASK // 重命名 Windows 的 ACCESS_MASK

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>

#undef ACCESS_MASK // 取消重命名，避免影响后续代码
#include <float.h>
#include <stdio.h>
#include <vector>
#include <chrono>
#include "BYTETracker.h"
#include <QCoreApplication>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QFile>

#define YOLOV8_PARAM "C:/Users/i4A/CLionProjects/yolov8-pose-fall-detection/weights/yolov8-pose-human-opt.param"
#define YOLOV8_BIN "C:/Users/i4A/CLionProjects/yolov8-pose-fall-detection/weights/yolov8-pose-human-opt.bin"
#define SAVE_PATH "outputs"

std::unique_ptr<Yolov8Pose> yolov8Pose(new Yolov8Pose(YOLOV8_PARAM, YOLOV8_BIN, true));

class WebSocketServer : public QObject {
    Q_OBJECT

public:
    WebSocketServer(quint16 port, QObject *parent = nullptr)
        : QObject(parent), m_server(new QWebSocketServer(QStringLiteral("Image Server"), QWebSocketServer::NonSecureMode, this)) {
        if (m_server->listen(QHostAddress::Any, port)) {
            connect(m_server, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);
            qDebug() << "WebSocket server listening on port" << port;
        }
    }

    ~WebSocketServer() {
        m_server->close();
        qDeleteAll(m_clients.begin(), m_clients.end());
    }

    void sendImage(const cv::Mat &image) {
        cv::Mat rgbImage;
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);

        QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
        if (qImage.isNull()) {
            qWarning() << "QImage创建失败";
            return;
        }

        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        if (!buffer.open(QIODevice::WriteOnly)) {
            qWarning() << "无法打开QBuffer";
            return;
        }
        if (!qImage.save(&buffer, "PNG")) {
            qWarning() << "无法将图像保存到QBuffer";
            return;
        }

        QJsonObject json;
        json["image"] = QString::fromLatin1(byteArray.toBase64().data());
        QJsonDocument doc(json);
        QByteArray message = doc.toJson(QJsonDocument::Compact);

        for (QWebSocket *client : qAsConst(m_clients)) {
            client->sendTextMessage(message);
        }
    }

    void sendText() {
        QFile file("fall.txt");
        // 读取文件内容并发送到前端
        if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString text = in.readAll();
            file.close();
            QJsonObject json;
            json["text"] = text;
            QJsonDocument doc(json);
            QByteArray message = doc.toJson(QJsonDocument::Compact);

            for (QWebSocket *client : qAsConst(m_clients)) {
                client->sendTextMessage(message);
            }
        }
    }
private slots:
    void onNewConnection() {
        QWebSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QWebSocket::disconnected, this, &WebSocketServer::socketDisconnected);
        m_clients << socket;
    }

    void socketDisconnected() {
        QWebSocket *client = qobject_cast<QWebSocket *>(sender());
        if (client) {
            m_clients.removeAll(client);
            client->deleteLater();
        }
    }

private:
    QWebSocketServer *m_server;
    QList<QWebSocket *> m_clients;
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    WebSocketServer server(114514);

    std::string inputType = argv[1];

    if (inputType == "camera") {
        cv::VideoCapture cap(0); // 打开默认摄像头
        if (!cap.isOpened()) {
            std::cerr << "Could not open the camera." << std::endl;
            return -1;
        }

#ifdef Tracker
        BYTETracker tracker(fps, 50);  // 30
#endif

        cv::Mat frame;
        while (cap.read(frame)) {
            std::vector<Object> objects;
            yolov8Pose->detect_yolov8(frame, objects);

#ifdef Tracker
            std::vector<STrack> output_stracks = tracker.update(objects);
#endif

            cv::Mat result;

#ifdef Tracker
            yolov8Pose->detect_objects_tracker(frame, result, objects, output_stracks, SKELETON, KPS_COLORS, LIMB_COLORS);
#else
            yolov8Pose->detect_objects(frame, result, objects, SKELETON, KPS_COLORS, LIMB_COLORS);
#endif
            yolov8Pose->draw_fps(result);

            for(const auto& obj:objects) {
                if(obj.is_fall) {
                    // 用QFile记录文字
                    QFile file("fall.txt");
                    if(file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream out(&file);
                        out << "检测到有人摔倒! 在 (" << obj.rect.x << ", " << obj.rect.y << ")";
                        file.close();
                    }
                }
            }

            server.sendImage(result);
            server.sendText();

            if (cv::waitKey(30) == 'q') {
                break;
            }
        }

        cap.release();
        cv::destroyAllWindows();

    }
    return app.exec();
}

#include "main.moc"