#include "mainwindow.h"
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include "qgraphicsvideoitem.h"
#include "movablerectitem.h"
#include "annotationwidget.h"
#include <QString>
#include <QVBoxLayout>
#include <QToolBar>
#include <QSlider>
#include <QPushButton>
#include <QIcon>
#include <QtGui>
#include <QDebug>
#include <QTemporaryFile>



MainWindow::MainWindow(QString filename,QWidget *parent)
    : QMainWindow(parent)
{
    initToolbox();
    _scale = 1.0;

    QFrame *f = new QFrame(this);
    QVBoxLayout *l = new QVBoxLayout();
    f->setLayout(l);
    this->setCentralWidget(f);

    scene = new QGraphicsScene(0,0, 800,800);
    view = new QGraphicsView (scene);
    view->show();

    slidertime = new QSlider(f);
    slidertime->setOrientation(Qt::Orientation::Horizontal);
    l->addWidget(view);
    l->addWidget(slidertime);


    cursor = new MovableRectItem(0.0,0.0,10.0,200.0);
    cursorhorizontal = new MovableRectItemHorizontal(0.0,0.0,200.0,10.0);

    //cursorhorizontal->setVisible(false);
    setUpMovableRectItem(cursor,QStyle::SP_ArrowDown,200,0);
    setUpMovableRectItem(cursorhorizontal,QStyle::SP_ArrowRight,0,200);

    //setRect(0,0,10,800);
    pixmap_item=new QGraphicsVideoItem("bigbuckbunny.mp4",0);
    pixmap_item2=new QGraphicsVideoItem("invbigbunny.mp4",200);

    QSize s = view->size();
    pixmap_item->setSize(s);
    pixmap_item2->setSize(s);
   // pixmap_item2->setPos(QPointF(s.width(),0));

    init();

    slidertime->setMinimum(0);
    slidertime->setMaximum(pixmap_item->getNbFrames());
    qDebug() << "nbFrame " << pixmap_item->getNbFrames();
}

void MainWindow::setUpMovableRectItem(MovableRectItem *r,QStyle::StandardPixmap s,int x,int y)
{
    QIcon icon = QApplication::style()->standardIcon(s);
    QPixmap p = icon.pixmap(icon.availableSizes().last());
    r->setPixmap(p);
    if ( x ==0 )
    {
        r->setPos(x,y-p.height()/2); // 200 trouver le millieux ....
    }
    else
    {
        r->setPos(x-p.width()/2,y); // 200 trouver le millieux ....
    }

}
void MainWindow::initToolbox()
{
    QToolBar* toolBar = new QToolBar(this);
    addToolBar(toolBar);

    QIcon qZoomIn = QIcon::fromTheme("zoom-in");
    QIcon qZoomOut = QIcon::fromTheme("zoom-out");
    QIcon qZoomOriginal = QIcon::fromTheme("zoom-original");
     // Create the actions
     QAction* playAction = createAction2(QStyle::StandardPixmap::SP_MediaPlay, "Play", "Play the media");
     QAction* rewindAction = createAction2(QStyle::StandardPixmap::SP_MediaPause , "Pause", "Pause the media");
     QAction* nextAction = createAction2(QStyle::StandardPixmap::SP_MediaSeekForward, "Next", "Play the next media");
     QAction* previousAction = createAction2(QStyle::StandardPixmap::SP_MediaSeekBackward, "Previous", "Play the previous media");
     QAction* changeorientation = createAction2(QStyle::StandardPixmap::SP_BrowserReload, "inverse", "inverse orientation");

     QAction* zoomInAction = createAction(qZoomIn, "Zoom+", "Zoom in ");
     QAction* zoomOutAction = createAction(qZoomOut, "Zoom-", "Zoom out");
     QAction* unZoom = createAction(qZoomOriginal, "Normal", "Reset Zoom");

     QAction* annotation = createAction(qZoomOriginal, "Annotation", "Annotation");

     // Add actions to the toolbar
     toolBar->addAction(playAction);
     toolBar->addAction(rewindAction);
     toolBar->addAction(nextAction);
     toolBar->addAction(previousAction);    
     toolBar->addAction(changeorientation);

     toolBar->addAction(zoomInAction);
     toolBar->addAction(zoomOutAction);
     toolBar->addAction(unZoom);

     toolBar->addAction(annotation);

    // Connect button signals to their respective slots
    connect(playAction, SIGNAL(triggered()), this, SLOT(playClicked()));
    connect(rewindAction, SIGNAL(triggered()), this, SLOT(rewindClicked()));
    connect(nextAction, SIGNAL(triggered()), this, SLOT(nextClicked()));
    connect(previousAction, SIGNAL(triggered()), this, SLOT(previousClicked()));
    connect(changeorientation, SIGNAL(triggered()), this, SLOT(changeOrientationClicked()));

    connect(zoomInAction, SIGNAL(triggered()), this, SLOT(zoomIn()));
    connect(zoomOutAction, SIGNAL(triggered()), this, SLOT(zoomOut()));    
    connect(unZoom, SIGNAL(triggered()), this, SLOT(resetZoom()));
    connect(annotation, SIGNAL(triggered()), this, SLOT(makeAnnotation()));
   }


void MainWindow::init()
{
    // Create QGraphicsPixmapItem to display video frames

    scene->addItem(pixmap_item);
    scene->addItem(pixmap_item2);
    scene->addItem(cursor);
    scene->addItem(cursorhorizontal);

    // Attention : hack cursor and cursorhorizontal same slot...
    connect(cursor->transmetter, SIGNAL(moveCursor(int )),this, SLOT(cursorMoved(int)));
    connect(cursorhorizontal->transmetter, SIGNAL(moveCursor(int )),this, SLOT(cursorMoved(int)));
    //connect(this, SIGNAL(recievedFrame(QPixmap &)), SLOT(frameRecieved(QPixmap &)));
    //connect(this,this->recievedFrame,this, this->frameRecieved)
    connect(pixmap_item->getSyncObj(),SIGNAL(recievedFrame()),this, SLOT(frameRecieved()));
    connect(this->slidertime ,SIGNAL(sliderReleased()),this,SLOT(sliderHasMoved()));
    //QGraphicsLineItem line(0,0,100,100) ;
    //scene->addItem(&line);
}

   // Button click slots
   void MainWindow::playClicked() {
       qDebug() << "Play button clicked";
       pixmap_item->play();
       pixmap_item2->play();
   }

   void MainWindow::rewindClicked() {
       qDebug() << "Rewind button clicked";
   }

   void MainWindow::nextClicked() {
       qDebug() << "Next button clicked";
   }

   void MainWindow::previousClicked() {
       qDebug() << "Previous button clicked";
   }

   void MainWindow::frameRecieved() {
       int frame = (int)pixmap_item->capture->get(cv::CAP_PROP_POS_FRAMES);
       this->slidertime->setValue(frame);
   }

   void MainWindow::sliderHasMoved()
   {
       int n = this->slidertime->value();
       qDebug() << "sliderHasMoved " << n;
       pixmap_item->setFrame(n);
       pixmap_item2->setFrame(n);
       scene->update(view->rect());
   }


   void MainWindow::cursorMoved(int x)
   {
       qDebug() << x;
       //pixmap_item->x= x ;
       pixmap_item2->offset = x;
       scene->update(view->rect());             
   }

    void MainWindow::updateScale()
    {
        qDebug() << " scale " << _scale;
         view->scale(_scale, _scale);
         scene->update(view->rect());
    }
    void MainWindow::zoomIn()
    {
        _scale = 1.1;
        updateScale();

    }
     void MainWindow::zoomOut()
     {
       _scale = 0.9;
       updateScale();
     }
     void MainWindow::resetZoom()
     {
         view->resetTransform();
         scene->update(view->rect());
     }

     void  MainWindow::makeAnnotation()
     {
        QPixmap pixMap = view->grab(view->sceneRect().toRect());        
        pixMap.save("tmp.jpg");
        dialogAnnotation *dialogA = new dialogAnnotation("tmp.jpg");
        dialogA->exec();
     }

   void MainWindow::changeOrientationClicked()
   {
     if (cursor->isVisible())
     {
         cursor->changeVisibility(false);
         cursorhorizontal->changeVisibility(true);
         pixmap_item2->compareVertical = false;
     }
     else
     {
         cursor->changeVisibility(true);
         cursorhorizontal->changeVisibility(false);
         pixmap_item2->compareVertical = true;
     }
     scene->update(view->rect());
   }

/*
void MainWindow::frameRecieved(QPixmap &pixmap)
{
   pixmap_item->setPixmap(pixmap.scaled(view->size(), Qt::KeepAspectRatio));
}
*/
MainWindow::~MainWindow()
{
}


QAction* MainWindow:: createAction(const QIcon& icon, const QString& text, const QString& tooltip) {
    QAction* action = new QAction(icon, text, this);
    action->setToolTip(tooltip);
    return action;
}

QAction* MainWindow:: createAction2(QStyle::StandardPixmap stdname, const QString& text, const QString& tooltip) {
    QStyle *style = this->style();
    QAction* action = new QAction(style->standardIcon(stdname), text, this);

    action->setToolTip(tooltip);
    return action;
}

