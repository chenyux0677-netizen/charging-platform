#ifndef ADMINMAINWINDOW_H
#define ADMINMAINWINDOW_H

#include <QWidget>

class QTabWidget;
class SalesPage;
class PileStatusPage;
class PileManagePage;
class StationManagePage;
class UserManagePage;

// 管理员端主界面:桌面风格宽屏,QTabWidget 放五大管理页签。
class AdminMainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AdminMainWindow(QWidget *parent = nullptr);

private:
    QTabWidget *m_tabs = nullptr;
};

#endif // ADMINMAINWINDOW_H
