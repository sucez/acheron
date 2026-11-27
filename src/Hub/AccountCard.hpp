#pragma once
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFrame>
#include <QMimeData>
#include <QMouseEvent>
#include <functional>

namespace Acheron::Hub {
class AccountCard : public QFrame
{
public:
    AccountCard(QString id, std::function<void(QString,QString)> move)
        : id(std::move(id)), move(std::move(move)) { setAcceptDrops(true); }
protected:
    void mousePressEvent(QMouseEvent *event) override { origin = event->pos(); }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton) || (event->pos()-origin).manhattanLength() < QApplication::startDragDistance())
            return;
        QDrag drag(window());
        auto *data = new QMimeData;
        data->setData(format, id.toUtf8());
        drag.setMimeData(data);
        drag.exec(Qt::MoveAction);
    }
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->source() == window() && event->mimeData()->hasFormat(format))
            event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override
    {
        if (event->source() != window()) return;
        event->acceptProposedAction();
        move(QString::fromUtf8(event->mimeData()->data(format)), id);
    }
private:
    static constexpr auto format = "application/x-acheron-profile";
    QString id;
    QPoint origin;
    std::function<void(QString,QString)> move;
};
}
