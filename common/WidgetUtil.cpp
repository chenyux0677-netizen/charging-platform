#include "WidgetUtil.h"

#include <QListWidget>
#include <QPoint>
#include <QWidget>

namespace WidgetUtil {

void showSuggestionPopup(QListWidget *popup, const QWidget *anchor)
{
    if (!popup || !anchor || popup->count() == 0) {
        if (popup)
            popup->hide();
        return;
    }

    int height = popup->frameWidth() * 2;
    const int visibleRows = qMin(popup->count(), 6);
    for (int row = 0; row < visibleRows; ++row)
        height += qMax(36, popup->sizeHintForRow(row));

    QWidget *container = popup->parentWidget();
    if (!container)
        return;
    const QPoint position = anchor->mapTo(container, QPoint(0, anchor->height()));
    popup->setAttribute(Qt::WA_StyledBackground);
    popup->setFocusPolicy(Qt::NoFocus);
    popup->setGeometry(position.x(), position.y(), anchor->width(), height);
    popup->raise();
    popup->show();
}

} // namespace WidgetUtil
