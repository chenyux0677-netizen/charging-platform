#include "UserLoginWindow.h"

#include "common/AppContext.h"

#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

UserLoginWindow::UserLoginWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("用户端登录"));
    setFixedSize(360, 280);

    auto *title = new QLabel(QStringLiteral("手机号登录"), this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setAlignment(Qt::AlignCenter);

    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setObjectName(QStringLiteral("phoneEdit"));
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入 11 位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{11}$")), this));
    m_phoneEdit->setClearButtonEnabled(true);

    auto *hint = new QLabel(QStringLiteral("未注册的手机号将自动注册"), this);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setAlignment(Qt::AlignCenter);

    m_loginButton = new QPushButton(QStringLiteral("登录"), this);
    m_loginButton->setObjectName(QStringLiteral("loginButton"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addSpacing(16);
    layout->addWidget(m_phoneEdit);
    layout->addSpacing(8);
    layout->addWidget(hint);
    layout->addSpacing(16);
    layout->addWidget(m_loginButton);

    connect(m_loginButton, &QPushButton::clicked,
            this, &UserLoginWindow::onLoginClicked);
}

bool UserLoginWindow::tryConnect()
{
    AppContext *ctx = AppContext::instance();
    if (!ctx->connectIfNeeded()) {
        QMessageBox::warning(this, QStringLiteral("无法连接"),
                             QStringLiteral("连接服务器失败,请检查服务器地址与端口。"));
        return false;
    }
    return true;
}

DataRow UserLoginWindow::findOrCreateUser(const QString &phone)
{
    return AppContext::instance()->dataSource()->loginUser(phone);
}

void UserLoginWindow::onLoginClicked()
{
    const QString phone = m_phoneEdit->text().trimmed();
    if (phone.size() != 11) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入 11 位手机号。"));
        return;
    }
    if (!tryConnect())
        return;

    const DataRow user = findOrCreateUser(phone);
    if (user.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("登录失败"),
                             QStringLiteral("注册或登录失败,请稍后重试。"));
        return;
    }
    emit loginSucceeded(user);
}
