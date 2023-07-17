#ifndef QGRAPHICSVIDEOITEM_H
#define QGRAPHICSVIDEOITEM_H

#include <QGraphicsPixmapItem>
#include <QTimer>
#include <opencv2/opencv.hpp>

class QGraphicsVideoItem;

class QSyncObject : public QObject
{
    Q_OBJECT
public:
     explicit QSyncObject(QGraphicsVideoItem *item,QObject *parent = nullptr);
    void setSize(QSize s);

    QGraphicsVideoItem *item;
    QSize size;




signals:
  void recievedFrame();

 public slots:
  void frameRecieved(QPixmap &pixmap);


};

class QGraphicsVideoItem : public QGraphicsPixmapItem
{    
public:
    explicit QGraphicsVideoItem(QString filename,int x,QGraphicsItem *parent = nullptr);

    void setSize(QSize s);
    int getNbFrames();
    QSyncObject *getSyncObj();


    void stop();
    void play();

    void setFrame(int n);
    void drawFrame();

    void paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *widget);


    ////
    /// \brief member.
    ///
    QString filename;
    cv::VideoCapture *capture;
    QTimer timer;
    int offset;
    QSyncObject *syncobj;
    QImage paintarea;
    QSize size4region;
    int interval;
    bool isvideofinished;
    unsigned int nbframes;

    bool compareVertical;

private:
    void init();
/*
public slots:
    void nextFrame();
*/




};

#endif // QGRAPHICSVIDEOITEM_H
