#include "qgraphicsvideoitem.h"
#include <QSize>
#include <QDebug>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QPainter>

QSyncObject::QSyncObject(QGraphicsVideoItem *item,QObject *parent)
    : QObject(parent)
{
   connect(this, SIGNAL(recievedFrame(QPixmap &)),this,SLOT(frameRecieved(QPixmap &)));
   this->item = item;
   this->size = QSize();
}


void QSyncObject::frameRecieved(QPixmap &pixmap)
{
    item->setPixmap(pixmap.scaled(size /*view->size(), Qt::KeepAspectRatio */));
   //item->setPixmap(pixmap);
    emit recievedFrame();
}

void QSyncObject::setSize(QSize s)
{
    this->size = s;
}


QGraphicsVideoItem::QGraphicsVideoItem(QString filename,int x,QGraphicsItem *parent)
    : QGraphicsPixmapItem{parent}
{
    this->compareVertical = true;
    this->interval = 25;
    this->isvideofinished = false;
   this->filename = filename;
    this->offset = x;
   //QObject::connect(&timer, &QTimer::timeout, this, &QGraphicsVideoItem::nextFrame);
    this->init();
    this->syncobj = new QSyncObject(this);
}

void QGraphicsVideoItem::setSize(QSize s)
{
    this->syncobj->setSize(s);
    this->size4region = s;
}
QSyncObject *QGraphicsVideoItem::getSyncObj()
{
  return this->syncobj;
}
int QGraphicsVideoItem::getNbFrames()
{
    return this->nbframes;
}

void QGraphicsVideoItem::init()
{
    // Open video file using OpenCV
    capture = new cv::VideoCapture(filename.toStdString());

    if (!capture->isOpened()) {
        qDebug() << "Could not open video file " << filename;
        //return 1;
    }

    nbframes = (int) capture->get(cv::CAP_PROP_FRAME_COUNT);
    int tmpw= (int) capture->get(cv::CAP_PROP_FRAME_WIDTH);
    int tmph= (int) capture->get(cv::CAP_PROP_FRAME_HEIGHT);
    int frameRate = (int)capture->get(cv::CAP_PROP_FPS);


    qDebug() << " frames " << nbframes << " tmpw " << tmpw << " tmph " << tmph << " framerate " << frameRate;

    /*
    cv::Mat frame;
    bool retinit = capture->read(frame);
    if ( retinit )
    {
    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
    paintarea  = QImage(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
    }
    */


    // Start timer to update QGraphicsPixmapItem with video frames
    qDebug() << " timer";
    int n= 0;

    QObject::connect(&timer, &QTimer::timeout, [&]() {
        drawFrame();
        /*
        cv::Mat frame;
        bool ret = capture->read(frame);
        if (ret) {
           // qDebug() << " frame ";
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGBA);
            QImage qimage(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGBX8888);

            QPixmap pixmap = QPixmap::fromImage(qimage); //.copy(x,0,qimage.width()-x,qimage.height());
            emit this->syncobj->frameRecieved(pixmap);
            //pixmap_item.setPixmap(pixmap.scaled(view->size(), Qt::KeepAspectRatio));
            //pixmap_item.setPixmap(pixmap);
        }
        else
        {
            this->isvideofinished = true;
            this->timer.stop();

        }
        */
    });
   // timer.start( this->interval );
}

void QGraphicsVideoItem::drawFrame()
{
    cv::Mat frame;
    bool ret = capture->read(frame);
    if (ret) {
       // qDebug() << " frame ";
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGBA);
        QImage qimage(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGBX8888);
        QPixmap pixmap = QPixmap::fromImage(qimage); //.copy(x,0,qimage.width()-x,qimage.height());
        emit this->syncobj->frameRecieved(pixmap);
    }
    else
    {
        this->isvideofinished = true;
        this->timer.stop();

    }
}

void QGraphicsVideoItem::setFrame(int n)
{
   capture->set(cv::CAP_PROP_POS_FRAMES,(double)n);
   drawFrame();
}

/*
void QGraphicsVideoItem::nextFrame()
{
qDebug() << " timer";
cv::Mat frame;
bool ret = capture->read(frame);
if (ret) {
   // qDebug() << " frame ";
    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGBA);
    QImage qimage(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGBX8888);

    QPixmap pixmap = QPixmap::fromImage(qimage); //.copy(x,0,qimage.width()-x,qimage.height());
    emit this->syncobj->recievedFrame(pixmap);
    //pixmap_item.setPixmap(pixmap.scaled(view->size(), Qt::KeepAspectRatio));
    //pixmap_item.setPixmap(pixmap);
}
else
{
    this->isvideofinished = true;
    this->timer.stop();

}
}
*/

void QGraphicsVideoItem::stop()
{

}

void QGraphicsVideoItem::play()
{
    if (timer.isActive())
    {
        timer.stop();
    }
    else
    {
        if (this->isvideofinished )
        {
            capture->release();
            init();
            timer.start( this->interval );
        }
        else
        {
          timer.start( this->interval );
        }
    }
}
void QGraphicsVideoItem::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget)
{
    if (compareVertical)
    {
       painter->setClipRegion(QRegion(offset,0,size4region.width(),size4region.height()));
    }
    else
    {
        painter->setClipRegion(QRegion(0,offset,size4region.width(),size4region.height()));
    }
   //  painter->setClipRegion(QRegion(0,x,size4region.width(),size4region.height()));
    painter->drawPixmap(0,0,this->pixmap());
}
