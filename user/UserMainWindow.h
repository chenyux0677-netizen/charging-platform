#ifndef USERMAINWINDOW_H
#define USERMAINWINDOW_H

#include <QWidget>

class QStackedWidget;
class StationListPage;
class ChargingPage;
class OrderPage;
class ProfilePage;

// 用户端主界面:手机风格(固定窄屏)。
// 结构:顶部标题栏 + 中部页面栈 + 底部四个导航按钮(电站 / 充电 / 订单 / 我的)。
class UserMainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit UserMainWindow(QWidget *parent = nullptr);

private:
    QStackedWidget *m_stack = nullptr;
    StationListPage *m_stationListPage = nullptr;
    ChargingPage *m_chargingPage = nullptr;
    OrderPage *m_orderPage = nullptr;
    ProfilePage *m_profilePage = nullptr;
};

#endif // USERMAINWINDOW_H
