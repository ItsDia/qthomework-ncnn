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
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

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
    //     // cv::Mat rgbImage;
    //     // cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    //     //
    //     // QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
    //     // if (qImage.isNull()) {
    //     //     qWarning() << "QImage创建失败";
    //     //     return;
    //     // }
    //     //
    //     // QByteArray byteArray;
    //     // QBuffer buffer(&byteArray);
    //     // if (!buffer.open(QIODevice::WriteOnly)) {
    //     //     qWarning() << "无法打开QBuffer";
    //     //     return;
    //     // }
    //     // if (!qImage.save(&buffer, "PNG")) {
    //     //     qWarning() << "无法将图像保存到QBuffer";
    //     //     return;
    //     // }
    //     //
    //     // QJsonObject json;
    //     // json["image"] = QString::fromLatin1(byteArray.toBase64().data());
    //     // QJsonDocument doc(json);
    //     // QByteArray message = doc.toJson(QJsonDocument::Compact);
    //     //
    //     // for (QWebSocket *client : qAsConst(m_clients)) {
    //     //     client->sendTextMessage(message);
    //     // }
    //     cv::Mat rgbImage;
    //     cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    //
    //     QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
    //
    //     if (qImage.isNull()) {
    //         qWarning() << "Failed to create QImage from OpenCV Mat.";
    //         return;
    //     }
    //
    //     // 优化：发送二进制图像数据
    //     QByteArray byteArray;
    //     QBuffer buffer(&byteArray);
    //     if (buffer.open(QIODevice::WriteOnly) && qImage.save(&buffer, "PNG")) {
    //         for (QWebSocket *client : qAsConst(m_clients)) {
    //             client->sendBinaryMessage(byteArray);
    //         }
    //     } else {
    //         qWarning() << "Failed to save QImage to buffer.";
    //     }
    // }
    void sendImage(const cv::Mat &image) {
        // 如果图像数据是 BGR 格式，可以直接转换为 RGB，避免不必要的转换
        cv::Mat rgbImage;
        if (image.channels() == 3 && image.type() == CV_8UC3) {
            cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);  // 将BGR转为RGB
        } else {
            rgbImage = image;  // 如果已经是RGB格式则直接使用
        }

        QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);

        if (qImage.isNull()) {
            qWarning() << "Failed to create QImage from OpenCV Mat.";
            return;
        }

        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        if (buffer.open(QIODevice::WriteOnly)) {
            if (!qImage.save(&buffer, "JPEG")) {
                qWarning() << "Failed to save QImage to buffer.";
                return;
            }
            for (QWebSocket *client : qAsConst(m_clients)) {
                client->sendBinaryMessage(byteArray);  // 发送JPEG二进制数据
            }
        } else {
            qWarning() << "Failed to open QBuffer.";
        }
    }

    void sendText() {
        QSqlQuery query;
        query.exec("select * from FallDetection");
        while (query.next()) {
            QString messageText = query.value(1).toString();
            QJsonObject json;
            json["message"] = messageText;
            QJsonDocument doc(json);
            QByteArray messageData = doc.toJson(QJsonDocument::Compact);

            for (QWebSocket *client : qAsConst(m_clients)) {
                client->sendTextMessage(messageData);
            }
        }
    }

private slots:
    void onNewConnection() {
        QWebSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QWebSocket::disconnected, this, &WebSocketServer::socketDisconnected);
        connect(socket, &QWebSocket::textMessageReceived, this, &WebSocketServer::onTextMessageReceived);
        m_clients << socket;
    }

    void socketDisconnected() {
        QWebSocket *client = qobject_cast<QWebSocket *>(sender());
        if (client) {
            m_clients.removeAll(client);
            client->deleteLater();
        }
    }

    //接受前端发送的请求,前端发送的是一个JSON
    void onTextMessageReceived(QString message) {
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject json = doc.object();

    QString type = json["type"].toString();

    if (type == "login") {
        QString username = json["user"].toString();
        QString password = json["password"].toString();

        QSqlQuery query;
        query.prepare("select * from User where NAME = :name and PASSWORD = :password");
        query.bindValue(":name", username);
        query.bindValue(":password", password);

        if (query.exec() && query.next()) {
            QJsonObject response;
            response["status"] = "success";
            QJsonDocument responseDoc(response);
            client->sendTextMessage(responseDoc.toJson(QJsonDocument::Compact));
        } else {
            QJsonObject response;
            response["status"] = "failure";
            QJsonDocument responseDoc(response);
            client->sendTextMessage(responseDoc.toJson(QJsonDocument::Compact));
        }
    } else if (type == "update") {
        int id = json["id"].toInt();
        int tem = json["temperature"].toInt();
        int woa = json["water"].toInt();
        int lig = json["light"].toInt();
        int noi = json["noise"].toInt();

        QSqlQuery query;
        query.exec(QString("select * from ComfortableForOlds where ID = %1").arg(id));
        if (query.next()) {
            query.exec(QString("update ComfortableForOlds set Temperature = %1, Water = %2, Light = %3, Noise = %4 where ID = %5")
                        .arg(tem)
                        .arg(woa)
                        .arg(lig)
                        .arg(noi)
                        .arg(id));
            qDebug() << query.lastError();
            QJsonObject json;
            json["status"] = "success";
            json["ID"] = id;
            json["Temperature"] = tem;
            json["Water"] = woa;
            json["Light"] = lig;
            json["Noise"] = noi;
            QJsonDocument doc(json);
            QByteArray messageData = doc.toJson(QJsonDocument::Compact);
            client->sendTextMessage(messageData);
            cout << "UPDATE DATA TO ComfortableForOlds" << messageData << endl;

        } else {
            int temperature = QRandomGenerator::global()->bounded(10, 30);
            int water = QRandomGenerator::global()->bounded(0, 100);
            int light = QRandomGenerator::global()->bounded(0, 100);
            int noise = QRandomGenerator::global()->bounded(0, 100);
            query.exec(QString("INSERT INTO ComfortableForOlds (Temperature, Water, Light, Noise) VALUES (%1, %2, %3, %4)")
                        .arg(temperature)
                        .arg(water)
                        .arg(light)
                        .arg(noise));
            QJsonObject json;
            json["status"] = "success";
            json["ID"] = query.lastInsertId().toInt();
            json["Temperature"] = temperature;
            json["Water"] = water;
            json["Light"] = light;
            json["Noise"] = noise;
            QJsonDocument doc(json);
            QByteArray messageData = doc.toJson(QJsonDocument::Compact);
            client->sendTextMessage(messageData);
            cout << "INSERT DATA TO ComfortableForOlds" << messageData << endl ;
        }
    }
    else if (type=="query") {
        //QJsonObject({"id":"2","type":"query"})
        QString idStr = json["id"].toString();
        int id = idStr.toInt();
        qDebug() << json << "id:" << id;
        QSqlQuery query;
        query.exec(QString("select * from ComfortableForOlds where ID = %1").arg(id));
        if (query.next()) {
            QJsonObject json;
            json["status"] = "success";
            json["ID"] = id;
            json["Temperature"] = query.value(1).toInt();
            json["Water"] = query.value(2).toInt();
            json["Light"] = query.value(3).toInt();
            json["Noise"] = query.value(4).toInt();
            QJsonDocument doc(json);
            QByteArray messageData = doc.toJson(QJsonDocument::Compact);
            client->sendTextMessage(messageData);
            cout << "SUCCEED QUERY DATA FROM ComfortableForOlds" << messageData << endl;
        } else {
            QJsonObject json;
            json["status"] = "failure";
            QJsonDocument doc(json);
            QByteArray messageData = doc.toJson(QJsonDocument::Compact);
            client->sendTextMessage(messageData);
            cout << "FAILED QUERY DATA FROM ComfortableForOlds" << messageData;
        }
    }
}
private:
    QWebSocketServer *m_server;
    QList<QWebSocket *> m_clients;
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::addLibraryPath("C:/Qt/6.7.3/msvc2019_64/plugins");

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("C:/Users/i4A/CLionProjects/untitled4/databases/database.db");
    if (!db.open()) {
        qDebug() << "FAILED to connect database.";
    } else {
        qDebug() << "succeed to connect database.";
    }

    QSqlQuery query(db);
    query.exec("select count(*) from sqlite_master where type='table' and name='ComfortableForOlds'");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("create table ComfortableForOlds (ID integer primary key autoincrement, Temperature int, Water int, Light int, Noise int)");
    }
    query.exec("select count(*) from sqlite_master where type='table' and name='FallDetection'");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("create table FallDetection (ID integer primary key autoincrement, message text)");
    }
    query.exec("select count(*) from sqlite_master where type='table' and name='User'");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("create table User (ID integer primary key autoincrement, NAME varchar(20), PASSWORD varchar(20))");
    }

    WebSocketServer server(114514);

    std::string inputType = argv[1];

    if (inputType == "camera") {
        cv::VideoCapture cap(0);
        //DEFAULT CAMERA
        if (!cap.isOpened()) {
            std::cerr << "COULD NOT OPEN THE CAMERA" << std::endl;
            return -1;
        }

#ifdef Tracker
        BYTETracker tracker(fps, 50);
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

            for (const auto& obj : objects) {
                if (obj.is_fall) {
                    QString message = QString("检测到有人摔倒，地点在 %1, %2")
                                        .arg(obj.rect.x)
                                        .arg(obj.rect.y);

                    QString sql = QString("INSERT INTO FallDetection (message) VALUES ('%1')")
                                    .arg(message);

                    if (!query.exec(sql)) {
                        qDebug() << "FAIL TO INSERT DATA TO FalldownDetection";
                    }
                }
            }

            cv::imshow("result", result);
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