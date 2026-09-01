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
    DataSource *ds = AppContext::instance()->dataSource();

    // 已注册 → 直接返回
    const QueryResult existing = ds->query(QStringLiteral("users"),
                                           {},
                                           QStringLiteral("phone = ?"),
                                           QVariantList{phone});
    if (!existing.isEmpty())
        return existing.first();

    // 未注册 → 自动注册:昵称"用户"+手机号后四位
    QHash<QString, QVariant> values;
    values.insert(QStringLiteral("phone"), phone);
    values.insert(QStringLiteral("nickname"),
                  QStringLiteral("用户") + phone.right(4));
    const qlonglong id = ds->insertRow(QStringLiteral("users"), values);
    if (id <= 0) {
        qWarning() << "[UserLogin] 自动注册失败 phone =" << phone;
        return DataRow();
    }

    // 回查整行(拿到 balance / status / created_at 等默认值)
    return ds->query(QStringLiteral("users"),
                     {},
                     QStringLiteral("id = ?"),
                     QVariantList{id}).value(0);
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
    if (user.value(QStringLiteral("status")).toString() == QStringLiteral("停用")) {
        QMessageBox::warning(this, QStringLiteral("账号已停用"),
                             QStringLiteral("该账号已被管理员停用,无法登录。"));
        return;
    }

    emit loginSucceeded(user);
}
