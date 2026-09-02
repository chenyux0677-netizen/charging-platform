#include "AdminLoginWindow.h"

#include "common/AppContext.h"

#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

AdminLoginWindow::AdminLoginWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("管理员端登录"));
    setFixedSize(360, 300);

    auto *title = new QLabel(QStringLiteral("管理员登录"), this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setAlignment(Qt::AlignCenter);

    m_userEdit = new QLineEdit(this);
    m_userEdit->setObjectName(QStringLiteral("userEdit"));
    m_userEdit->setPlaceholderText(QStringLiteral("账号"));

    m_passEdit = new QLineEdit(this);
    m_passEdit->setObjectName(QStringLiteral("passEdit"));
    m_passEdit->setPlaceholderText(QStringLiteral("密码"));
    m_passEdit->setEchoMode(QLineEdit::Password);

    m_loginButton = new QPushButton(QStringLiteral("登录"), this);
    m_loginButton->setObjectName(QStringLiteral("loginButton"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addSpacing(16);
    layout->addWidget(m_userEdit);
    layout->addWidget(m_passEdit);
    layout->addSpacing(16);
    layout->addWidget(m_loginButton);

    connect(m_loginButton, &QPushButton::clicked,
            this, &AdminLoginWindow::onLoginClicked);
}

void AdminLoginWindow::onLoginClicked()
{
    const QString username = m_userEdit->text().trimmed();
    const QString password = m_passEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入账号和密码。"));
        return;
    }

    AppContext *ctx = AppContext::instance();
    if (!ctx->connectIfNeeded()) {
        QMessageBox::warning(this, QStringLiteral("无法连接"),
                             QStringLiteral("连接服务器失败,请检查服务器地址与端口。"));
        return;
    }

    const QueryResult rows = ctx->dataSource()->query(
        QStringLiteral("admins"),
        {},
        QStringLiteral("username = ? AND password = ?"),
        QVariantList{username, password});

    if (rows.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("登录失败"),
                             QStringLiteral("账号或密码错误。"));
        return;
    }

    emit loginSucceeded(username);
}
