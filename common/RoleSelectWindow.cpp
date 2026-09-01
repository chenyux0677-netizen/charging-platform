#include "RoleSelectWindow.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

RoleSelectWindow::RoleSelectWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("充电桩应用管理平台"));
    setFixedSize(420, 320);

    auto *title = new QLabel(QStringLiteral("充电桩应用管理平台"), this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setAlignment(Qt::AlignCenter);

    auto *hint = new QLabel(QStringLiteral("请选择您的身份"), this);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setAlignment(Qt::AlignCenter);

    // 服务器地址:跨设备演示时改成管理员那台机器的 IP
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_hostEdit->setObjectName(QStringLiteral("hostEdit"));
    m_portEdit = new QLineEdit(QStringLiteral("9527"), this);
    m_portEdit->setObjectName(QStringLiteral("portEdit"));
    m_portEdit->setValidator(new QIntValidator(1, 65535, this));

    auto *addrForm = new QFormLayout;
    addrForm->addRow(QStringLiteral("服务器地址"), m_hostEdit);
    addrForm->addRow(QStringLiteral("端口"), m_portEdit);

    m_userButton = new QPushButton(QStringLiteral("用户端"), this);
    m_userButton->setObjectName(QStringLiteral("userButton"));
    m_adminButton = new QPushButton(QStringLiteral("管理员端"), this);
    m_adminButton->setObjectName(QStringLiteral("adminButton"));

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_userButton);
    buttons->addWidget(m_adminButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(hint);
    layout->addSpacing(16);
    layout->addLayout(addrForm);
    layout->addSpacing(16);
    layout->addLayout(buttons);

    connect(m_userButton, &QPushButton::clicked,
            this, &RoleSelectWindow::onUserButtonClicked);
    connect(m_adminButton, &QPushButton::clicked,
            this, &RoleSelectWindow::onAdminButtonClicked);
}

void RoleSelectWindow::onUserButtonClicked()
{
    const QString host = m_hostEdit->text().trimmed();
    const quint16 port = static_cast<quint16>(m_portEdit->text().toUInt());
    emit userModeSelected(host, port);
}

void RoleSelectWindow::onAdminButtonClicked()
{
    emit adminModeSelected();
}
