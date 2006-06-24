/*
 * FBReader -- electronic book reader
 * Copyright (C) 2004-2006 Nikolay Pultsin <geometer@mawhrin.net>
 * Copyright (C) 2005 Mikhail Sobolev <mss@mawhrin.net>
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <abstract/ZLDeviceInfo.h>

#include <maemo/GtkViewWidget.h>
#include <maemo/GtkKeyUtil.h>
#include <maemo/GtkPaintContext.h>
#include <maemo/GtkDialogManager.h>

#include "../../common/description/BookDescription.h"
#include "../../common/fbreader/BookTextView.h"
#include "../../common/fbreader/FootnoteView.h"
#include "../../common/fbreader/ContentsView.h"
#include "../../common/fbreader/CollectionView.h"
#include "GtkFBReader.h"

static bool quitFlag = false;

static bool acceptAction() {
	return
		!quitFlag &&
		GtkDialogManager::isInitialized() &&
		!((GtkDialogManager&)GtkDialogManager::instance()).isWaiting();
}

static bool applicationQuit(GtkWidget*, GdkEvent*, gpointer data) {
	if (acceptAction()) {
		((GtkFBReader*)data)->doAction(ACTION_QUIT);
	}
	return true;
}

static void repaint(GtkWidget*, GdkEvent*, gpointer data) {
	if (acceptAction()) {
		((GtkFBReader*)data)->repaintView();
	}
}

struct ActionSlotData {
	ActionSlotData(GtkFBReader *reader, ActionCode code) { Reader = reader; Code = code; }
	GtkFBReader *Reader;
	ActionCode Code;
};

static void actionSlot(GtkWidget*, GdkEventButton*, gpointer data) {
	if (acceptAction()) {
		ActionSlotData *uData = (ActionSlotData*)data;
		uData->Reader->doAction(uData->Code);
	}
}

static void menuActionSlot(GtkWidget *, gpointer data) {
	if (acceptAction()) {
		ActionSlotData *uData = (ActionSlotData*)data;
		uData->Reader->doAction(uData->Code);
	}
}

static void handleKey(GtkWidget*, GdkEventKey *key, gpointer data) {
	if (acceptAction()) {
		((GtkFBReader*)data)->handleKeyEventSlot(key);
	}
}

//static void hardwareStateHandler(osso_hw_state_t *state, gpointer data) {
//	// TODO: what is more expensive operation here? :)
//	if (acceptAction() && state->shutdown_ind) {
//		((GtkFBReader*)data)->doAction(ACTION_QUIT);
//	}
//}

GtkFBReader::GtkFBReader(const std::string& bookToOpen) : FBReader(new GtkPaintContext(), bookToOpen) {
	myApp = HILDON_APP(hildon_app_new());
	hildon_app_set_title(myApp, "FBReader");
	hildon_app_set_two_part_title(myApp, FALSE);

	osso_initialize("FBReader", "0.0", true, 0);

//	osso_context_t *context = osso_initialize("FBReader", "0.0", true, 0);
//
//	// TODO: should we actually check if the context was successfully created?
//	osso_hw_state_t state = { true, false, false, false, OSSO_DEVMODE_NORMAL };
//	osso_hw_set_event_cb(context, &state, hardwareStateHandler, this);

	myAppView = HILDON_APPVIEW(hildon_appview_new(0));

	myMenu = GTK_MENU(hildon_appview_get_menu(myAppView));

	myToolbar = GTK_TOOLBAR(gtk_toolbar_new());
	gtk_toolbar_set_show_arrow(myToolbar, false);
	gtk_toolbar_set_orientation(myToolbar, GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(myToolbar, GTK_TOOLBAR_ICONS);

	initWindow(this);

	gtk_widget_show_all(GTK_WIDGET(myMenu));

	hildon_appview_set_toolbar(myAppView, myToolbar);

	myViewWidget = new GtkViewWidget(this, (ZLViewWidget::Angle)AngleStateOption.value());
	gtk_container_add(GTK_CONTAINER(myAppView), ((GtkViewWidget*)myViewWidget)->area());
	gtk_signal_connect_after(GTK_OBJECT(((GtkViewWidget*)myViewWidget)->area()), "expose_event", GTK_SIGNAL_FUNC(repaint), this);

	hildon_app_set_appview(myApp, myAppView);

	gtk_widget_show_all(GTK_WIDGET(myApp));

	setMode(BOOK_TEXT_MODE);

	gtk_signal_connect(GTK_OBJECT(myApp), "delete_event", GTK_SIGNAL_FUNC(applicationQuit), this);
	gtk_signal_connect(GTK_OBJECT(myApp), "key_press_event", G_CALLBACK(handleKey), this);

	myFullScreen = false;
}

ActionSlotData *GtkFBReader::getSlotData(ActionCode	id) {
	ActionSlotData *data = myActions[id];

	if (data == NULL) {
		data = new ActionSlotData(this, id);
		myActions[id] = data;
	}

	return data;
}

void GtkFBReader::addMenubarItem(GtkMenu *menu, Menubar::ItemPtr item) {
	GtkWidget *widget = 0;

	switch(item->type()) {
		case Menubar::Item::SEPARATOR_ITEM:
			widget = gtk_separator_menu_item_new();
			break;

		case Menubar::Item::MENU_ITEM:
			{
				const Menubar::MenuItem &menuItem = (const Menubar::MenuItem&)*item;
				ActionCode id = (ActionCode)menuItem.actionId();
				widget = gtk_menu_item_new_with_label(menuItem.name().c_str());
				g_signal_connect(G_OBJECT(widget), "activate", G_CALLBACK(menuActionSlot), getSlotData(id));
				myMenuItems[id] = (GtkMenuItem*)widget;
			}
			break;

		case Menubar::Item::SUBMENU_ITEM:
			{
				const Menubar::SubMenuItem &subMenuItem = (const Menubar::SubMenuItem&)*item;
				widget = gtk_menu_item_new_with_label(subMenuItem.menuName().c_str());
				GtkMenu *subMenu = GTK_MENU(gtk_menu_new());
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget), GTK_WIDGET(subMenu));

				const Menubar::ItemVector &items = subMenuItem.items();
				for (Menubar::ItemVector::const_iterator it = items.begin(); it != items.end(); ++it) {
					addMenubarItem(subMenu, *it);
				}
			}
			break;
	}

	if (widget != 0) {
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(widget));
	}
}

GtkFBReader::~GtkFBReader() {
	for (std::map<ActionCode,ActionSlotData*>::iterator item = myActions.begin(); item != myActions.end(); ++item) {
		delete item->second;
	}

	delete (GtkViewWidget*)myViewWidget;
}

void GtkFBReader::handleKeyEventSlot(GdkEventKey *event) {
	doActionByKey(GtkKeyUtil::keyName(event));
}

void GtkFBReader::toggleFullscreenSlot() {
	myFullScreen = !myFullScreen;

	hildon_appview_set_fullscreen(myAppView, myFullScreen);
	if (myFullScreen) {
		gtk_widget_hide(GTK_WIDGET(myToolbar));
	} else if (!myFullScreen) {
		gtk_widget_show(GTK_WIDGET(myToolbar));
	}
}

bool GtkFBReader::isFullscreen() const {
	return myFullScreen;
}

void GtkFBReader::quitSlot() {
	if (!quitFlag) {
		quitFlag = true;
		delete this;
		gtk_main_quit();
	}
}

void GtkFBReader::addToolbarItem(Toolbar::ItemPtr item) {
	GtkToolItem *gtkItem;
	if (item->isButton()) {
		const Toolbar::ButtonItem &buttonItem = (const Toolbar::ButtonItem&)*item;
		GtkWidget *image = gtk_image_new_from_file((ImageDirectory + "/FBReader/" + buttonItem.iconName() + ".png").c_str());
		gtkItem = gtk_tool_item_new();
		GtkWidget *ebox = gtk_event_box_new();

		gtk_container_add(GTK_CONTAINER(ebox), image);
		gtk_container_add(GTK_CONTAINER(gtkItem), ebox);

		gtk_tool_item_set_homogeneous(gtkItem, false);
		gtk_tool_item_set_expand(gtkItem, false);

		GTK_WIDGET_UNSET_FLAGS(gtkItem, GTK_CAN_FOCUS);
		ActionCode id = (ActionCode)buttonItem.actionId();
		g_signal_connect(G_OBJECT(ebox), "button_press_event", GTK_SIGNAL_FUNC(actionSlot), getSlotData(id));
	} else {
		gtkItem = gtk_separator_tool_item_new();
		gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(gtkItem), false);
	}
	gtk_toolbar_insert(myToolbar, gtkItem, -1);
	myButtons[item] = gtkItem;
}

void GtkFBReader::addMenubarItem(Menubar::ItemPtr item) {
	addMenubarItem(myMenu, item);
}

void GtkFBReader::refresh() {
	const Toolbar::ItemVector &items = toolbar().items();
	bool enableToolbarSpace = false;
	for (Toolbar::ItemVector::const_iterator it = items.begin(); it != items.end(); ++it) {
		GtkToolItem *toolItem = myButtons[*it];
		if ((*it)->isButton()) {
			const Toolbar::ButtonItem &button = (const Toolbar::ButtonItem&)**it;
			int id = button.actionId();

			bool visible = application().isActionVisible(id);
			bool enabled = application().isActionEnabled(id);

			if (toolItem != 0) {
				gtk_tool_item_set_visible_horizontal(toolItem, visible);
				if (visible) {
					enableToolbarSpace = true;
				}
				/*
				 * Not sure, but looks like gtk_widget_set_sensitive(WIDGET, false)
				 * does something strange if WIDGET is already insensitive.
				 */
				bool alreadyEnabled = GTK_WIDGET_STATE(toolItem) != GTK_STATE_INSENSITIVE;
				if (enabled != alreadyEnabled) {
					gtk_widget_set_sensitive(GTK_WIDGET(toolItem), enabled);
				}
			}

			GtkMenuItem *item = myMenuItems[(ActionCode)button.actionId()];
			if (item != 0) {
				if (visible) {
					gtk_widget_show(GTK_WIDGET(item));
				} else {
					gtk_widget_hide(GTK_WIDGET(item));
				}
				bool alreadyEnabled = GTK_WIDGET_STATE(item) != GTK_STATE_INSENSITIVE;
				if (enabled != alreadyEnabled) {
					gtk_widget_set_sensitive(GTK_WIDGET(item), enabled);
				}
			}
		} else {
			if (toolItem != 0) {
				gtk_tool_item_set_visible_horizontal(toolItem, enableToolbarSpace);
				enableToolbarSpace = false;
			}
		}
	}
}

// vim:ts=2:sw=2:noet
