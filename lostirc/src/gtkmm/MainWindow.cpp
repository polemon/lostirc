/* 
 * Copyright (C) 2002 Morten Brix Pedersen <morten@wtf.dk>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <gtk--/box.h>
#include <algorithm>
#include <functional>
#include "Tab.h"
#include "MainWindow.h"

using std::vector;
using std::string;

MainWindow* AppWin = 0;

MainWindow::MainWindow()
: Gtk::Window(GTK_WINDOW_TOPLEVEL), _app (new LostIRCApp(this)), _nb(new MainNotebook())
{
    AppWin = this;
    set_policy(1, 1, 0); // Policy for main window: user resizeable
    set_title("LostIRC "VERSION);
    set_usize(600, 400);
    key_press_event.connect(slot(this, &MainWindow::on_key_press_event));
    
    Gtk::VBox *_vbox1 = manage(new Gtk::VBox());

    _vbox1->pack_start(*_nb, 1, 1);

    add(*_vbox1);

    show_all();

    int num_of_servers = _app->start();
    if (num_of_servers == 0) {
        // Construct initial tab
        Tab *tab = newServer();
        *tab << "\00311Welcome to LostIRC!\n\nThis client is mainly keyboard oriented, so don't expect fancy menus and buttons for you to click on.\n\n\0037Available commands:\n\0038/SERVER <hostname> - connect to server.\n/JOIN <channel> - join channel.\n/PART <channel> - part channel.\n/WHOIS <nick> - whois a user.\n/NICK <nick> - change nick.\n/CTCP <nick> <request> - send CTCP requests.\n/AWAY <msg> - go away.\n/QUIT <msg> - quit IRC with <msg>.\n \n\0037Available GUI commands:\n\0038/QUERY <nick> - start query with <nick>.\n \n\0037Available keybindings:\n\0038Alt + [1-9] - switch tabs from 1-9.\nAlt + n - create new server tab.\nAlt + c - close current tab.\nTab - nick-completion and command-completion.\n";
    }
}

MainWindow::~MainWindow()
{
    delete _nb;
    delete _app;

    AppWin = 0;
}

void MainWindow::displayMessage(const string& msg, FE::Destination d)
{

    if (d == FE::CURRENT) {
        Tab *tab = _nb->getCurrent();

        *tab << msg;
    } else if (d == FE::ALL) {
        vector<Tab*> tabs;
        vector<Tab*>::const_iterator i;
        _nb->Tabs(tabs);

        for (i = tabs.begin(); i != tabs.end(); ++i) {
            *(*i) << msg;
        }
    
    }
}

void MainWindow::displayMessage(const string& msg, FE::Destination d, ServerConnection *conn)
{
    if (d == FE::CURRENT) {
        Tab *tab = _nb->getCurrent(conn);

        *tab << msg;
    } else if (d == FE::ALL) {
        vector<Tab*> tabs;
        vector<Tab*>::const_iterator i;
        _nb->findTabs(conn, tabs);

        for (i = tabs.begin(); i != tabs.end(); ++i) {
            *(*i) << msg;
        }
    }
}

void MainWindow::displayMessage(const string& msg, ChannelBase& chan, ServerConnection *conn)
{
    Tab *tab = _nb->findTab(chan.getName(), conn);

    // if the channel doesn't exist, it's probably a query. (the channel is
    // created on join) - there is also a hack here to ensure that it's not
    // a channel
    if (!tab && chan.getName().at(0) != '#') {
        tab = _nb->addQueryTab(chan.getName(), conn);
    }

    if (tab)
        *tab << msg;
}

void MainWindow::join(const string& nick, Channel& chan, ServerConnection *conn)
{
    Tab *tab = _nb->findTab(chan.getName(), conn);
    if (!tab) {
        tab = _nb->addChannelTab(chan.getName(), conn);
        return;
    }
    tab->insertUser(nick);
}

void MainWindow::part(const string& nick, Channel& chan, ServerConnection *conn)
{
    Tab *tab = _nb->findTab(chan.getName(), conn);
    if (tab) {
        if (nick == conn->Session.nick) {
            // It's us who's parting
            tab->setInActive();
        }
        tab->removeUser(nick);
    }
}

void MainWindow::kick(const string& kicker, Channel& chan, const string& nick, const string& msg, ServerConnection *conn)
{
    Tab *tab = _nb->findTab(chan.getName(), conn);
    if (nick == conn->Session.nick) {
        // It's us who's been kicked
        tab->setInActive();
    }
    tab->removeUser(nick);
}


void MainWindow::quit(const string& nick, vector<ChannelBase*> chans, ServerConnection *conn)
{
    vector<ChannelBase*>::const_iterator i;

    for (i = chans.begin(); i != chans.end(); ++i) {
        if (Tab *tab = _nb->findTab((*i)->getName(), conn))
            tab->removeUser(nick);
    }
}

void MainWindow::nick(const string& nick, const string& to, vector<ChannelBase*> chans, ServerConnection *conn)
{
    vector<ChannelBase*>::const_iterator i;

    for (i = chans.begin(); i != chans.end(); ++i) {
        if (Tab *tab = _nb->findTab((*i)->getName(), conn))
              tab->renameUser(nick, to);
    }
}

void MainWindow::CUMode(const string& nick, Channel& chan, const std::vector<User>& users, ServerConnection *conn)
{
    Tab *tab = _nb->findTab(chan.getName(), conn);

    std::vector<User>::const_iterator i;
    for (i = users.begin(); i != users.end(); ++i) {
        tab->removeUser(i->nick);
        tab->insertUser(i->nick, i->getMode());
    }
}

void MainWindow::names(Channel& c, ServerConnection *conn)
{
    Tab *tab = _nb->findTab(c.getName(), conn);

    std::vector<User*> users = c.getUsers();
    std::vector<User*>::const_iterator i;

    for (i = users.begin(); i != users.end(); ++i) {
        tab->insertUser((*i)->nick, (*i)->getMode());
    }
}

void MainWindow::highlight(ChannelBase& chan, ServerConnection* conn)
{
    Tab *tab = _nb->findTab(chan.getName(), conn);

    if (tab)
          _nb->highlight(tab);
}

void MainWindow::away(bool away, ServerConnection* conn)
{
    vector<Tab*> tabs;

    _nb->findTabs(conn, tabs);
    if (away) {
        std::for_each(tabs.begin(), tabs.end(), std::mem_fun(&Tab::setAway));
    } else {
        std::for_each(tabs.begin(), tabs.end(), std::mem_fun(&Tab::setUnAway));
    }
}

void MainWindow::disconnected(ServerConnection* conn)
{
    vector<Tab*> tabs;

    _nb->findTabs(conn, tabs);

    std::for_each(tabs.begin(), tabs.end(), std::mem_fun(&Tab::setInActive));
}

void MainWindow::newTab(ServerConnection *conn)
{
    string name = "server";
    conn->Session.servername = name;
    Tab *tab = _nb->addChannelTab(name, conn);
    _nb->show_all();
    // XXX: this is a hack for a "bug" in the gtkmm code which makes the
    // application crash in the start when no pages exists, even though we
    // added one above... doing set_page(0) will somehow add it fully.
    if (_nb->get_current_page_num() == -1) {
        _nb->set_page(0);
    }
    tab->setInActive();
}

Tab* MainWindow::newServer()
{
    string name = "server";
    ServerConnection *conn = _app->newServer();
    conn->Session.servername = name;
    Tab *tab = _nb->addChannelTab(name, conn);
    tab->setInActive();
    return tab;
}

gint MainWindow::on_key_press_event(GdkEventKey* e)
{
    // Default keybindings. Still needs work.
    if ((e->keyval == GDK_0) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(9);
    }
    else if ((e->keyval == GDK_1) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(0);
    }
    else if ((e->keyval == GDK_2) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(1);
    }
    else if ((e->keyval == GDK_3) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(2);
    }
    else if ((e->keyval == GDK_4) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(3);
    }
    else if ((e->keyval == GDK_5) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(4);
    }
    else if ((e->keyval == GDK_6) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(5);
    }
    else if ((e->keyval == GDK_7) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(6);
    }
    else if ((e->keyval == GDK_8) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(7);
    }
    else if ((e->keyval == GDK_9) && (e->state & GDK_MOD1_MASK)) {
        _nb->set_page(8);
    }
    else if ((e->keyval == GDK_c) && (e->state & GDK_MOD1_MASK)) {
        TabChannel *tab = dynamic_cast<TabChannel*>(_nb->getCurrent());
        if (tab && tab->getConn()->Session.isConnected && tab->isActive()) {
            // It's a channel, so we need to part it
            tab->getConn()->sendPart(tab->getLabel()->get_text(), "");
        } else {
            // Query
            _nb->getCurrent()->getConn()->removeChannel(_nb->getCurrent()->getLabel()->get_text());
        }
        _nb->closeCurrent();
    }
    else if ((e->keyval == GDK_p) && (e->state & GDK_MOD1_MASK)) {
        if (!_nb->getCurrent()->hasPrefs) {
            _nb->getCurrent()->startPrefs();
        } else {
            _nb->getCurrent()->endPrefs();
        }
    }
    else if ((e->keyval == GDK_n) && (e->state & GDK_MOD1_MASK)) {
        newServer();
    }
    else if ((e->keyval == GDK_q) && (e->state & GDK_MOD1_MASK)) {
        Gtk::Main::quit();
    }
    else if (e->keyval == GDK_Up || e->keyval == GDK_Tab || e->keyval == GDK_Down) {
        if (!_nb->getCurrent()->hasPrefs) {
            _nb->getCurrent()->getEntry()->on_key_press_event(e);
            gtk_signal_emit_stop_by_name(GTK_OBJECT(this->gtkobj()), "key_press_event");
        }
    }
    else if ((e->keyval == GDK_f) && (e->state & GDK_MOD1_MASK)) {
        _nb->setFont();
    }
    return 0;
}
