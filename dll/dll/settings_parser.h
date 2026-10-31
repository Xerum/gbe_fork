/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef SETTINGS_PARSER_INCLUDE_H
#define SETTINGS_PARSER_INCLUDE_H

#include "settings.h"

//returns game appid
uint32 create_localstorage_settings(Settings **settings_client_out, Settings **settings_server_out, Local_Storage **local_storage_out);
void save_global_settings(class Local_Storage *local_storage, const char *name, const char *language);

bool settings_disable_lan_only();

// custom: [main::dll_redirect], lowercase dll file name -> absolute path of its replacement
const std::map<std::wstring, std::wstring>& settings_dll_redirects();

// custom: [main::host_redirect], lowercase host name -> address it should resolve to
// keys may also be "*.example.com" (the domain and all sub domains) or "*" (every other dotted host name, no ip literals)
const std::map<std::string, std::string>& settings_host_redirects();

// custom: [main::host_redirect_log] path, file that gets one line per redirected host name (empty = off)
const std::string& settings_host_redirect_log();

// custom: [main::winhttp] ignore_cert_errors, WinHTTP requests of the game accept any server certificate
bool settings_winhttp_ignore_cert_errors();

// custom: [main::playfab] url, replaces https://<titleid>.playfabapi.com in Unity games (empty = off)
const std::wstring& settings_playfab_url();

enum class SettingsItf {
   CLIENT,
   GAMESERVER_STATS,
   GAMESERVER,
   MATCHMAKING_SERVERS,
   MATCHMAKING,
   USER,
   FRIENDS,
   UTILS,
   USER_STATS,
   APPS,
   NETWORKING,
   REMOTE_STORAGE,
   SCREENSHOTS,
   HTTP,
   UNIFIED_MESSAGES,
   CONTROLLER,
   UGC,
   APPLIST,
   MUSIC,
   MUSIC_REMOTE,
   HTML_SURFACE,
   INVENTORY,
   VIDEO,
   MASTERSERVER_UPDATER,
};

const std::map<SettingsItf, std::string>& settings_old_interfaces();

#endif // SETTINGS_PARSER_INCLUDE_H
