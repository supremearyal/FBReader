/*
 * Copyright (C) 2004-2012 Geometer Plus <contact@geometerplus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <QtGui/QVBoxLayout>
#include <QtGui/QHBoxLayout>

#include <QtCore/QDebug>

#include <ZLNetworkManager.h>

#include "../image/ZLQtImageUtils.h"

#include "ZLQtPreviewWidget.h"


ZLQtButtonAction::ZLQtButtonAction(shared_ptr<ZLTreeAction> action,QWidget *parent) :
	QPushButton(parent), myAction(action) {
	connect(this, SIGNAL(clicked()), this, SLOT(onClicked()));
}

void ZLQtButtonAction::onClicked() {
	if (myAction.isNull() || !myAction->makesSense()) {
		return;
	}
	myAction->run();
}

ZLQtPreviewWidget::ZLQtPreviewWidget(QWidget *parent) : QWidget(parent) {
	myPicLabel = new QLabel;
	myPicLabel->setMaximumSize(300,300);
	myPicLabel->setMinimumSize(77,77);

	myTitleLabel = new QLabel;

	myActionsWidget = new QWidget;


	QVBoxLayout *previewLayout = new QVBoxLayout;
	QHBoxLayout *actionsLayout = new QHBoxLayout;
	myActionsWidget->setLayout(actionsLayout);

	previewLayout->addWidget(myPicLabel);
	previewLayout->addWidget(myTitleLabel);
	previewLayout->addWidget(myActionsWidget);
	setLayout(previewLayout);
}

void ZLQtPreviewWidget::fill(const ZLTreePageNode *node) {
	qDebug() << Q_FUNC_INFO;
	clear();

	shared_ptr<const ZLImage> image = node->image();
	if (!image.isNull()) {
		ZLNetworkManager::Instance().perform(image->synchronizationData());
		QPixmap pixmap = ZLQtImageUtils::ZLImageToQPixmap(image);
		myPicLabel->setPixmap(pixmap);
	}

	myTitleLabel->setText(QString::fromStdString(node->title()));
	myTitleLabel->setWordWrap(true);

	foreach(shared_ptr<ZLTreeAction> action, node->actions()) {
		if (!action->makesSense()) {
			continue;
		}
		QPushButton *actionButton = new ZLQtButtonAction(action);
		QString text = QString::fromStdString(node->actionText(action));
		actionButton->setText(text);
		myButtons.push_back(actionButton);
		myActionsWidget->layout()->addWidget(actionButton);
	}

}

void ZLQtPreviewWidget::clear() {
	myPicLabel->setPixmap(QPixmap());
	myTitleLabel->clear();
	foreach(QPushButton *button, myButtons) {
		delete button;
	}
	myButtons.clear();
}
