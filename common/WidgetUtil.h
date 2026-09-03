#ifndef WIDGETUTIL_H
#define WIDGETUTIL_H

class QListWidget;
class QWidget;

namespace WidgetUtil {

// 将地点候选列表显示为输入框下方的悬浮下拉层，不占用页面布局空间。
void showSuggestionPopup(QListWidget *popup, const QWidget *anchor);

} // namespace WidgetUtil

#endif // WIDGETUTIL_H
