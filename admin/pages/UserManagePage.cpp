#include "UserManagePage.h"

#include "common/AppContext.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QVBoxLayout>

// 与用户登录处的拦截逻辑保持一致(见 UserLoginWindow:status == "冻结" 拒绝登录)
static const QString kStatusNormal = QStringLiteral("正常");
static const QString kStatusFrozen = QStringLiteral("冻结");

UserManagePage::UserManagePage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("用户管理"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));

    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(QStringLiteral("按状态筛选:"), this));
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(QStringLiteral("全部用户"), QString());
    m_statusFilter->addItem(QStringLiteral("正常"), kStatusNormal);
    m_statusFilter->addItem(QStringLiteral("冻结"), kStatusFrozen);
    filterRow->addWidget(m_statusFilter);

    filterRow->addSpacing(16);
    filterRow->addWidget(new QLabel(QStringLiteral("手机号搜索:"), this));
    m_phoneSearch = new QLineEdit(this);
    m_phoneSearch->setObjectName(QStringLiteral("phoneSearchEdit"));
    m_phoneSearch->setPlaceholderText(QStringLiteral("输入手机号中的任意数字"));
    m_phoneSearch->setClearButtonEnabled(true);
    filterRow->addWidget(m_phoneSearch);
    filterRow->addStretch(1);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"),
                                        QStringLiteral("手机号"),
                                        QStringLiteral("昵称"),
                                        QStringLiteral("余额(元)"),
                                        QStringLiteral("状态"),
                                        QStringLiteral("注册时间")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_freezeBtn = new QPushButton(QStringLiteral("封号"), this);
    m_freezeBtn->setObjectName(QStringLiteral("adminDangerButton"));
    m_freezeBtn->setToolTip(QStringLiteral("冻结该用户,将无法登录"));
    m_unfreezeBtn = new QPushButton(QStringLiteral("解封"), this);
    m_unfreezeBtn->setObjectName(QStringLiteral("adminPositiveButton"));
    m_unfreezeBtn->setToolTip(QStringLiteral("恢复正常状态,可重新登录"));

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_freezeBtn);
    btnRow->addWidget(m_unfreezeBtn);
    btnRow->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);
    layout->addLayout(btnRow);

    connect(m_freezeBtn, &QPushButton::clicked, this, &UserManagePage::onFreeze);
    connect(m_unfreezeBtn, &QPushButton::clicked, this, &UserManagePage::onUnfreeze);
    connect(m_statusFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &UserManagePage::refresh);
    connect(m_phoneSearch, &QLineEdit::textChanged,
            this, &UserManagePage::refresh);
    connect(m_table, &QTableWidget::currentCellChanged,
            this, &UserManagePage::onSelectionChanged);

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            // 用户变化即刷新(含本页封号/解封、远端新用户注册、余额扣款)
            if (table == QStringLiteral("users"))
                refresh();
        });
    }

    refresh();
}

void UserManagePage::refresh()
{
    const QString filter = m_statusFilter->currentData().toString();
    const QString phoneKeyword = m_phoneSearch->text().trimmed();

    m_table->setRowCount(0);
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;

    const QueryResult users = ds->query(QStringLiteral("users"));
    QVector<DataRow> rows;
    rows.reserve(users.size());
    for (const DataRow &u : users) {
        const QString status = u.value(QStringLiteral("status")).toString();
        if (!filter.isEmpty() && status != filter)
            continue;
        if (!phoneKeyword.isEmpty()
            && !u.value(QStringLiteral("phone")).toString().contains(phoneKeyword))
            continue;
        rows << u;
    }

    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const DataRow &u = rows.at(i);
        const auto put = [&](int col, const QVariant &v) {
            auto *item = new QTableWidgetItem(v.toString());
            item->setData(Qt::UserRole, u.value(QStringLiteral("id")).toLongLong());
            item->setData(Qt::UserRole + 1,
                          u.value(QStringLiteral("status")).toString());
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(i, col, item);
        };
        put(0, u.value(QStringLiteral("id")));
        put(1, u.value(QStringLiteral("phone")));
        put(2, u.value(QStringLiteral("nickname")));
        put(3, QString::number(u.value(QStringLiteral("balance")).toDouble(), 'f', 2));
        put(4, u.value(QStringLiteral("status")));
        put(5, u.value(QStringLiteral("created_at")));
    }

    updateButtons();
}

qlonglong UserManagePage::selectedUserId() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return -1;
    const QTableWidgetItem *item = m_table->item(row, 0);
    return item ? item->data(Qt::UserRole).toLongLong() : -1;
}

QString UserManagePage::selectedUserStatus() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return QString();
    const QTableWidgetItem *item = m_table->item(row, 0);
    return item ? item->data(Qt::UserRole + 1).toString() : QString();
}

void UserManagePage::updateButtons()
{
    const QString status = selectedUserStatus();
    const bool hasSel = !status.isEmpty();
    // 已冻结 → 只能解封;正常 → 只能封号
    m_freezeBtn->setEnabled(hasSel && status != kStatusFrozen);
    m_unfreezeBtn->setEnabled(hasSel && status != kStatusNormal);
}

void UserManagePage::onSelectionChanged(int currentRow, int currentColumn,
                                        int previousRow, int previousColumn)
{
    Q_UNUSED(currentColumn);
    Q_UNUSED(previousRow);
    Q_UNUSED(previousColumn);
    updateButtons();
}

void UserManagePage::onFreeze()
{
    const qlonglong id = selectedUserId();
    const QString status = selectedUserStatus();
    if (id <= 0 || status.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要封号的用户。"));
        return;
    }
    if (status == kStatusFrozen) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("该用户已处于冻结状态。"));
        return;
    }

    const int row = m_table->currentRow();
    const QString phone = m_table->item(row, 1)->text();
    const QString nickname = m_table->item(row, 2)->text();
    const QString label = QStringLiteral("%1(%2)").arg(nickname, phone);

    QString hint = QStringLiteral("确定冻结用户「%1」吗?\n冻结后该用户将无法登录。").arg(label);
    // 有进行中的充电订单时提醒:冻结只阻止登录,不打断进行中的充电
    if (DataSource *ds = AppContext::instance()->dataSource()) {
        const QueryResult active = ds->query(QStringLiteral("orders"), {},
                                             QStringLiteral("user_id = ? AND status = '充电中'"),
                                             QVariantList{id});
        if (!active.isEmpty())
            hint += QStringLiteral("\n\n注意:该用户有进行中的充电订单,冻结不影响其本次充电,只阻止再次登录。");
    }

    if (QMessageBox::question(this, QStringLiteral("封号确认"), hint)
        != QMessageBox::Yes)
        return;

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        QHash<QString, QVariant> up;
        up.insert(QStringLiteral("status"), kStatusFrozen);
        ds->updateRows(QStringLiteral("users"), up,
                       QStringLiteral("id = ?"), QVariantList{id});
    }
}

void UserManagePage::onUnfreeze()
{
    const qlonglong id = selectedUserId();
    const QString status = selectedUserStatus();
    if (id <= 0 || status.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要解封的用户。"));
        return;
    }
    if (status != kStatusFrozen) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("该用户未处于冻结状态。"));
        return;
    }

    const int row = m_table->currentRow();
    const QString phone = m_table->item(row, 1)->text();
    const QString nickname = m_table->item(row, 2)->text();

    if (QMessageBox::question(this, QStringLiteral("解封确认"),
                              QStringLiteral("确定解封用户「%1(%2)」吗?该用户将可重新登录。")
                                  .arg(nickname, phone))
        != QMessageBox::Yes)
        return;

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        QHash<QString, QVariant> up;
        up.insert(QStringLiteral("status"), kStatusNormal);
        ds->updateRows(QStringLiteral("users"), up,
                       QStringLiteral("id = ?"), QVariantList{id});
    }
}
