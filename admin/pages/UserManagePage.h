#ifndef USERMANAGEPAGE_H
#define USERMANAGEPAGE_H

#include <QTableWidget>
#include <QWidget>

// 管理员端 · 用户管理页:用户列表(手机号搜索 / 状态筛选) / 封号 / 解封
class QComboBox;
class QLineEdit;
class QPushButton;

class UserManagePage : public QWidget
{
    Q_OBJECT
public:
    explicit UserManagePage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onFreeze();          // 封号:状态置'冻结'
    void onUnfreeze();        // 解封:状态置回'正常'
    void onSelectionChanged(int currentRow, int currentColumn,
                            int previousRow, int previousColumn);

private:
    // 当前选中行用户 id;无选中返回 -1
    qlonglong selectedUserId() const;
    // 当前选中行状态;无选中返回空串
    QString selectedUserStatus() const;
    void updateButtons();

    QComboBox *m_statusFilter = nullptr;
    QLineEdit *m_phoneSearch = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_freezeBtn = nullptr;
    QPushButton *m_unfreezeBtn = nullptr;
};

#endif // USERMANAGEPAGE_H
