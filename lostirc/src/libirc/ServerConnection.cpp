/*
 * Copyright (C) 2002-2004 Morten Brix Pedersen <morten@wtf.dk>
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

#include <config.h>
#include <iostream>
#include <cstring>
#include <functional>
#include <algorithm>
#include <unistd.h>
#include "ServerConnection.h"
#include "LostIRCApp.h"
#include "Commands.h"

using Glib::ustring;
using std::vector;

namespace algo
{

  struct deletePointer : public std::unary_function<ChannelBase*, void>
  {
      deletePointer() { }
      void operator() (ChannelBase* x) {
          delete x;
      }
  };

}

ServerConnection::ServerConnection(const ustring& host, const ustring& nick, int port, bool doconnect)
    : _parser(this)
{
    Session.nick = nick;
    Session.servername = host;
    Session.host = host;
    Session.port = port;
    Session.isConnected = false;
    Session.hasRegistered = false;
    Session.isAway = false;
    Session.endOfMotd = false;
    Session.sentLagCheck = false;
    Session.realname = App->options.realname;

    _socket.on_host_resolved.connect(sigc::mem_fun(*this, &ServerConnection::on_host_resolved));
    _socket.on_connected.connect(sigc::mem_fun(*this, &ServerConnection::on_connected));
    _socket.on_data_pending.connect(sigc::mem_fun(*this, &ServerConnection::onReadData));
    _socket.on_error.connect(sigc::mem_fun(*this, &ServerConnection::on_error));

    App->fe->newTab(this);

    if (doconnect)
          connect();
}

ServerConnection::~ServerConnection()
{
    for_each(Session.channels.begin(), Session.channels.end(), algo::deletePointer());
}

void ServerConnection::connect(const ustring &host, int port, const ustring& pass)
{
    Session.servername = host;
    Session.host = host;
    Session.password = pass;
    Session.port = port;

    connect();
}

void ServerConnection::reconnect()
{
    connect(Session.servername, Session.port, Session.password);
}

void ServerConnection::doCleanup()
{
    // called when we get disconnected or connect to a new server, need to
    // clean various things up.
    Session.isConnected = false;
    Session.isConnecting = false;
    Session.hasRegistered = false;
    Session.isAway = false;
    Session.endOfMotd = false;
    Session.sentLagCheck = false;
    _bufpos = 0;

    if (signal_connection.connected())
          signal_connection.disconnect();

    for_each(Session.channels.begin(), Session.channels.end(), algo::deletePointer());
    Session.channels.clear();

    _socket.disconnect();
}

void ServerConnection::disconnect()
{
    #ifdef DEBUG
    App->log << "ServerConnection::disconnect()" << std::endl;
    #endif

    doCleanup();

    FE::emit(FE::get(CLIENTMSG) << _("Disconnected."), FE::ALL, this);
    App->fe->disconnected(this);
}

void ServerConnection::connect()
{
    #ifdef DEBUG
    App->log << "ServerConnection::connect()" << std::endl;
    #endif

    doCleanup();
    App->fe->disconnected(this);

    Session.isConnecting = true;

    FE::emit(FE::get(CONNECTING) << Session.host << Session.port, FE::CURRENT, this);

    _socket.connect(Session.host, Session.port);
}


void ServerConnection::on_error(const char *msg)
{
    FE::emit(FE::get(ERRORMSG) << ustring(_("Failed connecting: ")) + Util::convert_to_utf8(msg), FE::CURRENT, this);
    disconnect();
}

void ServerConnection::on_host_resolved()
{
    FE::emit(FE::get(CLIENTMSG) << _("Resolved host. Connecting.."), FE::CURRENT, this);
}

void ServerConnection::on_connected(Glib::IOCondition cond)
{
    Session.isConnecting = false;
    Session.isConnected = true;
    App->fe->connected(this);

    // The only purpose of this function is to register us to the server
    // when we are able to write
    FE::emit(FE::get(CLIENTMSG) << _("Connected. Logging in..."), FE::CURRENT, this);

    if (cond & Glib::IO_OUT) {
        char hostname[256]; // FIXME: should this be located somewhere else?
        gethostname(hostname, sizeof(hostname) - 1);

        if (!Session.password.empty())
              sendPass(Session.password);

        sendNick(Session.nick);
        sendUser(App->options.ircuser, hostname, Session.host, Session.realname);
    }
}

void ServerConnection::onReadData()
{
    #ifdef DEBUG
    App->log << "Serverconnection::onReadData(): reading.." << std::endl;
    #endif

    try {

        char buf[4096];
        int received = 0;
        if (_socket.receive(buf, 4095, received)) {

            for (int i = 0; i < received; ++i) {
                if (buf[i] == '\r') {
                    // Skip.
                } else if (buf[i] == '\n') {
                    // Send line to parser
                    _tmpbuf[_bufpos] = '\0';
                    std::string str(_tmpbuf, _bufpos);
                    Glib::ustring str_utf8 = Util::convert_to_utf8(str);
                    _parser.parseLine(str_utf8);
                    _bufpos = 0;
                } else {
                    if (_bufpos < 520) {
                        _tmpbuf[_bufpos] = buf[i];
                        // Ignore line if it's too long.
                        _bufpos++;
                    }
                }
            }
        }

    } catch (SocketException &e) {
        FE::emit(FE::get(ERRORMSG) << ustring(_("Failed to receive: ")) + Util::convert_to_utf8(e.what()), FE::ALL, this);
        disconnect();
        addReconnectTimer();
    } catch (SocketDisconnected &e) {
        disconnect();
        addReconnectTimer();
    }
}


bool ServerConnection::autoReconnect()
{
    #ifdef DEBUG
    App->log << "ServerConnection::autoReconnect(): reconnecting." << std::endl;
    #endif
    connect();
    return false;
}

bool ServerConnection::connectionCheck()
{
    if (Session.sentLagCheck) {
        // disconnected! last lag check was never replied to
        #ifdef DEBUG
        App->log << "ServerConnection::connectionCheck(): disconnected." << std::endl;
        #endif
        disconnect();
        connect();

        return false;
    } else {
        #ifdef DEBUG
        App->log << "ServerConnection::connectionCheck(): still on" << std::endl;
        #endif
        sendPing();
        Session.sentLagCheck = true;
        return true;
    }
}

void ServerConnection::addConnectionTimerCheck()
{
    signal_connection = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &ServerConnection::connectionCheck),
            30000);
}

void ServerConnection::addReconnectTimer()
{
    if (!signal_autoreconnect.connected())
          signal_autoreconnect = Glib::signal_timeout().connect(
                  sigc::mem_fun(*this, &ServerConnection::autoReconnect),
                  2000);
}

void ServerConnection::removeReconnectTimer()
{
    if (signal_autoreconnect.connected())
          signal_autoreconnect.disconnect();
}


bool ServerConnection::sendPong(const ustring& crap)
{
    ustring msg("PONG :" + crap + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendPing(const ustring& crap)
{
    ustring msg("PING LAG" + crap + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendUser(const ustring& nick, const ustring& localhost, const ustring& remotehost, const ustring& name)
{
    ustring realname = (name.empty() ? nick : name);

    ustring msg("USER " + nick + " " + localhost + " " + remotehost + " :" + realname + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendNick(const ustring& nick)
{
    if (Session.isConnected && Session.hasRegistered) {
        ustring msg("NICK " + nick + "\r\n");

        return _socket.send(msg);
    } else if (Session.isConnected && !Session.hasRegistered) {
        Session.nick = nick;
        ustring msg("NICK " + nick + "\r\n");

        return _socket.send(msg);
    } else {
        Session.nick = nick;
        return false;
    }
}

bool ServerConnection::sendPass(const ustring& pass)
{
    ustring msg("PASS " + pass + "\r\n");
    return _socket.send(msg);
}

bool ServerConnection::sendVersion(const ustring& to)
{
    #ifndef WIN32
    ustring s(LostIRCApp::uname_info.sysname);
    ustring r(LostIRCApp::uname_info.release);
    ustring m(LostIRCApp::uname_info.machine);
    #else
    ustring s("Windows");
    ustring r("");
    ustring m("");
    #endif
    ustring vstring("LostIRC " VERSION " on " + s + " " + r + " [" + m + "]");
    ustring msg("NOTICE " + to + " :\001VERSION " + vstring + "\001\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendMsg(const ustring& to, const ustring& message, bool sendToGui)
{
    ustring msg("PRIVMSG " + to + " :" + message + "\r\n");

    if (sendToGui) {
        ChannelBase *chan = findChannel(to);
        if (!chan)
              chan = findQuery(to);

        if (chan)
              FE::emit(FE::get(PRIVMSG_SELF) << Session.nick << message, *chan, this);
        else
              FE::emit(FE::get(PRIVMSG_SELF) << Session.nick << message, FE::CURRENT, this);
    }

    return _socket.send(msg);
}

bool ServerConnection::sendNotice(const ustring& to, const ustring& message)
{
    ustring msg("NOTICE " + to + " :" + message + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendJoin(const ustring& chan)
{
    ustring msg("JOIN " + chan + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendPart(const ustring& chan, const ustring& message)
{
    ustring msg;
    if (!message.empty())
          msg = "PART " + chan + " :" + message + "\r\n";
    else
          msg = "PART " + chan + "\r\n";

    return _socket.send(msg);
}

bool ServerConnection::sendKick(const ustring& chan, const ustring& nick, const ustring& kickmsg)
{
    ustring msg("KICK " + chan + " " + nick + " :" + kickmsg + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendWhois(const ustring& params)
{
    ustring msg("WHOIS " + params + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendList(const ustring& params)
{
    ustring msg("LIST " + params + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendQuit(const ustring& quitmsg)
{
    if (Session.isConnected) {
        ustring msg;
        if (quitmsg.size() < 1) {
            msg = "QUIT\r\n";
        } else {
            msg = "QUIT :" + quitmsg + "\r\n";
        }

        return _socket.send(msg);
    }
    return true;
}

bool ServerConnection::sendMode(const ustring& params)
{
    ustring msg("MODE " + params + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendCtcp(const ustring& to, const ustring& params)
{
    ustring msg("PRIVMSG " + to + " :\001" + params + "\001\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendCtcpNotice(const ustring& to, const ustring& params)
{
    ustring msg("NOTICE " + to + " :\001" + params + "\001\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendAway(const ustring& params)
{
    ustring msg("AWAY :" + params + "\r\n");
    Session.awaymsg = params;

    return _socket.send(msg);
}

bool ServerConnection::sendAdmin(const ustring& params)
{
    ustring msg("ADMIN " + params + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendWhowas(const ustring& params)
{
    ustring msg("WHOWAS " + params + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendInvite(const ustring& to, const ustring& params)
{
    ustring msg("INVITE " + to + " " + params + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendTopic(const ustring& chan, const ustring& topic)
{
    ustring msg;
    if (topic.empty())
          msg = "TOPIC " + chan + "\r\n";
    else
          msg = "TOPIC " + chan + " :" + topic + "\r\n";

    return _socket.send(msg);
}

bool ServerConnection::sendBanlist(const ustring& chan)
{
    ustring msg("MODE " + chan + " +b\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendMe(const ustring& to, const ustring& message)
{
    ustring msg("PRIVMSG " + to + " :\001ACTION " + message + "\001\r\n");

    ChannelBase *chan = findChannel(to);
    if (!chan)
          chan = findQuery(to);

    if (chan)
          FE::emit(FE::get(ACTION) << Session.nick << message, *chan, this);
    else
          FE::emit(FE::get(ACTION) << Session.nick << message, FE::CURRENT, this);

    return _socket.send(msg);
}

bool ServerConnection::sendWho(const ustring& mask)
{
    ustring msg("WHO " + mask + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendNames(const ustring& chan)
{
    ustring msg("NAMES " + chan + "\r\n");

    return _socket.send(msg);
}

bool ServerConnection::sendOper(const ustring& login, const ustring& password)
{
    ustring msg("OPER " + login + ' ' + password + "\r\n" );

    return _socket.send(msg);
}

bool ServerConnection::sendWallops(const ustring& message)
{
    ustring msg("WALLOPS :" + message + "\r\n" );

    return _socket.send(msg);
}

bool ServerConnection::sendKill(const ustring& nick, const ustring& reason)
{
    ustring msg("KILL " + nick + " :" + reason + "\r\n" );

    return _socket.send(msg);
}

bool ServerConnection::sendRaw(const ustring& text)
{
    ustring msg(text + "\r\n");

    return _socket.send(msg);
}

void ServerConnection::addChannel(const ustring& n)
{
    Channel *c = new Channel(n);
    Session.channels.push_back(c);
}

void ServerConnection::addQuery(const ustring& n)
{
    Query *c = new Query(n);
    Session.channels.push_back(c);
}

bool ServerConnection::removeChannel(const ustring& n)
{
    vector<ChannelBase*>::iterator i = Session.channels.begin();

    ustring chan = Util::lower(n);
    for (;i != Session.channels.end(); ++i) {
        ustring name = Util::lower((*i)->getName());
        if (name == chan) {
            Session.channels.erase(i);
            return true;
        }
    }
    return false;
}


vector<ChannelBase*> ServerConnection::findUser(const ustring& n)
{
    vector<ChannelBase*> chans;
    vector<ChannelBase*>::iterator i = Session.channels.begin();

    for (;i != Session.channels.end(); ++i) {
        if ((*i)->findUser(n)) {
            chans.push_back(*i);
        }
    }
    return chans;
}

Channel* ServerConnection::findChannel(const ustring& c)
{
    ustring chan = Util::lower(c);
    vector<ChannelBase*>::iterator i = Session.channels.begin();
    for (;i != Session.channels.end(); ++i) {
        ustring name = Util::lower((*i)->getName());
        if (name == chan)
              return dynamic_cast<Channel*>(*i);
    }
    return 0;
}

Query* ServerConnection::findQuery(const ustring& c)
{
    ustring chan = Util::lower(c);
    vector<ChannelBase*>::iterator i = Session.channels.begin();
    for (;i != Session.channels.end(); ++i) {
        ustring name = Util::lower((*i)->getName());
        if (name == chan)
              return dynamic_cast<Query*>(*i);
    }
    return 0;
}

void ServerConnection::sendCmds()
{
    vector<ustring>::iterator i;
    for (i = Session.cmds.begin(); i != Session.cmds.end(); ++i) {
        if (i->empty())
              continue;
        if (i->at(0) == '/') {

            ustring::size_type pos = i->find_first_of(" ");

            ustring params;
            if (pos != ustring::npos) {
                params = i->substr(pos + 1);
            }

            try {

                Commands::send(this, Util::upper(i->substr(1, pos - 1)), params);

            } catch (CommandException& ce) {

                FE::emit(FE::get(CLIENTMSG) << ce.what(), FE::CURRENT, this);
            }
        }
    }
}
