#include "UserMainWindow.h"

#include "user/pages/ChargingPage.h"
#include "user/pages/OrderPage.h"
#include "user/pages/ProfilePage.h"
#include "user/pages/NavigationPage.h"
#include "user/pages/StationDetailPage.h"
#include "user/pages/StationListPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVector>

namespace {
// 底部导航按钮的统一样式开关
QPushButton *makeNavButton(const QString &text, const QString &objName)
{
    auto *btn = new QPushButton(text);
    btn->setObjectName(objName);
    btn->setCheckable(true);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}
} // namespace

UserMainWindow::UserMainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("充电桩 · 用户端"));
    setFixedSize(360, 640);

    m_stationListPage = new StationListPage(this);
    m_stationDetailPage = new StationDetailPage(this);
    m_chargingPage = new ChargingPage(this);
    m_orderPage = new OrderPage(this);
    m_profilePage = new ProfilePage(this);
    m_navigationPage = new NavigationPage(this);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("pageStack"));
    m_stack->addWidget(m_stationListPage);   // 0 电站列表
    m_stack->addWidget(m_stationDetailPage); // 1 电站详情(临时页)
    m_stack->addWidget(m_chargingPage);      // 2 充电
    m_stack->addWidget(m_orderPage);         // 3 订单
    m_stack->addWidget(m_profilePage);       // 4 我的
    m_stack->addWidget(m_navigationPage);    // 5 导航(临时页)

    // 顶部标题栏
    auto *titleBar = new QLabel(QStringLiteral("充电桩用户端"), this);
    titleBar->setObjectName(QStringLiteral("titleLabel"));
    titleBar->setAlignment(Qt::AlignCenter);

    // 底部导航:按钮 → 页面栈索引
    struct NavItem { QPushButton *btn; int index; };
    const QVector<NavItem> navItems = {
        { makeNavButton(QStringLiteral("电站"), QStringLiteral("navStationBtn")), 0 },
        { makeNavButton(QStringLiteral("充电"), QStringLiteral("navChargeBtn")), 2 },
        { makeNavButton(QStringLiteral("订单"), QStringLiteral("navOrderBtn")), 3 },
        { makeNavButton(QStringLiteral("我的"), QStringLiteral("navProfileBtn")), 4 },
    };
    for (const NavItem &n : navItems)
        m_navButtons << n.btn;

    auto *navBar = new QHBoxLayout;
    for (QPushButton *b : m_navButtons)
        navBar->addWidget(b);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(titleBar);
    layout->addWidget(m_stack, 1);
    layout->addLayout(navBar);

    for (const NavItem &n : navItems) {
        connect(n.btn, &QPushButton::clicked, this, [this, n] { switchTo(n.index); });
    }

    // 页面间流转
    connect(m_stationListPage, &StationListPage::stationClicked,
            this, [this](const DataRow &st) {
        m_stationDetailPage->setStation(st);
        switchTo(1);
    });
    connect(m_stationDetailPage, &StationDetailPage::backRequested,
            this, [this] { switchTo(0); });
    connect(m_stationDetailPage, &StationDetailPage::pileChosen,
            this, [this](const DataRow &pile) {
        m_chargingPage->setPile(pile);
        switchTo(2);
    });
    connect(m_stationDetailPage, &StationDetailPage::navigationRequested,
            this, [this](const DataRow &station) {
        m_navigationPage->showLocations(
            m_stationListPage->currentLocationName(),
            m_stationListPage->currentLatitude(),
            m_stationListPage->currentLongitude(), station);
        switchTo(5);
    });
    connect(m_navigationPage, &NavigationPage::backRequested,
            this, [this] { switchTo(1); });
    connect(m_chargingPage, &ChargingPage::goToOrders,
            this, [this] { switchTo(3); });
    connect(m_profilePage, &ProfilePage::logoutRequested,
            this, &UserMainWindow::logoutRequested);

    switchTo(0); // 默认停在"电站"
}

void UserMainWindow::switchTo(int index)
{
    m_stack->setCurrentIndex(index);

    // 先同步底部导航。页面进入钩子可能再次触发 switchTo（例如存在未结算
    // 订单时从充电页跳到订单页），后发生的跳转应保留最终选中态。
    if (m_navButtons.size() == 4) {
        m_navButtons[0]->setChecked(index == 0 || index == 1 || index == 5);
        m_navButtons[1]->setChecked(index == 2);
        m_navButtons[2]->setChecked(index == 3);
        m_navButtons[3]->setChecked(index == 4);
    }

    // 页面切换钩子
    switch (index) {
    case 0: m_stationListPage->refresh(); break;
    case 2: m_chargingPage->onPageEntered(); break;
    case 3: m_orderPage->refresh(); break;
    case 4: m_profilePage->refresh(); break;
    default: break;
    }
}
