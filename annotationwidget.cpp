#include <QImage>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFileInfo>
#include "annotationwidget.h"

annotationWidget::annotationWidget(QString filename, QWidget *parent)
    : QWidget{parent}
{
    _filename = filename;
    init();
}

annotationWidget::annotationWidget(QImage img ,QWidget *parent)
    : QWidget{parent}
{
    _image = img;
    init();
}
void annotationWidget::init()
{
    QVBoxLayout *vBoxLayout = new QVBoxLayout(this);
    this->setLayout(vBoxLayout);
    _view = new QWebView(this);
    vBoxLayout->addWidget(_view);
    _view->page()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    _view->load(QUrl("qrc:/drawingboard/index.html"));
    //load(QUrl("qrc:/index.htm"));
    //_view->setUrl(QUrl::fromLocalFile("/home/easy/STUFF/C++/widgetdraw/drawingboard/drawingboard/index.html"));
    _frame = _view->page()->mainFrame();
     connect(_frame,SIGNAL(loadFinished(bool)),this,SLOT(loadedfinish(bool)));
}
void annotationWidget::changedColor(QColor color)
{
    if (color.isValid()) {
        QVariant currentObject =  _frame->evaluateJavaScript("canvas.getActiveObject();");
        if ( currentObject.isValid())
        {
             _frame->evaluateJavaScript("canvas.getActiveObject().set('stroke','" +  color.name() + "');");
        }
        else
        {
          _frame->evaluateJavaScript("changeColor(\"" +  color.name() + "\");");
        }
       _frame->evaluateJavaScript("canvas.renderAll();");
    }

}
void annotationWidget::changedSize(int size)
{
    QVariant currentObject =  _frame->evaluateJavaScript("canvas.getActiveObject();");
    if ( currentObject.isValid())
    {
        _frame->evaluateJavaScript("canvas.getActiveObject().set('strokeWidth',"+QString::number(size)+");canvas.renderAll();");
    }
    else
    {
        _frame->evaluateJavaScript("changeSize(\"" +  QString::number(size) + "\"); canvas.renderAll();");
    }

}

void annotationWidget::loadedfinish(bool value)
{
   QString urlfile = "";
   qDebug() << " loadedfinish " << value;
   if ( _filename.isNull() == false )
   {
           QFileInfo fi = this->_filename;
           urlfile = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
           this->_image = QImage(this->_filename);
           if ( this->_image.isNull() )
           {
               qDebug() << " Error loading image ";
           }

   }
   _frame->evaluateJavaScript("changeBackground('"+urlfile+"');");
}

QImage annotationWidget::getImage()
{
    QVariant res = _frame->evaluateJavaScript("canvas.toDataURL('png')");
    qDebug() << res;
    QImage img;
    QString uuencodeStr=  res.toString().right(res.toString().length()-QString("data:image/png;base64,").length());
    img.loadFromData(QByteArray::fromBase64( uuencodeStr.toLocal8Bit().data()));
    return img;
}


dialogAnnotation::dialogAnnotation(QString filename ,QWidget* parent)
{
    _filename = filename;
    init();
}
void dialogAnnotation::init()
{
    QVBoxLayout *vLayout = new QVBoxLayout();
    this->setLayout(vLayout);
    annotationWidget *aWid = new annotationWidget(_filename, this);
    vLayout->addWidget(aWid);
}
