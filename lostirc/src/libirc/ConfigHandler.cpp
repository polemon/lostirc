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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include "ConfigHandler.h"
#include "LostIRCApp.h"

using Glib::ustring;
using std::cout;
using std::vector;
using std::map;

Servers::Servers(const char *file)
{
    filename = App->home;
    filename += file;
    readServersFile();
}

Servers::~Servers()
{
    vector<Server*>::iterator i;

    for (i = _servers.begin(); i != _servers.end();) {
        delete (*i);
        i = _servers.erase(i);
    }
}

bool Servers::readServersFile()
{
    std::ifstream in(filename.c_str());

    if (!in)
          return false;

    vector<ustring> tmpcmds;
    ustring server, nick, password;
    std::string tmp;
    int port = 0;
    int auto_connect = 1;
    while (getline(in, tmp)) {
        ustring::size_type pos1 = tmp.find_first_of("=");
        ustring param;
        ustring value;

        if (pos1 != ustring::npos) {
            param = tmp.substr(0, pos1 - 1);
            try {
                value = tmp.substr(pos1 + 2);
            } catch (std::out_of_range) {
                value = tmp.substr(pos1 + 1);
            }

            if (param == "hostname") {
                if (!server.empty()) {
                    Server *j = new Server;
                    j->hostname = server;
                    j->port = port ? port : 6667;
                    j->password = password;
                    j->cmds = tmpcmds;
                    j->nick = nick;
                    j->auto_connect = auto_connect;

                    port = 0;
                    auto_connect = 1;
                    password.erase();
                    nick.erase();

                    _servers.push_back(j);
                    tmpcmds.clear();
                } 
                server = value;
            } else if (param == "cmd") {
                tmpcmds.push_back(value);
            } else if (param == "port") {
                port = Util::convert<int>(value);
            } else if (param == "password") {
                password = value;
            } else if (param == "nick") {
                nick = value;
            } else if (param == "auto_connect") {
                auto_connect = Util::convert<int>(value);
            }
        }
    }
    if (!server.empty()) {
        Server *j = new Server;
        j->hostname = server;
        j->port = port ? port : 6667;
        j->password = password;
        j->cmds = tmpcmds;
        j->nick = nick;
        j->auto_connect = auto_connect;

        _servers.push_back(j);
    } 
    return true;
}

bool Servers::writeServersFile()
{
    std::ofstream out(filename.c_str());

    if (!out)
          return false;

    vector<Server*>::const_iterator i;
    vector<ustring>::const_iterator ivec;

    for (i = _servers.begin(); i != _servers.end(); ++i) {
        out << "hostname = " << (*i)->hostname.raw() << std::endl;
        out << "port = " << (*i)->port << std::endl;
        out << "password = " << (*i)->password.raw() << std::endl;
        out << "nick = " << (*i)->nick.raw() << std::endl;
        out << "auto_connect = " << static_cast<int>((*i)->auto_connect) << std::endl;

        for (ivec = (*i)->cmds.begin(); ivec != (*i)->cmds.end(); ++ivec)
              out << "cmd = " << (*ivec).raw() << std::endl;

        out << std::endl;
    }
    return true;
}

bool Servers::hasAutoConnects()
{
    vector<Server*>::const_iterator i;
    for (i = _servers.begin(); i != _servers.end(); ++i)
          if ((*i)->auto_connect)
                return true;

    return false;
}

void Servers::removeServer(Server *a)
{
    vector<Server*>::iterator i = std::find(_servers.begin(), _servers.end(), a);
    _servers.erase(i);
    delete a;
}

baseConfig::baseConfig(const char *file)
{
    filename = App->home;
    filename += file;
}

void baseConfig::set(const ustring& key, const ustring& value)
{
    if (configvalues.find(key) != configvalues.end())
          *(configvalues[key]) = value;
    else
        std::cerr << _("Not found key, `") << key << "'" << std::endl;
}

Glib::ustring baseConfig::get(const ustring& key)
{
    map<ustring, baseConfigValue*>::const_iterator i = configvalues.find(key);

    if (i != configvalues.end())
          return i->second->getString();

    std::cerr << _("Not found key, `") << key << "'" << std::endl;
    return "";
}

bool baseConfig::readConfigFile()
{
    std::ifstream in(filename.c_str());

    if (!in)
          return false;

    std::string str;
    while (getline(in, str)) {
        ustring::size_type pos = str.find(" = ");
        ustring param = str.substr(0, pos);
        ustring value = str.substr(pos + 3);

        if (configvalues.find(param.c_str()) != configvalues.end())
              *(configvalues[param.c_str()]) = value;
    }
    return true;
}

bool baseConfig::writeConfigFile()
{
    #ifdef DEBUG
    App->log << "Trying to write configfile: `" << filename << "'" << std::endl;
    #endif
    std::ofstream out(filename.c_str());

    if (!out)
          return false;

    #ifdef DEBUG
    App->log << "\twriting..." << std::endl;
    #endif
    map<ustring, baseConfigValue*>::const_iterator i;

    for (i = configvalues.begin(); i != configvalues.end(); ++i)
    {
        out << i->first.raw() << " = " << i->second->getString().raw() << std::endl;
    }
    return true;

}


Options::Options(const char *filename)
    : baseConfig(filename),
    nickcompletion_char(this, "nickcompletion_character", ','),
    buffer_size(this, "buffer_size", 500),
    window_width(this, "window_width", 0),
    window_height(this, "window_height", 0),
    window_x(this, "window_x", 0),
    window_y(this, "window_y", 0),
    realname(this, "realname"),
    nick(this, "nick"),
    ircuser(this, "ircuser"),
    dccip(this, "dccip"),
    dccport(this, "dccport", 0),
    highlight_words(this, "highlight_words"),
    font(this, "font", "Sans 10"),
    strip_colors(this, "strip_colors", true),
    strip_boldandunderline(this, "strip_boldandunderline", false),
    logging(this, "logging", false),
    encoding(this, "encoding", "System"),
    hidenicklist(this, "hidenicklist", false),
    hidemenu(this, "hidemenu", false),
    hidestatusbar(this, "hidestatusbar", false),
    colorscheme(this, "colorscheme", 1)
{
    readConfigFile();
    writeConfigFile();
}

Colors1::Colors1(const char *filename)
    : baseConfig(filename),
    bgcolor(this, "bgcolor", "#000000"),
    color0(this, "color0", "#FFFFFF"),
    color1(this, "color1", "#000000"),
    color2(this, "color2", "#0000CC"),
    color3(this, "color3", "#00CC00"),
    color4(this, "color4", "#DD0000"),
    color5(this, "color5", "#AA0000"),
    color6(this, "color6", "#BB00BB"),
    color7(this, "color7", "#FFAA00"),
    color8(this, "color8", "#EEDD22"),
    color9(this, "color9", "#33DE55"),
    color10(this, "color10", "#00CCCC"),
    color11(this, "color11", "#33DDEE"),
    color12(this, "color12", "#0000FF"),
    color13(this, "color13", "#EE22EE"),
    color14(this, "color14", "#777777"),
    color15(this, "color15", "#999999"),
    color16(this, "color16", "#BEBEBE"),
    color17(this, "color17", "#000000"),
    color18(this, "color18", "#FFFFFF"),
    color19(this, "color19", "#000000")

{
    readConfigFile();
    writeConfigFile();
}

Colors2::Colors2(const char *filename)
    : baseConfig(filename),
    bgcolor(this, "bgcolor", "#FFFFFF"),
    color0(this, "color0", "#000000"),
    color1(this, "color1", "#FFFFFF"),
    color2(this, "color2", "#0000CC"),
    color3(this, "color3", "#00CC00"),
    color4(this, "color4", "#DD0000"),
    color5(this, "color5", "#AA0000"),
    color6(this, "color6", "#BB00BB"),
    color7(this, "color7", "#FFAA00"),
    color8(this, "color8", "#EEDD22"),
    color9(this, "color9", "#33DE55"),
    color10(this, "color10", "#00CCCC"),
    color11(this, "color11", "#33DDEE"),
    color12(this, "color12", "#0000FF"),
    color13(this, "color13", "#EE22EE"),
    color14(this, "color14", "#777777"),
    color15(this, "color15", "#999999"),
    color16(this, "color16", "#BEBEBE"),
    color17(this, "color17", "#000000"),
    color18(this, "color18", "#FFFFFF"),
    color19(this, "color19", "#000000")

{
    readConfigFile();
    writeConfigFile();
}

Events::Events(const char *filename)
    : baseConfig(filename),
    privmsg(this, "privmsg", "$12<$00%1$12>$00 %2"),
    privmsg_highlight(this, "privmsg_highlight", "$02<$08%1$02>$00 %2"),
    privmsg_self(this, "privmsg_self", "$07<$00%1$07>$00 %2"),
    action(this, "action", "$07* %1$00 %2"),
    action_highlight(this, "action_highlight", "$08* %1$00 %2"),
    dcc_receive(this, "dcc_receive", "$15-- $16DCC SEND from $00%1$16 (%2), use /DCC RECEIVE %3 to accept the file transfer."),
    servermsg1(this, "servermsg1", "$15-- :$00 %1 %2"),
    servermsg2(this, "servermsg2", "$15-- $00%1:$16 %2"),
    clientmsg(this, "clientmsg", "$15-- $16%1 %2"),
    ctcp(this, "ctcp", "$15-- $16CTCP $07%1 $16received from $00%2"),
    ctcp_multi(this, "ctcp_multi", "$15-- $16CTCP $07%1 $16received from $00%2$16 $15($16to �%3�$15)"),
    ctcp_reply(this, "ctcp_reply", "$15-- $16CTCP $07%1 $16reply from $00%2: %3"),
    topicchange(this, "topicchange", "$15-- $00%1$16 changes topic to:$00 %2"),
    topicis(this, "topicis", "$15-- $16Topic for �%1� is:$00 %2"),
    topictime(this, "topictime", "$15-- $16Set by $00%1$16 on $00%2"),
    noticepriv(this, "noticepriv", "$15-- $07NOTICE $00%1$07: %2"),
    noticepubl(this, "noticepubl", "$15-- $07NOTICE $00%1$07 (to �%2�$07): %3"),
    error(this, "error", "$15-- $04Error:$00 %1"),
    away(this, "away", "$15-- $16User $00%1$00: is away $15($09%2$15)"),
    banlist(this, "banlist", "$15-- $16Ban: $03%1$16 set by: $00%2"),
    unknown(this, "unknown", "$15-- $16Unknown message: $02%1"),
    join(this, "join", "$15-- $00%1 $15($03%3$15)$16 has joined �%2"),
    part(this, "part", "$15-- $00%1 $15($03%3$15)$16 has parted �%2� $15($09%4$15)"),
    part2(this, "part2", "$15-- $00%1 $15($03%3$15)$16 has parted �%2"),
    quit(this, "quit", "$15-- $00%1$16 has quit $15($09%2$15)"),
    quit2(this, "quit2", "$15-- $00%1$16 has quit"),
    nick(this, "nick", "$15-- $00%1$16 changes nick to $00%2"),
    mode(this, "mode", "$15-- $00%1$16 sets mode $07%2 %3"),
    cmode(this, "cmode", "$15-- $00%1$16 sets channel mode$07 %2$16 on �%3"),
    wallops(this, "wallops", "$08WALLOPS/<$00%1$8>$07 %2"),
    kicked(this, "kicked", "$15-- $00%1$16 was kicked from �%2� by $00%3 $15($09%4$15)"),
    opped(this, "opped", "$15-- $00%1$16 gives channel operator status to $00%2"),
    deopped(this, "deopped", "$15-- $00%1$16 removes channel operator status from $00%2"),
    admined(this, "admined", "$15-- $00%1$16 gives channel admin status to $00%2"),
    deadmined(this ,"deadmined", "$15-- $00%1$16 removes channel admin status from $00%2"),
    ownered(this, "ownered", "$15-- $00%1$16 gives channel owner status to $00%2"),
    deownered(this, "deownered", "$15-- $00%1$16 removes channel owner status from $00%2"),
    voiced(this, "voiced", "$15-- $00%1$16 gives voice to $00%2"),
    devoiced(this, "devoiced", "$15-- $00%1$16 removes voice from $00%2"),
    halfopped(this, "halfopped", "$15-- $00%1$16 gives channel half-operator status to $00%2"),
    halfdeopped(this, "halfdeopped", "$15-- $00%1$16 removes channel half-operator status from $00%2"),
    banned(this, "banned", "$15-- $00%1$16 sets ban on $03%2"),
    unbanned(this, "unbanned", "$15-- $00%1$16 unbans $03%2"),
    invited(this, "invited", "$15-- $00%1$16 invites you to join �%2"),
    connecting(this, "connecting", "$15-- $16Connecting to �$11%1�$16 on port �$11%2�$16..."),
    names(this, "names", "$15-- $16Names �%1�$00:$16 %2"),
    killed(this, "killed", "$15-- $16You were killed by $00%1$16 $15($09%2$15)"),
    whois_user(this, "whois_user", "$15-- $16User $00%1$00: $03%2@%3$00 - %4"),
    whois_channels(this, "whois_channels", "$15-- $16User $00%1$00: on channels �%2"),
    whois_server(this, "whois_server", "$15-- $16User $00%1$00: on server %2 (%3)"),
    whois_generic(this, "whois_generic", "$15-- $16User $00%1$00: %2")
{
    map<ustring, baseConfigValue*>::iterator i = configvalues.begin();

    for (; i != configvalues.end(); ++i) {
        std::string msg = i->second->getString();
        std::replace(msg.begin(), msg.end(), '$', '\003');
        std::replace(msg.begin(), msg.end(), '�', '\002');
        *(i->second) = msg;
    }

    readConfigFile();
    writeConfigFile();
}
