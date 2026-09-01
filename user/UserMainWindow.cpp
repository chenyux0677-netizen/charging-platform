#include "UserMainWindow.h"

#include "user/pages/ChargingPage.h"
#include "user/pages/OrderPage.h"
#include "user/pages/ProfilePage.h"
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

    // 中部页面栈:四个标签页
    m_stationListPage = new StationListPage(this);
    m_chargingPage = new ChargingPage(this);
    m_orderPage = new OrderPage(this);
    m_profilePage = new ProfilePage(this);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("pageStack"));
    m_stack->addWidget(m_stationListPage);   // 0 电站
    m_stack->addWidget(m_chargingPage);      // 1 充电
    m_stack->addWidget(m_orderPage);         // 2 订单
    m_stack->addWidget(m_profilePage);       // 3 我的

    // 顶部标题栏
    auto *titleBar = new QLabel(QStringLiteral("充电桩用户端"), this);
    titleBar->setObjectName(QStringLiteral("titleLabel"));
    titleBar->setAlignment(Qt::AlignCenter);

    // 底部导航
    const QVector<QPushButton *> navButtons = {
        makeNavButton(QStringLiteral("电站"), QStringLiteral("navStationBtn")),
        makeNavButton(QStringLiteral("充电"), QStringLiteral("navChargeBtn")),
        makeNavButton(QStringLiteral("订单"), QStringLiteral("navOrderBtn")),
        makeNavButton(QStringLiteral("我的"), QStringLiteral("navProfileBtn")),
    };

    auto *navBar = new QHBoxLayout;
    for (QPushButton *b : navButtons)
        navBar->addWidget(b);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(titleBar);
    layout->addWidget(m_stack, 1);
    layout->addLayout(navBar);

    // 点击导航 → 切页面,并把选中态移动到对应按钮
    for (int i = 0; i < navButtons.size(); ++i) {
        QPushButton *btn = navButtons[i];
        connect(btn, &QPushButton::clicked, this, [this, i, btn, navButtons] {
            m_stack->setCurrentIndex(i);
            for (QPushButton *b : navButtons)
                b->setChecked(b == btn);
        });
    }
    navButtons.first()->setChecked(true); // 默认停在"电站"
}
