#include "SalesPage.h"

#include <QLabel>
#include <QVBoxLayout>

SalesPage::SalesPage(QWidget *parent)
    : QWidget(parent)
{
    auto *placeholder = new QLabel(QStringLiteral("销售统计页(待填充)"), this);
    placeholder->setObjectName(QStringLiteral("placeholderLabel"));
    placeholder->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(placeholder);
}
