#ifndef USERMAINWINDOW_H
#define USERMAINWINDOW_H

#include <QVector>
#include <QWidget>

class QPushButton;
class QStackedWidget;
class StationListPage;
class StationDetailPage;
class ChargingPage;
class OrderPage;
class ProfilePage;
class NavigationPage;

// 用户端主界面:手机风格(固定窄屏)。
// 结构:顶部标题栏 + 中部页面栈 + 底部四个导航按钮(电站 / 充电 / 订单 / 我的)。
// 页面栈索引:0=电站列表 1=电站详情 2=充电 3=订单 4=我的 5=地图导航
class UserMainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit UserMainWindow(QWidget *parent = nullptr);

signals:
    void logoutRequested();

private:
    void switchTo(int index);

    QStackedWidget *m_stack = nullptr;
    QVector<QPushButton *> m_navButtons; // 电站/充电/订单/我的,与 0/2/3/4 页对应
    StationListPage *m_stationListPage = nullptr;
    StationDetailPage *m_stationDetailPage = nullptr;
    ChargingPage *m_chargingPage = nullptr;
    OrderPage *m_orderPage = nullptr;
    ProfilePage *m_profilePage = nullptr;
    NavigationPage *m_navigationPage = nullptr;
};

#endif // USERMAINWINDOW_H
