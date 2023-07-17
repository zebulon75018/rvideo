#include "mainwindow.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QDebug>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        qDebug() << "Please specify a video file" ;
        return 1;
    }

    // Initialize Qt application
    QApplication app(argc, argv);

    MainWindow *main = new MainWindow(argv[1]);
    main->show();
/*
    // Create QGraphicsView and QGraphicsScene
    QGraphicsView view;
    QGraphicsScene scene;
    view.setScene(&scene);

    // Create QGraphicsPixmapItem to display video frames
    QGraphicsPixmapItem pixmap_item;
    scene.addItem(&pixmap_item);

    QGraphicsLineItem line(0,0,100,100) ;
    scene.addItem(&line);


    // Open video file using OpenCV
    cv::VideoCapture capture(argv[1]);
    if (!capture.isOpened()) {
        std::cout << "Could not open video file " << argv[1] << std::endl;
        return 1;
    }

    // Start timer to update QGraphicsPixmapItem with video frames
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        cv::Mat frame;
        bool ret = capture.read(frame);
        if (ret) {
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            QImage qimage(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
            QPixmap pixmap = QPixmap::fromImage(qimage);
            pixmap_item.setPixmap(pixmap.scaled(view.size(), Qt::KeepAspectRatio));
        }
    });
    timer.start(30);

    // Show QGraphicsView and start Qt event loop
    view.show();

    */
    return app.exec();
}


