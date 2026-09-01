#include "PileStatusPage.h"

#include <QLabel>
#include <QVBoxLayout>

PileStatusPage::PileStatusPage(QWidget *parent)
    : QWidget(parent)
{
    auto *placeholder = new QLabel(QStringLiteral("充电桩状态页(待填充)"), this);
    placeholder->setObjectName(QStringLiteral("placeholderLabel"));
    placeholder->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(placeholder);
}
