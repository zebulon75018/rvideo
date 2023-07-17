#ifndef MOVABLERECTITEM_H
#define MOVABLERECTITEM_H

#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QGraphicsSceneMouseEvent>

class Transmetter : public QObject {
    Q_OBJECT
public:
     explicit Transmetter(QObject *parent = nullptr);
    ~Transmetter();

signals:
  void moveCursor(int pos);
};

class MovableRectItem : public QGraphicsRectItem {
public:
    MovableRectItem(qreal x, qreal y, qreal w, qreal h,QGraphicsItem* parent = nullptr);
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

    void paint( QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget = 0 ) Q_DECL_OVERRIDE;

    void changeVisibility(bool b);

    QPixmap pixmap();
    void setPixmap(QPixmap p);

    Transmetter *transmetter;
protected:
    QPointF initialPos;
    QPixmap  _pixmap;

};

class MovableRectItemHorizontal : public MovableRectItem {
public:
    MovableRectItemHorizontal(qreal x, qreal y, qreal w, qreal h,QGraphicsItem* parent = nullptr);
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void changeVisibility(bool b);

};
#endif
