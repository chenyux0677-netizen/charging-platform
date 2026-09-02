#include "ProfilePage.h"

#include "common/AppContext.h"

#include <QFileDialog>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

ProfilePage::ProfilePage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("我的"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));
    title->setAlignment(Qt::AlignCenter);

    m_avatarBtn = new QPushButton(QStringLiteral("头像"), this);
    m_avatarBtn->setObjectName(QStringLiteral("avatarBtn"));
    m_avatarBtn->setFixedSize(72, 72);

    m_nicknameLabel = new QLabel(this);
    m_nicknameLabel->setObjectName(QStringLiteral("nicknameLabel"));
    m_nicknameLabel->setAlignment(Qt::AlignCenter);

    m_balanceLabel = new QLabel(this);
    m_balanceLabel->setObjectName(QStringLiteral("balanceLabel"));
    m_balanceLabel->setAlignment(Qt::AlignCenter);

    m_rechargeBtn = new QPushButton(QStringLiteral("充值"), this);
    m_rechargeBtn->setObjectName(QStringLiteral("rechargeBtn"));

    m_changeNicknameBtn = new QPushButton(QStringLiteral("修改昵称"), this);
    m_changeNicknameBtn->setObjectName(QStringLiteral("changeNicknameBtn"));

    m_logoutBtn = new QPushButton(QStringLiteral("退出登录"), this);
    m_logoutBtn->setObjectName(QStringLiteral("logoutBtn"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addSpacing(8);
    layout->addWidget(m_avatarBtn, 0, Qt::AlignHCenter);
    layout->addWidget(m_nicknameLabel);
    layout->addWidget(m_balanceLabel);
    layout->addSpacing(12);
    layout->addWidget(m_rechargeBtn);
    layout->addWidget(m_changeNicknameBtn);
    layout->addSpacing(20);
    layout->addWidget(m_logoutBtn);
    layout->addStretch(1);

    connect(m_avatarBtn, &QPushButton::clicked, this, &ProfilePage::onAvatarClicked);
    connect(m_rechargeBtn, &QPushButton::clicked, this, &ProfilePage::onRecharge);
    connect(m_changeNicknameBtn, &QPushButton::clicked,
            this, &ProfilePage::onChangeNickname);
    connect(m_logoutBtn, &QPushButton::clicked, this, &ProfilePage::logoutRequested);
}

void ProfilePage::reloadUser()
{
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const qlonglong id =
        AppContext::instance()->currentUser().value(QStringLiteral("id")).toLongLong();
    const QueryResult rows = ds->query(QStringLiteral("users"), {},
                                       QStringLiteral("id = ?"), QVariantList{id});
    if (!rows.isEmpty()) {
        AppContext::instance()->setCurrentUser(rows.first());
        refresh();
    }
}

void ProfilePage::refresh()
{
    const DataRow user = AppContext::instance()->currentUser();
    if (user.isEmpty())
        return;
    m_nicknameLabel->setText(user.value(QStringLiteral("nickname")).toString());
    m_balanceLabel->setText(QStringLiteral("余额:%1 元")
                                .arg(user.value(QStringLiteral("balance")).toDouble(), 0, 'f', 2));

    const QString avatarPath = user.value(QStringLiteral("avatar")).toString();
    QPixmap pix;
    if (!avatarPath.isEmpty() && pix.load(avatarPath)) {
        m_avatarBtn->setIcon(QIcon(pix.scaled(72, 72, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation)));
        m_avatarBtn->setIconSize(QSize(72, 72));
        m_avatarBtn->setText(QString());
    } else {
        m_avatarBtn->setIcon(QIcon());
        m_avatarBtn->setText(QStringLiteral("头像"));
    }
}

void ProfilePage::onRecharge()
{
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    bool ok = false;
    const double amount = QInputDialog::getDouble(
        this, QStringLiteral("充值"), QStringLiteral("充值金额(元):"),
        100.0, 0.01, 1000000.0, 2, &ok);
    if (!ok)
        return;

    const qlonglong id =
        AppContext::instance()->currentUser().value(QStringLiteral("id")).toLongLong();
    // 加钱在服务器端完成,冻结/不存在的用户不会入账
    if (ds->rechargeBalance(id, amount)) {
        reloadUser();
        QMessageBox::information(this, QStringLiteral("充值成功"),
                                 QStringLiteral("已充值 %1 元。").arg(amount, 0, 'f', 2));
    } else {
        QMessageBox::warning(this, QStringLiteral("充值失败"),
                             QStringLiteral("充值失败,可能账号不存在或已被冻结。"));
    }
}

void ProfilePage::onChangeNickname()
{
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("修改昵称"), QStringLiteral("新昵称:"),
        QLineEdit::Normal,
        AppContext::instance()->currentUser().value(QStringLiteral("nickname")).toString(),
        &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    const qlonglong id =
        AppContext::instance()->currentUser().value(QStringLiteral("id")).toLongLong();
    QHash<QString, QVariant> up;
    up.insert(QStringLiteral("nickname"), name.trimmed());
    ds->updateRows(QStringLiteral("users"), up,
                   QStringLiteral("id = ?"), QVariantList{id});
    reloadUser();
}

void ProfilePage::onAvatarClicked()
{
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;

    const qlonglong id =
        AppContext::instance()->currentUser().value(QStringLiteral("id")).toLongLong();
    QHash<QString, QVariant> up;
    up.insert(QStringLiteral("avatar"), path);
    ds->updateRows(QStringLiteral("users"), up,
                   QStringLiteral("id = ?"), QVariantList{id});
    reloadUser();
}
