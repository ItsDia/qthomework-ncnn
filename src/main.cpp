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

    // void sendImage(const cv::Mat &image) {
    //     // if (image.empty()) {
    //     //     qWarning() << "图像数据为空，无法发送";
    //     //     return;
    //     // }
    //     //
    //     // // 输出图像的基本信息
    //     // qDebug() << "图像大小:" << image.cols << "x" << image.rows;
    //     // qDebug() << "通道数:" << image.channels();
    //     // qDebug() << "数据类型:" << image.type();
    //
    //     cv::Mat rgbImage;
    //     cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB); // 转换为RGB格式
    //     QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
    //     // if (qImage.isNull()) {
    //     //     qWarning() << "QImage创建失败";
    //     //     return;
    //     // }
    //
    //     // 使用QImage::copy()创建一个独立的QImage对象
    //     QImage copiedImage = qImage.copy();
    //     if (copiedImage.isNull()) {
    //         qWarning() << "QImage复制失败";
    //         return;
    //     }
    //
    //     // 尝试将QImage保存到文件以进行调试
    //     if (!copiedImage.save("output_qimage.png")) { // 使用PNG格式
    //         qWarning() << "无法将QImage保存到文件";
    //     }
    //
    //     QByteArray byteArray;
    //     QBuffer buffer(&byteArray);
    //     if (!buffer.open(QIODevice::WriteOnly)) {
    //         qWarning() << "无法打开QBuffer";
    //         return;
    //     }
    //     if (!copiedImage.save(&buffer, "PNG")) { // 使用PNG格式
    //         qWarning() << "无法将图像保存到QBuffer";
    //         return;
    //     }
    //
    //     QJsonObject json;
    //     json["image"] = QString::fromLatin1(byteArray.toBase64().data());
    //     QJsonDocument doc(json);
    //     QByteArray message = doc.toJson(QJsonDocument::Compact);
    //
    //     // qDebug() << "发送的Base64图像数据:" << json["image"].toString().left(100) << "..."; // 仅输出前100个字符
    //
    //     for (QWebSocket *client : qAsConst(m_clients)) {
    //         client->sendTextMessage(message);
    //     }
    // }
    void sendImage(const cv::Mat &image) {
        if (image.empty()) {
            qWarning() << "图像数据为空，无法发送";
            return;
        }

        // 将图像从BGR转换为RGB格式
        cv::Mat rgbImage;
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);

        // 创建QImage对象
        QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
        if (qImage.isNull()) {
            qWarning() << "QImage创建失败";
            return;
        }

        // 将QImage保存到QBuffer中
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        if (!buffer.open(QIODevice::WriteOnly)) {
            qWarning() << "无法打开QBuffer";
            return;
        }
        if (!qImage.save(&buffer, "PNG")) { // 使用PNG格式
            qWarning() << "无法将图像保存到QBuffer";
            return;
        }

        // 创建JSON对象并发送
        QJsonObject json;
        json["image"] = QString::fromLatin1(byteArray.toBase64().data());
        QJsonDocument doc(json);
        QByteArray message = doc.toJson(QJsonDocument::Compact);

        for (QWebSocket *client : qAsConst(m_clients)) {
            client->sendTextMessage(message);
        }
    }
private slots:
    void onNewConnection() {
        QWebSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QWebSocket::textMessageReceived, this, &WebSocketServer::processTextMessage);
        connect(socket, &QWebSocket::disconnected, this, &WebSocketServer::socketDisconnected);
        m_clients << socket;
    }

    void processTextMessage(const QString &message) {
        Q_UNUSED(message);
        // 处理来自客户端的消息
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

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <image|video|camera>" << std::endl;
        return -1;
    }

    WebSocketServer server(114514);

    std::string inputType = argv[1];

    if (inputType == "image") {
        // 处理图像的代码保持不变
    } else if (inputType == "video") {
        // 处理视频文件的代码保持不变
    } else if (inputType == "camera") {
        cv::VideoCapture cap(0); // 打开默认摄像头
        if (!cap.isOpened()) {
            std::cerr << "Could not open the camera." << std::endl;
            return -1;
        }

        int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        int fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));

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

            // cv::imshow("Detection Result", result);

            // 发送结果到前端
            server.sendImage(result);

            if (cv::waitKey(30) == 'q') {
                break;
            }
        }

        cap.release();
        cv::destroyAllWindows();

    } else {
        std::cerr << "Invalid input type. Please use 'image', 'video', or 'camera'." << std::endl;
        return -1;
    }

    return app.exec();
}

#include "main.moc"