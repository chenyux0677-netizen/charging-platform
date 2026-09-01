#include "AdminMainWindow.h"

#include "admin/pages/PileManagePage.h"
#include "admin/pages/PileStatusPage.h"
#include "admin/pages/SalesPage.h"
#include "admin/pages/StationManagePage.h"
#include "admin/pages/UserManagePage.h"

#include <QTabWidget>
#include <QVBoxLayout>

AdminMainWindow::AdminMainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("充电桩平台 · 管理员端"));
    resize(1200, 800);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("mainTabs"));
    m_tabs->addTab(new SalesPage(m_tabs),        QStringLiteral("销售统计"));
    m_tabs->addTab(new PileStatusPage(m_tabs),   QStringLiteral("充电桩状态"));
    m_tabs->addTab(new PileManagePage(m_tabs),   QStringLiteral("充电桩管理"));
    m_tabs->addTab(new StationManagePage(m_tabs), QStringLiteral("充电站管理"));
    m_tabs->addTab(new UserManagePage(m_tabs),   QStringLiteral("用户管理"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tabs);
}
