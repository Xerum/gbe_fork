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

#include "dll/base.h"
#include "dll/settings_parser.h"

#ifndef EMU_RELEASE_BUILD
#include "dbg_log/dbg_log.hpp"
#endif


std::recursive_mutex global_mutex{};
// some arbitrary counter/time for reference
extern const std::chrono::time_point<std::chrono::high_resolution_clock> startup_counter = std::chrono::high_resolution_clock::now();
extern const std::chrono::time_point<std::chrono::system_clock> startup_time = std::chrono::system_clock::now();

#ifndef EMU_RELEASE_BUILD
dbg_log dbg_logger(get_full_program_path() + "STEAM_LOG_" + std::to_string(common_helpers::rand_number(UINT32_MAX)) + ".log");
#endif


#ifdef __WINDOWS__

void randombytes(char *buf, size_t size)
{
    // NT_SUCCESS is: return value >= 0, including Ntdef.h causes so many errors
    while (BCryptGenRandom(NULL, (PUCHAR) buf, (ULONG) size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        PRINT_DEBUG("ERROR");
        Sleep(100);
    }
    
}

std::string get_env_variable(const std::string &name)
{
    wchar_t env_variable[1024]{};
    DWORD ret = GetEnvironmentVariableW(utf8_decode(name).c_str(), env_variable, _countof(env_variable));
    if (ret <= 0 || !env_variable[0]) {
        return std::string();
    }

    env_variable[ret] = 0;
    return utf8_encode(env_variable);
}

bool set_env_variable(const std::string &name, const std::string &value)
{
    return SetEnvironmentVariableW(utf8_decode(name).c_str(), utf8_decode(value).c_str());
}

#else

static int fd = -1;

void randombytes(char *buf, size_t size)
{
  int i{};

  if (fd == -1) {
    for (;;) {
      fd = open("/dev/urandom",O_RDONLY);
      if (fd != -1) break;
      sleep(1);
    }
  }

  while (size > 0) {
    if (size < 1048576) i = size; else i = 1048576;

    i = read(fd,buf,i);
    if (i < 1) {
      sleep(1);
      continue;
    }

    buf += i;
    size -= i;
  }
}

std::string get_env_variable(const std::string &name)
{
    char *env = getenv(name.c_str());
    if (!env) {
        return std::string();
    }

    return std::string(env);
}

bool set_env_variable(const std::string &name, const std::string &value)
{
    return setenv(name.c_str(), value.c_str(), 1) == 0;
}

#endif


unsigned generate_account_id()
{
    int a = 0;
    randombytes((char *)&a, sizeof(a));
    a = abs(a);
    if (!a) ++a;
    return a;
}

CSteamID generate_steam_anon_user()
{
    return CSteamID(generate_account_id(), k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeAnonUser);
}

SteamAPICall_t generate_steam_api_call_id() {
    static SteamAPICall_t a;
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    randombytes((char *)&a, sizeof(a));
    ++a;
    if (a == 0) ++a;
    return a;
}

CSteamID generate_steam_id_user()
{
    return CSteamID(generate_account_id(), k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeIndividual);
}

CSteamID generate_steam_id_server()
{
    return CSteamID(generate_account_id(), k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeGameServer);
}

CSteamID generate_steam_id_anonserver()
{
    return CSteamID(generate_account_id(), k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeAnonGameServer);
}

CSteamID generate_steam_id_lobby()
{
    return CSteamID(generate_account_id(), k_EChatInstanceFlagLobby | k_EChatInstanceFlagMMSLobby, k_EUniversePublic, k_EAccountTypeChat);
}

bool check_timedout(std::chrono::high_resolution_clock::time_point old, double timeout, std::chrono::high_resolution_clock::time_point now) // in seconds
{
    if (timeout == 0.0) return true;

    if (std::chrono::duration_cast<std::chrono::duration<double>>(now - old).count() > timeout) {
        return true;
    }

    return false;
}

std::string get_full_exe_path()
{
    // https://github.com/gpakosz/whereami/blob/master/src/whereami.c
    // https://stackoverflow.com/q/1023306

    static std::string exe_path{};
    static std::recursive_mutex mtx{};

    if (!exe_path.empty()) {
        return exe_path;
    }

    std::lock_guard lock(mtx);
    // check again in case we didn't win this thread arbitration
    if (!exe_path.empty()) {
        return exe_path;
    }

#if defined(__WINDOWS__)
    static wchar_t path[8192]{};
    auto ret = ::GetModuleFileNameW(nullptr, path, _countof(path));
    if (ret >= _countof(path) || 0 == ret) {
        path[0] = '.';
        path[1] = 0;
    }
    exe_path = canonical_path(utf8_encode(path));
#else
    // https://man7.org/linux/man-pages/man5/proc.5.html
    // https://linux.die.net/man/5/proc
    // https://man7.org/linux/man-pages/man2/readlink.2.html
    // https://linux.die.net/man/3/readlink
    static char path[8192]{};
    auto read = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (-1 == read) {
        path[0] = '.';
        read = 1;
    }
    path[read] = 0;
    exe_path = canonical_path(path);
#endif // __WINDOWS__

    return exe_path;
}

std::string get_exe_dirname()
{
    std::string env_exe_dir = get_env_variable("GseExeDir");
    if (!env_exe_dir.empty()) {
        if (env_exe_dir.back() != PATH_SEPARATOR[0]) {
            env_exe_dir = env_exe_dir.append(PATH_SEPARATOR);
        }

        return env_exe_dir;
    }

    std::string full_exe_path = get_full_exe_path();
    return full_exe_path.substr(0, full_exe_path.rfind(PATH_SEPARATOR)).append(PATH_SEPARATOR);

}

#ifdef __LINUX__
std::string get_lib_path()
{
    Dl_info info;
    if (dladdr((void*)get_lib_path, &info))
    {
        return std::string(info.dli_fname);
    }
    return ".";
}
#endif

std::string get_full_lib_path()
{
    std::string program_path;
#if defined(__WINDOWS__)
    wchar_t   DllPath[2048] = {0};
    GetModuleFileNameW((HINSTANCE)&__ImageBase, DllPath, _countof(DllPath));
    program_path = utf8_encode(DllPath);
#else
    program_path = get_lib_path();
#endif
    return program_path;
}

std::string get_full_program_path()
{
    std::string env_program_path = get_env_variable("GseAppPath");
    if (env_program_path.length()) {
        if (env_program_path.back() != PATH_SEPARATOR[0]) {
            env_program_path = env_program_path.append(PATH_SEPARATOR);
        }

        return env_program_path;
    }

    std::string program_path{};
    program_path = get_full_lib_path();
    return program_path.substr(0, program_path.rfind(PATH_SEPARATOR)).append(PATH_SEPARATOR);
}

std::string get_current_path()
{
    std::string path;
#if defined(STEAM_WIN32)
    char *buffer = _getcwd( NULL, 0 );
#else
    char *buffer = get_current_dir_name();
#endif
    if (buffer) {
        path = buffer;
        path.append(PATH_SEPARATOR);
        free(buffer);
    }

    return path;
}

std::string canonical_path(const std::string &path)
{
    std::string output;
#if defined(STEAM_WIN32)
    wchar_t *buffer = _wfullpath(NULL, utf8_decode(path).c_str(), 0);
    if (buffer) {
        output = utf8_encode(buffer);
        free(buffer);
    }
#else
    char *buffer = canonicalize_file_name(path.c_str());
    if (buffer) {
        output = buffer;
        free(buffer);
    }
#endif

    return output;
}

bool file_exists_(const std::string &full_path)
{
#if defined(STEAM_WIN32)
    struct _stat buffer{};
    if (_wstat(utf8_decode(full_path).c_str(), &buffer) != 0)
        return false;

    if ( buffer.st_mode & S_IFDIR)
        return false;
#else
    struct stat buffer{};
    if (stat(full_path.c_str(), &buffer) != 0)
        return false;

    if (S_ISDIR(buffer.st_mode))
        return false;
#endif

    return true;
}

unsigned int file_size_(const std::string &full_path)
{
#if defined(STEAM_WIN32)
    struct _stat buffer{};
    if (_wstat(utf8_decode(full_path).c_str(), &buffer) != 0) return 0;
#else
    struct stat buffer{};
    if (stat (full_path.c_str(), &buffer) != 0) return 0;
#endif
    return buffer.st_size;
}


#ifdef EMU_EXPERIMENTAL_BUILD

static std::vector<void*> loaded_libs{};

static void load_dlls()
{
    constexpr static const char LIB_EXTENSION[] =
#ifdef __WINDOWS__
        ".dll"
#else
        ".so"
#endif
        ;

    std::string path(Local_Storage::get_game_settings_path() + "load_dlls" + PATH_SEPARATOR);

    std::vector<std::string> paths(Local_Storage::get_filenames_path(path));
    for (auto & p: paths) {
        std::string full_path(path + p);
        if (!common_helpers::ends_with_i(full_path, LIB_EXTENSION)) continue;

        PRINT_DEBUG("loading '%s'", full_path.c_str());
        auto lib_handle =
#ifdef __WINDOWS__
            LoadLibraryW(utf8_decode(full_path).c_str());
#else
            dlopen(full_path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

        if (lib_handle != nullptr) {
            loaded_libs.push_back(reinterpret_cast<void *>(lib_handle));
            PRINT_DEBUG(" LOADED");
        } else {
#ifdef __WINDOWS__
            PRINT_DEBUG(" FAILED, error code 0x%X", GetLastError());
#else
            PRINT_DEBUG(" FAILED, error string '%s'", dlerror());
#endif
        }
    }
}

static void unload_dlls()
{
    for (auto lib_handle : loaded_libs) {
#ifdef __WINDOWS__
        FreeLibrary(reinterpret_cast<HMODULE>(lib_handle));
#else
        dlclose(lib_handle);
#endif
    }
}

#ifdef __WINDOWS__

struct ips_test {
    uint32_t ip_from;
    uint32_t ip_to;
};

static std::vector<struct ips_test> whitelist_ips;

void set_whitelist_ips(uint32_t *from, uint32_t *to, unsigned num_ips)
{
    whitelist_ips.clear();
    for (unsigned i = 0; i < num_ips; ++i) {
        struct ips_test ip_a;
        PRINT_DEBUG("from: %hhu.%hhu.%hhu.%hhu", ((unsigned char *)&from[i])[0], ((unsigned char *)&from[i])[1], ((unsigned char *)&from[i])[2], ((unsigned char *)&from[i])[3]);
        PRINT_DEBUG("to: %hhu.%hhu.%hhu.%hhu", ((unsigned char *)&to[i])[0], ((unsigned char *)&to[i])[1], ((unsigned char *)&to[i])[2], ((unsigned char *)&to[i])[3]);
        ip_a.ip_from = ntohl(from[i]);
        ip_a.ip_to = ntohl(to[i]);
        if (ip_a.ip_to < ip_a.ip_from) continue;
        if ((ip_a.ip_to - ip_a.ip_from) > (1 << 25)) continue;
        PRINT_DEBUG("added ip to whitelist");
        whitelist_ips.push_back(ip_a);
    }
}

static bool is_whitelist_ip(unsigned char *ip)
{
    uint32_t ip_temp = 0;
    memcpy(&ip_temp, ip, sizeof(ip_temp));
    ip_temp = ntohl(ip_temp);

    for (auto &i : whitelist_ips) {
        if (i.ip_from <= ip_temp && ip_temp <= i.ip_to) {
            PRINT_DEBUG("IP IS WHITELISTED %hhu.%hhu.%hhu.%hhu", ip[0], ip[1], ip[2], ip[3]);
            return true;
        }
    }

    return false;
}

static bool is_lan_ipv4(unsigned char *ip)
{
    PRINT_DEBUG("CHECK LAN IP %hhu.%hhu.%hhu.%hhu", ip[0], ip[1], ip[2], ip[3]);
    if (is_whitelist_ip(ip)) return true;
    if (ip[0] == 127) return true;
    if (ip[0] == 10) return true;
    if (ip[0] == 192 && ip[1] == 168) return true;
    if (ip[0] == 169 && ip[1] == 254 && ip[2] != 0) return true;
    if (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) return true;
    if ((ip[0] == 100) && ((ip[1] & 0xC0) == 0x40)) return true;
    if (ip[0] == 239) return true; //multicast
    if (ip[0] == 0) return true; //Current network
    if (ip[0] == 192 && (ip[1] == 18 || ip[1] == 19)) return true; //Used for benchmark testing of inter-network communications between two separate subnets.
    if (ip[0] >= 224) return true; //ip multicast (224 - 239) future use (240.0.0.0 - 255.255.255.254) broadcast (255.255.255.255)
    return false;
}

static bool is_lan_ip(const sockaddr *addr, int namelen)
{
    if (!namelen) return false;

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        unsigned char ip[4];
        memcpy(ip, &addr_in->sin_addr, sizeof(ip));
        if (is_lan_ipv4(ip)) return true;
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
        unsigned char ip[16];
        unsigned char zeroes[16] = {};
        memcpy(ip, &addr_in6->sin6_addr, sizeof(ip));
        PRINT_DEBUG("CHECK LAN IP6 %hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu", ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15]);
        if (((ip[0] == 0xFF) && (ip[1] < 3) && (ip[15] == 1)) ||
        ((ip[0] == 0xFE) && ((ip[1] & 0xC0) == 0x80))) return true;
        if (memcmp(zeroes, ip, sizeof(ip)) == 0) return true;
        if (memcmp(zeroes, ip, sizeof(ip) - 1) == 0 && ip[15] == 1) return true;
        if (ip[0] == 0xff) return true; //multicast
        if (ip[0] == 0xfc) return true; //unique local
        if (ip[0] == 0xfd) return true; //unique local

        unsigned char ipv4_mapped[12] = {};
        ipv4_mapped[10] = 0xFF;
        ipv4_mapped[11] = 0xFF;
        if (memcmp(ipv4_mapped, ip, sizeof(ipv4_mapped)) == 0) {
            if (is_lan_ipv4(ip + 12)) return true;
        }
    }

    PRINT_DEBUG("NOT LAN IP");
    return false;
}

int ( WINAPI *Real_SendTo )( SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen) = sendto;
int ( WINAPI *Real_Connect )( SOCKET s, const sockaddr *addr, int namelen ) = connect;
int ( WINAPI *Real_WSAConnect )( SOCKET s, const sockaddr *addr, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS) = WSAConnect;

static int WINAPI Mine_SendTo( SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen)
{
    PRINT_DEBUG_ENTRY();
    if (is_lan_ip(to, tolen)) {
        return Real_SendTo( s, buf, len, flags, to, tolen );
    } else {
        return len;
    }
}

static int WINAPI Mine_Connect( SOCKET s, const sockaddr *addr, int namelen )
{
    PRINT_DEBUG_ENTRY();
    if (is_lan_ip(addr, namelen)) {
        return Real_Connect(s, addr, namelen);
    } else {
        WSASetLastError(WSAECONNREFUSED);
        return SOCKET_ERROR;
    }
}

static int WINAPI Mine_WSAConnect( SOCKET s, const sockaddr *addr, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS)
{
    PRINT_DEBUG_ENTRY();
    if (is_lan_ip(addr, namelen)) {
        return Real_WSAConnect(s, addr, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS);
    } else {
        WSASetLastError(WSAECONNREFUSED);
        return SOCKET_ERROR;
    }
}

inline bool file_exists (const std::string& name)
{
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

#ifdef DETOURS_64BIT
    #define DLL_NAME "steam_api64.dll"
#else
    #define DLL_NAME "steam_api.dll"
#endif

HMODULE (WINAPI *Real_GetModuleHandleA)(LPCSTR lpModuleName) = GetModuleHandleA;
HMODULE WINAPI Mine_GetModuleHandleA(LPCSTR lpModuleName)
{
    PRINT_DEBUG("%s", lpModuleName);
    if (!lpModuleName) return Real_GetModuleHandleA(lpModuleName);
    std::string in(lpModuleName);
    if (in == std::string(DLL_NAME)) {
        in = std::string("crack") + in;
    }

    return Real_GetModuleHandleA(in.c_str());
}

static void redirect_crackdll()
{
    DetourTransactionBegin();
    DetourUpdateThread( GetCurrentThread() );
    DetourAttach( reinterpret_cast<PVOID*>(&Real_GetModuleHandleA), reinterpret_cast<PVOID>(Mine_GetModuleHandleA) );
    DetourTransactionCommit();
}

static void unredirect_crackdll()
{
    DetourTransactionBegin();
    DetourUpdateThread( GetCurrentThread() );
    DetourDetach( reinterpret_cast<PVOID*>(&Real_GetModuleHandleA), reinterpret_cast<PVOID>(Mine_GetModuleHandleA) );
    DetourTransactionCommit();
}

HMODULE crack_dll_handle{};
static void load_crack_dll()
{
    std::string path(get_full_program_path() + "crack" + DLL_NAME);
    PRINT_DEBUG("searching for crack file '%s'", path.c_str());
    if (file_exists(path)) {
        redirect_crackdll();
        crack_dll_handle = LoadLibraryW(utf8_decode(path).c_str());
        unredirect_crackdll();
        PRINT_DEBUG("Loaded crack file");
    }
}

#include "dll/local_storage.h"

//For some reason when this function is optimized it breaks the shogun 2 prophet (reloaded) crack.
#pragma optimize( "", off )
bool crack_SteamAPI_RestartAppIfNecessary(uint32 unOwnAppID)
{
    if (crack_dll_handle) {
        bool (__stdcall* restart_app)(uint32) = (bool (__stdcall *)(uint32))GetProcAddress(crack_dll_handle, "SteamAPI_RestartAppIfNecessary");
        if (restart_app) {
            PRINT_DEBUG("Calling crack SteamAPI_RestartAppIfNecessary");
            redirect_crackdll();
            bool ret = restart_app(unOwnAppID);
            unredirect_crackdll();
            return ret;
        }
    }

    return false;
}
#pragma optimize( "", on )

bool crack_SteamAPI_Init()
{
    if (crack_dll_handle) {
        bool (__stdcall* init_app)() = (bool (__stdcall *)())GetProcAddress(crack_dll_handle, "SteamAPI_Init");
        if (init_app) {
            PRINT_DEBUG("Calling crack SteamAPI_Init");
            redirect_crackdll();
            bool ret = init_app();
            unredirect_crackdll();
            return ret;
        }
    }

    return false;
}

// custom: resolve host names listed in [main::host_redirect] (see settings_host_redirects()) to the configured address,
// like an entry in the hosts file, but only for this game
// the wide strings are kept for the whole lifetime of the process because GetAddrInfoExW() may resolve asynchronously
static std::map<std::string, std::wstring> host_redirects_w{};

static bool is_ip_literal(const std::string &host)
{
    in_addr ip4{};
    in6_addr ip6{};
    std::string h = host;
    if (h.size() > 2 && h.front() == '[' && h.back() == ']') h = h.substr(1, h.size() - 2);
    return inet_pton(AF_INET, h.c_str(), &ip4) == 1 || inet_pton(AF_INET6, h.c_str(), &ip6) == 1;
}

// exact name first, then the longest matching "*.domain" pattern, then "*"
// "*" only applies to dotted names which are no ip literals, so the own computer name, "localhost" and addresses stay untouched
template<typename T>
static const T* match_host_redirect(const std::map<std::string, T> &redirects, std::string host)
{
    if (redirects.empty() || host.empty()) return nullptr;
    host = common_helpers::to_lower(host);
    while (!host.empty() && host.back() == '.') host.pop_back();

    auto it = redirects.find(host);
    if (it != redirects.end()) return &it->second;
    if (is_ip_literal(host)) return nullptr;

    const T *best = nullptr;
    size_t best_len = 0;
    for (const auto &[pattern, addr] : redirects) {
        if (pattern.size() < 3 || pattern[0] != '*' || pattern[1] != '.') continue;
        std::string domain = pattern.substr(2);  // "example.com"
        std::string suffix = pattern.substr(1);  // ".example.com"
        bool match = host == domain ||
            (host.size() > suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0);
        if (match && pattern.size() > best_len) {
            best = &addr;
            best_len = pattern.size();
        }
    }
    if (best) return best;

    if (host.find('.') == std::string::npos) return nullptr;
    it = redirects.find("*");
    return it == redirects.end() ? nullptr : &it->second;
}

// custom: [main::host_redirect_log], one line per redirected name, to find out which services a game talks to
static void log_host_redirect(const char *func, const std::string &host, const std::string &addr)
{
    const auto &path = settings_host_redirect_log();
    if (path.empty()) return;

    static std::mutex log_mutex{};
    std::lock_guard lock(log_mutex);
    FILE *f = _wfopen(common_helpers::to_wstr(path).c_str(), L"a");
    if (!f) return;
    SYSTEMTIME t{};
    GetLocalTime(&t);
    fprintf(f, "%02u:%02u:%02u.%03u %s %s -> %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, func, host.c_str(), addr.c_str());
    fclose(f);
}

static const std::string* host_redirect(PCSTR host)
{
    const std::string *addr = match_host_redirect(settings_host_redirects(), std::string(host));
    if (addr) log_host_redirect("getaddrinfo", host, *addr);
    return addr;
}

static const std::wstring* host_redirect_w(PCWSTR host, const char *func = "GetAddrInfoW")
{
    const std::wstring *addr = match_host_redirect(host_redirects_w, common_helpers::to_str(host));
    if (addr) log_host_redirect(func, common_helpers::to_str(host), common_helpers::to_str(*addr));
    return addr;
}

// custom: WinHTTP
// - lan only mode: connections to names/addresses outside the lan go to 127.1.33.7 (original behaviour)
// - [main::host_redirect]: WinHTTP connects to the configured address directly (it does not have to resolve the name through
//   the hooked functions) and every request of that connection gets the original "Host" header, so a local replacement
//   still knows which service was meant
// - [main::winhttp] ignore_cert_errors=1: every request accepts any server certificate
// winhttp.dll is often loaded after this dll (ForceInjectSteamClient), the hooks are then placed by Mine_LoadLibraryExW()
HINTERNET (WINAPI *Real_WinHttpConnect)(HINTERNET hSession, LPCWSTR pswzServerName, INTERNET_PORT nServerPort, DWORD dwReserved) = nullptr;
HINTERNET (WINAPI *Real_WinHttpOpenRequest)(HINTERNET hConnect, LPCWSTR pwszVerb, LPCWSTR pwszObjectName, LPCWSTR pwszVersion,
    LPCWSTR pwszReferrer, LPCWSTR *ppwszAcceptTypes, DWORD dwFlags) = nullptr;
static BOOL (WINAPI *WinHttpAddRequestHeaders_fn)(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwModifiers) = nullptr;
static BOOL (WINAPI *WinHttpSetOption_fn)(HINTERNET hInternet, DWORD dwOption, LPVOID lpBuffer, DWORD dwBufferLength) = nullptr;

static std::mutex winhttp_hosts_mutex{};
static std::unordered_map<HINTERNET, std::wstring> winhttp_hosts{};  // connection handle -> value of the "Host" header
static bool winhttp_attached = false;

static bool winhttp_hooks_wanted()
{
    return !settings_disable_lan_only() || !settings_host_redirects().empty() || settings_winhttp_ignore_cert_errors();
}

// only the custom options wait for winhttp.dll to be loaded later, the lan only mode keeps its original behaviour
static bool winhttp_late_hooks_wanted()
{
    return !settings_host_redirects().empty() || settings_winhttp_ignore_cert_errors();
}

HINTERNET WINAPI Mine_WinHttpConnect(HINTERNET hSession, LPCWSTR pswzServerName, INTERNET_PORT nServerPort, DWORD dwReserved)
{
    PRINT_DEBUG("%ls %u", pswzServerName, nServerPort);
    HINTERNET ret = nullptr;
    std::wstring host_header{};

    const std::wstring *addr = pswzServerName ? host_redirect_w(pswzServerName, "WinHttpConnect") : nullptr;
    if (addr) {
        ret = Real_WinHttpConnect(hSession, addr->c_str(), nServerPort, dwReserved);
        host_header = pswzServerName;
        if (nServerPort != INTERNET_DEFAULT_PORT && nServerPort != 80 && nServerPort != 443) {
            host_header += L":" + std::to_wstring(nServerPort);
        }
    } else if (settings_disable_lan_only() || !pswzServerName) {
        ret = Real_WinHttpConnect(hSession, pswzServerName, nServerPort, dwReserved);
    } else {
        struct sockaddr_in ip4{};
        struct sockaddr_in6 ip6{};
        ip4.sin_family = AF_INET;
        ip6.sin6_family = AF_INET6;
        if ((InetPtonW(AF_INET, pswzServerName, &(ip4.sin_addr)) && is_lan_ip((sockaddr *)&ip4, sizeof(ip4))) || (InetPtonW(AF_INET6, pswzServerName, &(ip6.sin6_addr)) && is_lan_ip((sockaddr *)&ip6, sizeof(ip6)))) {
            ret = Real_WinHttpConnect(hSession, pswzServerName, nServerPort, dwReserved);
        } else {
            ret = Real_WinHttpConnect(hSession, L"127.1.33.7", nServerPort, dwReserved);
        }
    }

    if (ret) {
        std::lock_guard lock(winhttp_hosts_mutex);
        if (host_header.size()) winhttp_hosts[ret] = host_header;
        else winhttp_hosts.erase(ret);  // handle values get reused
    }
    return ret;
}

HINTERNET WINAPI Mine_WinHttpOpenRequest(HINTERNET hConnect, LPCWSTR pwszVerb, LPCWSTR pwszObjectName, LPCWSTR pwszVersion,
    LPCWSTR pwszReferrer, LPCWSTR *ppwszAcceptTypes, DWORD dwFlags)
{
    PRINT_DEBUG("%ls %ls %i", pwszVerb, pwszObjectName, dwFlags);
    HINTERNET req = Real_WinHttpOpenRequest(hConnect, pwszVerb, pwszObjectName, pwszVersion, pwszReferrer, ppwszAcceptTypes, dwFlags);
    if (!req) return req;

    std::wstring host_header{};
    {
        std::lock_guard lock(winhttp_hosts_mutex);
        auto it = winhttp_hosts.find(hConnect);
        if (it != winhttp_hosts.end()) host_header = it->second;
    }
    if (host_header.size() && WinHttpAddRequestHeaders_fn) {
        std::wstring header = L"Host: " + host_header;
        WinHttpAddRequestHeaders_fn(req, header.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }
    if (settings_winhttp_ignore_cert_errors() && WinHttpSetOption_fn) {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption_fn(req, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    }
    return req;
}

// returns true if the hooks are in place, must not be called under the loader lock unless winhttp.dll is already loaded
static bool attach_winhttp_hooks()
{
    if (winhttp_attached) return true;
    HMODULE winhttp = GetModuleHandleW(L"winhttp.dll");
    if (!winhttp) return false;

    Real_WinHttpConnect = (decltype(Real_WinHttpConnect))GetProcAddress(winhttp, "WinHttpConnect");
    Real_WinHttpOpenRequest = (decltype(Real_WinHttpOpenRequest))GetProcAddress(winhttp, "WinHttpOpenRequest");
    WinHttpAddRequestHeaders_fn = (decltype(WinHttpAddRequestHeaders_fn))GetProcAddress(winhttp, "WinHttpAddRequestHeaders");
    WinHttpSetOption_fn = (decltype(WinHttpSetOption_fn))GetProcAddress(winhttp, "WinHttpSetOption");
    if (!Real_WinHttpConnect || !Real_WinHttpOpenRequest) return false;

    DetourTransactionBegin();
    DetourUpdateThread( GetCurrentThread() );
    DetourAttach( reinterpret_cast<PVOID*>(&Real_WinHttpConnect), reinterpret_cast<PVOID>(Mine_WinHttpConnect) );
    DetourAttach( reinterpret_cast<PVOID*>(&Real_WinHttpOpenRequest), reinterpret_cast<PVOID>(Mine_WinHttpOpenRequest) );
    winhttp_attached = DetourTransactionCommit() == NO_ERROR;
    PRINT_DEBUG("hooking winhttp, result %i", (int)winhttp_attached);
    return winhttp_attached;
}

static void detach_winhttp_hooks()
{
    if (!winhttp_attached) return;
    DetourTransactionBegin();
    DetourUpdateThread( GetCurrentThread() );
    DetourDetach( reinterpret_cast<PVOID*>(&Real_WinHttpConnect), reinterpret_cast<PVOID>(Mine_WinHttpConnect) );
    DetourDetach( reinterpret_cast<PVOID*>(&Real_WinHttpOpenRequest), reinterpret_cast<PVOID>(Mine_WinHttpOpenRequest) );
    DetourTransactionCommit();
    winhttp_attached = false;
}

static decltype(&getaddrinfo) Real_getaddrinfo = getaddrinfo;
static decltype(&GetAddrInfoW) Real_GetAddrInfoW = GetAddrInfoW;
static decltype(&GetAddrInfoExW) Real_GetAddrInfoExW = GetAddrInfoExW;

static INT WSAAPI Mine_getaddrinfo(PCSTR pNodeName, PCSTR pServiceName, const ADDRINFOA *pHints, PADDRINFOA *ppResult)
{
    const std::string *addr = pNodeName ? host_redirect(pNodeName) : nullptr;
    if (addr) {
        PRINT_DEBUG("redirecting host '%s' to '%s'", pNodeName, addr->c_str());
        return Real_getaddrinfo(addr->c_str(), pServiceName, pHints, ppResult);
    }

    return Real_getaddrinfo(pNodeName, pServiceName, pHints, ppResult);
}

static INT WSAAPI Mine_GetAddrInfoW(PCWSTR pNodeName, PCWSTR pServiceName, const ADDRINFOW *pHints, PADDRINFOW *ppResult)
{
    const std::wstring *addr = pNodeName ? host_redirect_w(pNodeName) : nullptr;
    if (addr) {
        PRINT_DEBUG("redirecting host '%ls' to '%ls'", pNodeName, addr->c_str());
        return Real_GetAddrInfoW(addr->c_str(), pServiceName, pHints, ppResult);
    }

    return Real_GetAddrInfoW(pNodeName, pServiceName, pHints, ppResult);
}

static INT WSAAPI Mine_GetAddrInfoExW(PCWSTR pName, PCWSTR pServiceName, DWORD dwNameSpace, LPGUID lpNspId, const ADDRINFOEXW *hints,
    PADDRINFOEXW *ppResult, struct timeval *timeout, LPOVERLAPPED lpOverlapped, LPLOOKUPSERVICE_COMPLETION_ROUTINE lpCompletionRoutine, LPHANDLE lpHandle)
{
    const std::wstring *addr = pName ? host_redirect_w(pName, "GetAddrInfoExW") : nullptr;
    if (addr) {
        PRINT_DEBUG("redirecting host '%ls' to '%ls'", pName, addr->c_str());
        pName = addr->c_str();
    }

    return Real_GetAddrInfoExW(pName, pServiceName, dwNameSpace, lpNspId, hints, ppResult, timeout, lpOverlapped, lpCompletionRoutine, lpHandle);
}

// custom: point the PlayFab SDK of Unity (il2cpp) games at the server from [main::playfab] (see settings_playfab_url())
// by rewriting the result of PlayFab.PlayFabSettings.GetFullUrl(), which builds the url of every PlayFab api call:
// https://<titleid>.playfabapi.com/Client/LoginWithSteam?sdk=... -> <url>/Client/LoginWithSteam?sdk=...
// the method can only be found once il2cpp_init() has loaded the metadata, so GameAssembly.dll's il2cpp_init() gets hooked first
static void* (*il2cpp_string_new_utf16_fn)(const wchar_t *text, int32_t len) = nullptr;
static void* (*Real_PlayFabGetFullUrl)(void *apiCall, void *getParams, void *apiSettings, const void *method) = nullptr;
static int (*Real_il2cpp_init)(const char *domain_name) = nullptr;

static void* Mine_PlayFabGetFullUrl(void *apiCall, void *getParams, void *apiSettings, const void *method)
{
    void *ret = Real_PlayFabGetFullUrl(apiCall, getParams, apiSettings, method);
    if (!ret) return ret;

    // System.String: object header (klass, monitor), int32 length, utf-16 chars
    auto str = reinterpret_cast<const char *>(ret) + 2 * sizeof(void *);
    std::wstring_view url(reinterpret_cast<const wchar_t *>(str + sizeof(int32_t)), *reinterpret_cast<const int32_t *>(str));
    auto host_start = url.find(L"://");
    if (host_start == std::wstring_view::npos) return ret;
    host_start += 3;
    auto path_start = url.find(L'/', host_start);
    auto host = url.substr(host_start, path_start == std::wstring_view::npos ? std::wstring_view::npos : path_start - host_start);
    if (common_helpers::to_lower(host).find(L"playfabapi.com") == std::wstring::npos) return ret;

    std::wstring new_url = settings_playfab_url();
    if (path_start != std::wstring_view::npos) new_url.append(url.substr(path_start));
    PRINT_DEBUG("playfab '%ls' -> '%ls'", std::wstring(url).c_str(), new_url.c_str());
    return il2cpp_string_new_utf16_fn(new_url.c_str(), static_cast<int32_t>(new_url.size()));
}

static DWORD WINAPI hook_playfab_get_full_url(LPVOID)
{
    HMODULE game_assembly = GetModuleHandleW(L"GameAssembly.dll");
    if (!game_assembly) return 0;

    auto domain_get = (void* (*)())GetProcAddress(game_assembly, "il2cpp_domain_get");
    auto domain_get_assemblies = (void** (*)(const void *, size_t *))GetProcAddress(game_assembly, "il2cpp_domain_get_assemblies");
    auto assembly_get_image = (void* (*)(const void *))GetProcAddress(game_assembly, "il2cpp_assembly_get_image");
    auto class_from_name = (void* (*)(const void *, const char *, const char *))GetProcAddress(game_assembly, "il2cpp_class_from_name");
    auto class_get_method_from_name = (const void* (*)(void *, const char *, int))GetProcAddress(game_assembly, "il2cpp_class_get_method_from_name");
    il2cpp_string_new_utf16_fn = (decltype(il2cpp_string_new_utf16_fn))GetProcAddress(game_assembly, "il2cpp_string_new_utf16");
    if (!domain_get || !domain_get_assemblies || !assembly_get_image || !class_from_name || !class_get_method_from_name || !il2cpp_string_new_utf16_fn) {
        PRINT_DEBUG("missing il2cpp exports");
        return 0;
    }

    size_t count = 0;
    void **assemblies = domain_get_assemblies(domain_get(), &count);
    for (size_t i = 0; assemblies && i < count; ++i) {
        void *klass = class_from_name(assembly_get_image(assemblies[i]), "PlayFab", "PlayFabSettings");
        if (!klass) continue;

        // MethodInfo starts with the pointer to the native code of the method
        const void *method = class_get_method_from_name(klass, "GetFullUrl", 3);
        if (!method || !*reinterpret_cast<void * const *>(method)) break;

        Real_PlayFabGetFullUrl = *reinterpret_cast<decltype(Real_PlayFabGetFullUrl) const *>(method);
        DetourTransactionBegin();
        DetourUpdateThread( GetCurrentThread() );
        DetourAttach( reinterpret_cast<PVOID*>(&Real_PlayFabGetFullUrl), reinterpret_cast<PVOID>(Mine_PlayFabGetFullUrl) );
        [[maybe_unused]] LONG res = DetourTransactionCommit();
        PRINT_DEBUG("hooking PlayFab.PlayFabSettings.GetFullUrl, result %li", res);
        return 0;
    }

    PRINT_DEBUG("PlayFab.PlayFabSettings.GetFullUrl not found");
    return 0;
}

// custom: own Photon app from [main::photon] (see settings_photon_app_id()) for Unity (il2cpp) games using Photon Realtime,
// also the copy inside Fusion (Fusion.Photon.Realtime): LoadBalancingPeer.OpAuthenticate()/OpAuthenticateOnce() get the
// app id of the own app, and unless keep_auth=1 the authentication type None, so Photon doesn't check the token of the game
// (custom authentication against PlayFab etc., which a replacement server can't produce)
// arguments after appId/authValues are only passed through, pointer sized like on the x64 stack
static bool (*Real_PhotonOpAuthenticate)(void *self, void *appId, void *appVersion, void *authValues, void *regionCode, uintptr_t getLobbyStatistics, const void *method) = nullptr;
static bool (*Real_PhotonOpAuthenticateOnce)(void *self, void *appId, void *appVersion, void *authValues, void *regionCode, uintptr_t encryptionMode, uintptr_t expectedProtocol, const void *method) = nullptr;
static void* (*il2cpp_object_get_class_fn)(void *obj) = nullptr;
static const void* (*il2cpp_class_get_method_from_name_fn)(void *klass, const char *name, int argsCount) = nullptr;
static void photon_patch_auth(void *&appId, void *authValues)
{
    // a new string on every call: a cached one isn't referenced by managed code and gets collected by the il2cpp gc
    const auto &app_id = settings_photon_app_id();
    void *str = il2cpp_string_new_utf16_fn(app_id.c_str(), static_cast<int32_t>(app_id.size()));
    if (str) appId = str;

    if (authValues && !settings_photon_keep_auth()) {
        // AuthenticationValues.AuthType = CustomAuthenticationType.None (byte 255)
        const void *setter = il2cpp_class_get_method_from_name_fn(il2cpp_object_get_class_fn(authValues), "set_AuthType", 1);
        if (setter && *reinterpret_cast<void * const *>(setter)) {
            auto fn = *reinterpret_cast<void (* const *)(void *, uint8_t, const void *)>(setter);
            fn(authValues, 255, setter);
        }
    }
}

static bool Mine_PhotonOpAuthenticate(void *self, void *appId, void *appVersion, void *authValues, void *regionCode, uintptr_t getLobbyStatistics, const void *method)
{
    photon_patch_auth(appId, authValues);
    PRINT_DEBUG("photon OpAuthenticate with own app id");
    return Real_PhotonOpAuthenticate(self, appId, appVersion, authValues, regionCode, getLobbyStatistics, method);
}

static bool Mine_PhotonOpAuthenticateOnce(void *self, void *appId, void *appVersion, void *authValues, void *regionCode, uintptr_t encryptionMode, uintptr_t expectedProtocol, const void *method)
{
    photon_patch_auth(appId, authValues);
    PRINT_DEBUG("photon OpAuthenticateOnce with own app id");
    return Real_PhotonOpAuthenticateOnce(self, appId, appVersion, authValues, regionCode, encryptionMode, expectedProtocol, method);
}

static DWORD WINAPI hook_photon_authenticate(LPVOID)
{
    HMODULE game_assembly = GetModuleHandleW(L"GameAssembly.dll");
    if (!game_assembly || Real_PhotonOpAuthenticate || Real_PhotonOpAuthenticateOnce) return 0;

    auto domain_get = (void* (*)())GetProcAddress(game_assembly, "il2cpp_domain_get");
    auto domain_get_assemblies = (void** (*)(const void *, size_t *))GetProcAddress(game_assembly, "il2cpp_domain_get_assemblies");
    auto assembly_get_image = (void* (*)(const void *))GetProcAddress(game_assembly, "il2cpp_assembly_get_image");
    auto class_from_name = (void* (*)(const void *, const char *, const char *))GetProcAddress(game_assembly, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name_fn = (decltype(il2cpp_class_get_method_from_name_fn))GetProcAddress(game_assembly, "il2cpp_class_get_method_from_name");
    il2cpp_object_get_class_fn = (decltype(il2cpp_object_get_class_fn))GetProcAddress(game_assembly, "il2cpp_object_get_class");
    il2cpp_string_new_utf16_fn = (decltype(il2cpp_string_new_utf16_fn))GetProcAddress(game_assembly, "il2cpp_string_new_utf16");
    if (!domain_get || !domain_get_assemblies || !assembly_get_image || !class_from_name || !il2cpp_class_get_method_from_name_fn ||
        !il2cpp_object_get_class_fn || !il2cpp_string_new_utf16_fn) {
        PRINT_DEBUG("missing il2cpp exports");
        return 0;
    }

    // Fusion brings its own copy of Photon Realtime, prefer it
    for (const char *ns : { "Fusion.Photon.Realtime", "Photon.Realtime" }) {
        size_t count = 0;
        void **assemblies = domain_get_assemblies(domain_get(), &count);
        for (size_t i = 0; assemblies && i < count; ++i) {
            void *klass = class_from_name(assembly_get_image(assemblies[i]), ns, "LoadBalancingPeer");
            if (!klass) continue;

            const void *auth = il2cpp_class_get_method_from_name_fn(klass, "OpAuthenticate", 5);
            const void *auth_once = il2cpp_class_get_method_from_name_fn(klass, "OpAuthenticateOnce", 6);
            DetourTransactionBegin();
            DetourUpdateThread( GetCurrentThread() );
            if (auth && *reinterpret_cast<void * const *>(auth)) {
                Real_PhotonOpAuthenticate = *reinterpret_cast<decltype(Real_PhotonOpAuthenticate) const *>(auth);
                DetourAttach( reinterpret_cast<PVOID*>(&Real_PhotonOpAuthenticate), reinterpret_cast<PVOID>(Mine_PhotonOpAuthenticate) );
            }
            if (auth_once && *reinterpret_cast<void * const *>(auth_once)) {
                Real_PhotonOpAuthenticateOnce = *reinterpret_cast<decltype(Real_PhotonOpAuthenticateOnce) const *>(auth_once);
                DetourAttach( reinterpret_cast<PVOID*>(&Real_PhotonOpAuthenticateOnce), reinterpret_cast<PVOID>(Mine_PhotonOpAuthenticateOnce) );
            }
            [[maybe_unused]] LONG res = DetourTransactionCommit();
            PRINT_DEBUG("hooking %s.LoadBalancingPeer.OpAuthenticate(Once), result %li", ns, res);
            return 0;
        }
    }

    PRINT_DEBUG("LoadBalancingPeer not found");
    return 0;
}

static bool il2cpp_hooks_wanted()
{
    return settings_playfab_url().size() || settings_photon_app_id().size();
}

static DWORD WINAPI hook_il2cpp_methods(LPVOID)
{
    if (settings_playfab_url().size()) hook_playfab_get_full_url(nullptr);
    if (settings_photon_app_id().size()) hook_photon_authenticate(nullptr);
    return 0;
}

static int Mine_il2cpp_init(const char *domain_name)
{
    int ret = Real_il2cpp_init(domain_name);
    hook_il2cpp_methods(nullptr);
    return ret;
}

static void hook_il2cpp_init(HMODULE game_assembly)
{
    if (Real_il2cpp_init) return;

    Real_il2cpp_init = (decltype(Real_il2cpp_init))GetProcAddress(game_assembly, "il2cpp_init");
    if (!Real_il2cpp_init) return;

    DetourTransactionBegin();
    DetourUpdateThread( GetCurrentThread() );
    DetourAttach( reinterpret_cast<PVOID*>(&Real_il2cpp_init), reinterpret_cast<PVOID>(Mine_il2cpp_init) );
    [[maybe_unused]] LONG res = DetourTransactionCommit();
    PRINT_DEBUG("hooking il2cpp_init for playfab, result %li", res);
}

// custom: load the replacement for dlls listed in [main::dll_redirect] (see settings_dll_redirects()),
// no matter whether the game loads them by name or by full path (UE loads EOSSDK by full path)
HMODULE (WINAPI *Real_LoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) = nullptr;

static HMODULE WINAPI Mine_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    constexpr DWORD data_flags = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;
    if (lpLibFileName && !(dwFlags & data_flags)) {
        std::wstring_view path(lpLibFileName);
        auto sep = path.find_last_of(L"\\/");
        auto name = common_helpers::to_lower(sep == std::wstring_view::npos ? path : path.substr(sep + 1));
        const auto &redirects = settings_dll_redirects();
        auto it = redirects.find(name);
        if (it != redirects.end()) {
            PRINT_DEBUG("redirecting '%ls' to '%ls'", lpLibFileName, it->second.c_str());
            return Real_LoadLibraryExW(it->second.c_str(), hFile, dwFlags);
        }

        // custom: see Mine_PlayFabGetFullUrl() and Mine_PhotonOpAuthenticate()
        if (name == L"gameassembly.dll" && il2cpp_hooks_wanted()) {
            HMODULE module = Real_LoadLibraryExW(lpLibFileName, hFile, dwFlags);
            if (module) hook_il2cpp_init(module);
            return module;
        }
    }

    HMODULE module = Real_LoadLibraryExW(lpLibFileName, hFile, dwFlags);
    // custom: winhttp.dll may come in as a dependency of any dll, see attach_winhttp_hooks()
    if (module && !winhttp_attached && winhttp_late_hooks_wanted()) attach_winhttp_hooks();
    return module;
}

static bool network_functions_attached = false;
static bool loadlibrary_attached = false;
static bool host_functions_attached = false;
BOOL WINAPI DllMain( HINSTANCE, DWORD dwReason, LPVOID )
{
    switch ( dwReason ) {
        case DLL_PROCESS_ATTACH:
            PRINT_DEBUG("experimental DLL_PROCESS_ATTACH");
            if (!settings_disable_lan_only()) {
                PRINT_DEBUG("Hooking lan only functions");
                DetourTransactionBegin();
                DetourUpdateThread( GetCurrentThread() );
                DetourAttach( reinterpret_cast<PVOID*>(&Real_SendTo), reinterpret_cast<PVOID>(Mine_SendTo) );
                DetourAttach( reinterpret_cast<PVOID*>(&Real_Connect), reinterpret_cast<PVOID>(Mine_Connect) );
                DetourAttach( reinterpret_cast<PVOID*>(&Real_WSAConnect), reinterpret_cast<PVOID>(Mine_WSAConnect) );
                DetourTransactionCommit();
                network_functions_attached = true;
            }
            // custom: see Mine_WinHttpConnect(), only if winhttp.dll is already there, otherwise Mine_LoadLibraryExW() does it
            if (winhttp_hooks_wanted()) {
                attach_winhttp_hooks();
            }
            // custom: see Mine_LoadLibraryExW()
            if (!settings_dll_redirects().empty() || il2cpp_hooks_wanted() || winhttp_late_hooks_wanted()) {
                HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
                Real_LoadLibraryExW = (decltype(Real_LoadLibraryExW))GetProcAddress(kernelbase ? kernelbase : GetModuleHandleW(L"kernel32.dll"), "LoadLibraryExW");
                if (Real_LoadLibraryExW) {
                    PRINT_DEBUG("Hooking LoadLibraryExW for dll redirects");
                    DetourTransactionBegin();
                    DetourUpdateThread( GetCurrentThread() );
                    DetourAttach( reinterpret_cast<PVOID*>(&Real_LoadLibraryExW), reinterpret_cast<PVOID>(Mine_LoadLibraryExW) );
                    loadlibrary_attached = DetourTransactionCommit() == NO_ERROR;
                }
            }
            // custom: see Mine_PlayFabGetFullUrl(), without ForceInjectSteamClient the game has already initialized il2cpp
            // when it loads this dll, the hook is placed from a new thread because il2cpp must not be called under the loader lock
            if (il2cpp_hooks_wanted() && GetModuleHandleW(L"GameAssembly.dll")) {
                HANDLE thread = CreateThread(nullptr, 0, hook_il2cpp_methods, nullptr, 0, nullptr);
                if (thread) CloseHandle(thread);
            }
            // custom: see Mine_getaddrinfo()
            if (!settings_host_redirects().empty()) {
                for (const auto &[host, addr] : settings_host_redirects()) {
                    host_redirects_w[host] = common_helpers::to_wstr(addr);
                }
                PRINT_DEBUG("Hooking getaddrinfo for host redirects");
                DetourTransactionBegin();
                DetourUpdateThread( GetCurrentThread() );
                DetourAttach( reinterpret_cast<PVOID*>(&Real_getaddrinfo), reinterpret_cast<PVOID>(Mine_getaddrinfo) );
                DetourAttach( reinterpret_cast<PVOID*>(&Real_GetAddrInfoW), reinterpret_cast<PVOID>(Mine_GetAddrInfoW) );
                DetourAttach( reinterpret_cast<PVOID*>(&Real_GetAddrInfoExW), reinterpret_cast<PVOID>(Mine_GetAddrInfoExW) );
                host_functions_attached = DetourTransactionCommit() == NO_ERROR;
            }
            load_crack_dll();
            load_dlls();
        break;

        case DLL_PROCESS_DETACH:
            PRINT_DEBUG("experimental DLL_PROCESS_DETACH");
            if (network_functions_attached) {
                DetourTransactionBegin();
                DetourUpdateThread( GetCurrentThread() );
                DetourDetach( reinterpret_cast<PVOID*>(&Real_SendTo), reinterpret_cast<PVOID>(Mine_SendTo) );
                DetourDetach( reinterpret_cast<PVOID*>(&Real_Connect), reinterpret_cast<PVOID>(Mine_Connect) );
                DetourDetach( reinterpret_cast<PVOID*>(&Real_WSAConnect), reinterpret_cast<PVOID>(Mine_WSAConnect) );
                DetourTransactionCommit();
            }
            detach_winhttp_hooks();
            if (loadlibrary_attached) {
                DetourTransactionBegin();
                DetourUpdateThread( GetCurrentThread() );
                DetourDetach( reinterpret_cast<PVOID*>(&Real_LoadLibraryExW), reinterpret_cast<PVOID>(Mine_LoadLibraryExW) );
                DetourTransactionCommit();
            }
            if (host_functions_attached) {
                DetourTransactionBegin();
                DetourUpdateThread( GetCurrentThread() );
                DetourDetach( reinterpret_cast<PVOID*>(&Real_getaddrinfo), reinterpret_cast<PVOID>(Mine_getaddrinfo) );
                DetourDetach( reinterpret_cast<PVOID*>(&Real_GetAddrInfoW), reinterpret_cast<PVOID>(Mine_GetAddrInfoW) );
                DetourDetach( reinterpret_cast<PVOID*>(&Real_GetAddrInfoExW), reinterpret_cast<PVOID>(Mine_GetAddrInfoExW) );
                DetourTransactionCommit();
            }
            // the il2cpp hooks stay, GameAssembly.dll is only unloaded together with the process

            unload_dlls();
        break;
    }

    return TRUE;
}

#else


// this acts as both an entry and an exit points for the library
// avoid "__attribute__((__constructor__))" and "__attribute__((__destructor__))"
// since they run before the C+++ runtime has been initialized
// causing problems related to uninitialized global objects
struct CppRuntimeTrick {
    CppRuntimeTrick()
    {
        PRINT_DEBUG_ENTRY();
        load_dlls();
    }

    ~CppRuntimeTrick()
    {
        PRINT_DEBUG_ENTRY();
        unload_dlls();
    }
} static g_cpp_rt{};


void set_whitelist_ips(uint32_t *from, uint32_t *to, unsigned num_ips)
{

}

#endif // __WINDOWS__

#else

void set_whitelist_ips(uint32_t *from, uint32_t *to, unsigned num_ips)
{

}

#endif // EMU_EXPERIMENTAL_BUILD
