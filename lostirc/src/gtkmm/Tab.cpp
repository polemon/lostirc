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

#include <ctime>
#include <ServerConnection.h>
#include <Utils.h>
#include "Tab.h"
#include <algorithm>
#include <gtk--/frame.h>

using std::vector;
using std::string;

// Color code definitions taken from palette.c in xchat, and modified just a
// bit

        //{0, 0xcf3c, 0xcf3c, 0xcf3c}, /* 0  white */
GdkColor colors[] = { 
        {0, 0xFFFF, 0xFFFF, 0xFFFF}, /* 0  white */
        {0, 0x0000, 0x0000, 0x0000}, /* 1  black */
        {0, 0x0000, 0x0000, 0xcccc}, /* 2  blue */
        {0, 0x0000, 0xcccc, 0x0000}, /* 3  green */
        {0, 0xdddd, 0x0000, 0x0000}, /* 4  red */
        {0, 0xaaaa, 0x0000, 0x0000}, /* 5  light red */
        {0, 0xbbbb, 0x0000, 0xbbbb}, /* 6  purple */
        {0, 0xffff, 0xaaaa, 0x0000}, /* 7  orange */
        {0, 0xeeee, 0xdddd, 0x2222}, /* 8  yellow */
        {0, 0x3333, 0xdede, 0x5555}, /* 9  green */
        {0, 0x0000, 0xcccc, 0xcccc}, /* 10 aqua */
        {0, 0x3333, 0xdddd, 0xeeee}, /* 11 light aqua */
        {0, 0x0000, 0x0000, 0xffff}, /* 12 blue */
        {0, 0xeeee, 0x2222, 0xeeee}, /* 13 light purple */
        {0, 0x7777, 0x7777, 0x7777}, /* 14 grey */
        {0, 0x9999, 0x9999, 0x9999}, /* 15 light grey */
        {0, 0xbe00, 0xbe00, 0xbe00}, /* 16 marktext Back (white) */
        {0, 0x0000, 0x0000, 0x0000}, /* 17 marktext Fore (black) */
        {0, 0xcf3c, 0xcf3c, 0xcf3c}, /* 18 foreground (white) */
        {0, 0x0000, 0x0000, 0x0000}, /* 19 background (black) */
};

Tab::Tab(Gtk::Label *label, ServerConnection *conn, Gdk_Font *font)
    : Gtk::VBox(), _label(label), _conn(conn), is_highlighted(false), _font(font),
    is_on_channel(true)

{
    // To hold current context (colors) for Text widget
    _current_cx = new Gtk::Text::Context;

    // Creating HBox; will contain 2 widgets, a scrollwindow and an entry
    _hbox = manage(new Gtk::HBox(false, 3)); 
    _scrollwindow = manage(new Gtk::ScrolledWindow());
    _entry = manage(new Entry(this));

    // Attaching Gtk::Text to scollwindow
    _text = manage(new Gtk::Text());
    _text->set_word_wrap(true);
    _scrollwindow->set_policy(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    _scrollwindow->add(*_text);

    //_away = manage(new Gtk::Label("You are away"));
    setStyle();

    _hbox->pack_start(*_scrollwindow);
    pack_start(*_hbox);
    _hbox2 = manage(new Gtk::HBox());
    pack_start(*_hbox2, 0, 1);
    _hbox2->pack_start(*_entry, 1, 1);

}

Tab::~Tab()
{
    delete _current_cx;
}

Gtk::Text* Tab::getText()
{
    return _text;
}

Gtk::HBox* Tab::getHBox()
{
    return _hbox;
}

Gtk::Label* Tab::getLabel()
{
    return _label;
}

Entry* Tab::getEntry()
{
    return _entry;
}

ServerConnection* Tab::getConn()
{
    return _conn;
}

void Tab::setFont(Gdk_Font *font)
{
    _font = font;
}

void Tab::setStyle() {
    // Should go into ressource file!
    GdkColor col1;
    col1.red   = 0;
    col1.green = 0;
    col1.blue  = 0;

    GdkColor col2;
    col2.red   = 50000;
    col2.green = 50000;
    col2.blue  = 50000;
    
    Gtk::Style *style = Gtk::Style::create();
    style->set_font(*_font);
    style->set_base(GTK_STATE_NORMAL, col1);
    style->set_bg(GTK_STATE_NORMAL, col1);
    style->set_text(GTK_STATE_NORMAL, col2);
    style->set_fg(GTK_STATE_PRELIGHT, col2);
    _text->set_style(*style);
}


bool isDigit(const string& str)
{
    stringstream ss(str);
    int i;
    ss >> i;
    if (ss.fail())
          return false;
    else
          return true;
}

void Tab::parseAndInsert(const string& str)
{
    // Add timestamp 
    time_t timeval = time(0);
    char tim[11];
    strftime(tim, 10, "%H:%M:%S ", localtime(&timeval));

    insertWithColor(0, string(tim));
    string line(str);

    string::size_type lastPos = line.find_first_not_of("\003", 0);
    string::size_type pos = line.find_first_of("\003", lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {   
        // Check for digits
        if (Utils::isDigit(line.substr(lastPos, 2))) {
            int color = Utils::stoi(line.substr(lastPos, 2));
            insertWithColor(color, line.substr(lastPos + 2, (pos - lastPos) - 2));
        } else if (Utils::isDigit(line.substr(lastPos, 1))) {
            int color = Utils::stoi(line.substr(lastPos, 1));
            insertWithColor(color, line.substr(lastPos + 1, (pos - lastPos) - 1));
        } else {
            insertWithColor(0, line.substr(lastPos, pos - lastPos));
        }

        lastPos = line.find_first_not_of("\003", pos);
        pos = line.find_first_of("\003", lastPos);
    }

}

void Tab::insertWithColor(int color, const string& str)
{   
/*
    Gdk_Color colors[10];

    colors[0] = Gdk_Color("#C5C2C5");
    colors[1] = Gdk_Color("#FFFFFF");
    colors[2] = Gdk_Color("#FFABCF");
    colors[3] = Gdk_Color("#9AAB4F");
    colors[4] = Gdk_Color("#f9ef25");
    colors[5] = Gdk_Color("#ea6b6b");
    colors[6] = Gdk_Color("#6bdde5");
    colors[7] = Gdk_Color("#6b8ae5");
    colors[8] = Gdk_Color("#4aff4a");
    colors[9] = Gdk_Color("#5ea524");*/

    // Find out whether we need to scroll this widget auto
    float vscroll = _text->get_vadjustment()->get_value();
    float page_size = _text->get_vadjustment()->get_page_size();
    float upper_vscroll = _text->get_vadjustment()->get_upper();

    bool autoscroll = false;
    if (vscroll + page_size == upper_vscroll)
          autoscroll = true;

    _text->freeze();

    _current_cx->set_foreground(colors[color]);
    _text->insert(*_current_cx, str);

    _text->thaw();

    // Scroll it down, if true
    if (autoscroll)
        _text->get_vadjustment()->set_value(_text->get_vadjustment()->get_upper());
}

void Tab::setAway()
{
    bool away = false;

    Gtk::Box_Helpers::BoxList::iterator i;

    for (i = _hbox2->children().begin(); i != _hbox2->children().end(); ++i) {
        Gtk::Label *a = dynamic_cast<Gtk::Label*>((*i)->get_widget());
        if (a)
              away = true;
    }

    if (!away) {
        Gtk::Label *a = manage(new Gtk::Label("You are away"));
        _hbox2->pack_start(*a);
        _hbox2->show_all();
    }
}

void Tab::setUnAway()
{
    Gtk::Box_Helpers::BoxList::iterator i;

    for (i = _hbox2->children().begin(); i != _hbox2->children().end();) {
        Gtk::Label *a = dynamic_cast<Gtk::Label*>((*i)->get_widget());
        if (a) {
            i = _hbox2->children().erase(i);
        } else {
            ++i;
        }
    }
    _hbox2->show_all();
}

TabQuery::TabQuery(Gtk::Label *label, ServerConnection *conn, Gdk_Font *font)
    : Tab(label, conn, font)
{

}

TabChannel::TabChannel(Gtk::Label *label, ServerConnection *conn, Gdk_Font *font)
    : Tab(label, conn, font)
{

    Gtk::ScrolledWindow *swin = manage(new Gtk::ScrolledWindow());
    swin->set_policy(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    Gtk::VBox *v = manage(new Gtk::VBox());
    _users = manage(new Gtk::Label("Not on channel"));
    _clist = manage(new Gtk::CList(2));
    _clist->set_column_width(0, 10);
    _clist->set_auto_sort(1);
    _clist->set_sort_type(GTK_SORT_DESCENDING);
    _clist->set_usize(100, 100);
    swin->add(*_clist);

    getHBox()->pack_start(*v, 0, 0, 0);
    v->pack_start(*_users, 0, 0, 0);
    v->pack_start(*swin, 1, 1, 0);
}

void TabChannel::insertUser(const vector<string>& users)
{
    _clist->rows().push_back(users);
    size_t size = _clist->rows().size();
    stringstream ss;
    ss << size;
    _users->set_text(ss.str() + " users");
}

void TabChannel::insertUser(const string& user)
{
    vector<string> users;
    users.push_back(" ");
    users.push_back(user);
    _clist->rows().push_back(users);
    size_t size = _clist->rows().size();
    stringstream ss;
    ss << size;
    _users->set_text(ss.str() + " users");
}

void TabChannel::insertUser(const Mode& m)
{
    vector<string> tmp;

    switch (m.mode)
    {
        case IRC::OP:
            tmp.push_back("@");
            break;
        case IRC::VOICE:
            tmp.push_back("+");
            break;
        default:
            tmp.push_back(" ");
    }

    tmp.push_back(m.nick);
    insertUser(tmp);

}

void TabChannel::removeUser(const string& nick)
{
    Gtk::CList_Helpers::RowIterator i = _clist->rows().begin();

    while(i != _clist->rows().end())
    {
        int row = i->get_row_num();
        string text = _clist->cell(row, 1).get_text();

        if (text == nick) {
            _clist->rows().remove(_clist->row(row));
            break;
        }
        i++;
    }
    size_t size = _clist->rows().size();
    stringstream ss;
    ss << size;
    _users->set_text(ss.str() + " users");
}

void TabChannel::renameUser(const string& from, const string& to)
{
    Gtk::CList_Helpers::RowIterator i = _clist->rows().begin();

    while(i != _clist->rows().end())
    {
        int row = i->get_row_num();
        string text = _clist->cell(row, 1).get_text();

        if (text == from) {
            _clist->cell(row, 1).set_text(to);
            break;
        }
        i++;
    }

}

bool TabChannel::findUser(const string& nick)
{
    Gtk::CList_Helpers::RowIterator i = _clist->rows().begin();

    while(i != _clist->rows().end())
    {
        int row = i->get_row_num();
        string text = _clist->cell(row, 1).get_text();

        if (text == nick) {
            return true;
        }
        i++;
    }
    return false;
}

Gtk::CList* TabChannel::getCList()
{
    return _clist;
}

bool TabChannel::nickCompletion(const string& word, string& str)
{
    Gtk::CList_Helpers::RowIterator i = getCList()->rows().begin();

    int matches = 0;
    string nicks;
    // Convert it to lowercase so we can search ignoring the case
    string lcword = word;
    lcword = Utils::tolower(lcword);
    while(i != getCList()->rows().end())
    {
        int row = i->get_row_num();
        string nick = getCList()->cell(row, 1).get_text();

        // Lower case again
        string lcnick = nick;
        lcnick = Utils::tolower(lcnick);
        if (lcword == lcnick.substr(0, lcword.length())) {
            str = nick;
            nicks += nick + " ";
            matches++;
        }
        i++;
    }
    if (matches == 1) {
        return true;
    } else if (matches > 1) {
        str = nicks + "\n";
        return false;
    } else if (matches == 0) {
        str = "";
        return false;
    }
}


