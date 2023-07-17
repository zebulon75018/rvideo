#include <QPainter>
#include "movablerectitem.h"

Transmetter::Transmetter(QObject *parent)
    : QObject(parent)
{
}

Transmetter::~Transmetter()
{
}

MovableRectItem::MovableRectItem(qreal x, qreal y, qreal w, qreal h,QGraphicsItem* parent) : QGraphicsRectItem(x,y,w,h,parent) {
        setFlag(QGraphicsItem::ItemIsFocusable);
        setFlag(QGraphicsItem::ItemIsMovable);
        transmetter = new Transmetter();
 }

    void MovableRectItem::keyPressEvent(QKeyEvent* event)  {
        if (event->key() == Qt::Key_Left) {
            moveBy(-10, 0);  // Move left by 10 units
        } else if (event->key() == Qt::Key_Right) {
            moveBy(10, 0);   // Move right by 10 units
        } else {
            QGraphicsRectItem::keyPressEvent(event);
        }
        emit transmetter->moveCursor(this->x()+this->pixmap().width()/2);
    }

    void MovableRectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)  {
        // Store the initial position for calculating the movement distance
        initialPos = event->pos();
        QGraphicsRectItem::mousePressEvent(event);
    }

    void MovableRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)  {
        // Calculate the movement distance
        QPointF newPos = event->pos();
        qreal dx = newPos.x() - initialPos.x();
        //qreal dy = newPos.y() - initialPos.y();

        // Move the item horizontally
        moveBy(dx, 0);

        emit transmetter->moveCursor(this->x()+this->pixmap().width()/2);
        initialPos = newPos;
        QGraphicsRectItem::mouseMoveEvent(event);
        this->setY(0);
    }

    void MovableRectItem::changeVisibility(bool b)
    {
       this->setVisible(b);
        if (b)
        {
            emit transmetter->moveCursor(this->x()+this->pixmap().width()/2);
        }
    }

    QPixmap MovableRectItem::pixmap()
    {
      return _pixmap;
    }

    void MovableRectItem::setPixmap(QPixmap p)
    {
        _pixmap = p;
    }


    void MovableRectItem::paint( QPainter *painter, const QStyleOptionGraphicsItem *o, QWidget *widget)
    {
        painter->drawPixmap(_pixmap.rect(), _pixmap, _pixmap.rect());
//        QGraphicsRectItem::paint(painter,o,widget);
    }



    /*************************************************************************************+
     *
     *
     *
     *
     *************************************************************************************/

    MovableRectItemHorizontal::MovableRectItemHorizontal(qreal x, qreal y, qreal w, qreal h,QGraphicsItem* parent) : MovableRectItem(x,y,w,h,parent)
    {

    }

    void MovableRectItemHorizontal::mouseMoveEvent(QGraphicsSceneMouseEvent* event)  {
        // Calculate the movement distance
        QPointF newPos = event->pos();
        qreal dy = newPos.y() - initialPos.y();

        // Move the item horizontally
        moveBy(0,dy);

        emit transmetter->moveCursor(this->y()+this->pixmap().height()/2);
        initialPos = newPos;
        QGraphicsRectItem::mouseMoveEvent(event);
        this->setX(0);
    }

    void MovableRectItemHorizontal::changeVisibility(bool b)
    {
        this->setVisible(b);
         if (b)
         {
            emit transmetter->moveCursor(this->y()+this->pixmap().height()/2);
         }
    }

