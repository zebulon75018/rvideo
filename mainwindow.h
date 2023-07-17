#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <QStyle>

class QString;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsVideoItem;
class QPushButton;
class MovableRectItem;
class MovableRectItemHorizontal;
class QSlider;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QString filename,QWidget *parent = nullptr);
    ~MainWindow();

    void init();
    void initToolbox();


       // Button click slots
  //  QString filename;
    QGraphicsView *view;
    QGraphicsScene *scene;
    QSlider * slidertime;
    //cv::VideoCapture *capture;
    QGraphicsVideoItem *pixmap_item;
    QGraphicsVideoItem *pixmap_item2;
    MovableRectItem *cursor;
    MovableRectItemHorizontal * cursorhorizontal;
    //QTimer timer;

private:
    QAction* createAction(const QIcon& icon, const QString& text, const QString& tooltip);
    QAction* createAction2(QStyle::StandardPixmap stdname, const QString& text, const QString& tooltip);

    void setUpMovableRectItem(MovableRectItem *r,QStyle::StandardPixmap style,int x,int y);

    void updateScale();

    qreal _scale;
    /*

  signals:
    void recievedFrame(QPixmap &pixmap);

   public slots:
    void frameRecieved(QPixmap &pixmap);
*/
  public slots:
    void playClicked();
    void rewindClicked() ;
    void nextClicked();
    void previousClicked();

    void changeOrientationClicked();

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void makeAnnotation();

    void cursorMoved(int);

    void sliderHasMoved();


    void frameRecieved();
};
#endif // MAINWINDOW_H
