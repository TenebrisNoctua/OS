#include "Library.h"
#include "lualib.h"

#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <malloc.h>
#include <psapi.h>
#else
#include <climits>
#include <unistd.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#if !defined(__APPLE__)
extern char** environ;
#endif

// Linux names the longest host name, macOS only has the POSIX minimum's name for it
#ifndef HOST_NAME_MAX
#if defined(_POSIX_HOST_NAME_MAX)
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#else
#define HOST_NAME_MAX 255
#endif
#endif
#endif

#if defined(__APPLE__)
#include <crt_externs.h>
#include <mach/mach.h>
#include <malloc/malloc.h>
#endif

namespace Luwow::Os {
    ILuauModule* Library::initialize(ILuauHost* host) {
        Library* os = new Library();
        os->host = host;
        return os;
    }

    Luwow::Engine::RunMode Library::getRunMode() const {
        return LUWOW_MODULE_RUN_MODE;
    }

#if defined(_WIN32)
    static std::wstring toWide(const std::string& text) {
        if (text.empty()) return {};
        int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(length, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    static std::string toUtf8(const std::wstring& wide) {
        if (wide.empty()) return {};
        int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string text(length, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), text.data(), length, nullptr, nullptr);
        return text;
    }

    static void pushUtf8(lua_State* L, const std::wstring& wide) {
        std::string text = toUtf8(wide);
        lua_pushlstring(L, text.data(), text.size());
    }
#endif

    // Environment

    // os.getEnv(name: string) -> string?
    static int os_getEnv(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
#if defined(_WIN32)
        std::wstring wideName = toWide(name);
        DWORD size = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
        if (size == 0) {
            lua_pushnil(L);
            return 1;
        }
        std::wstring value(size, L'\0');
        size = GetEnvironmentVariableW(wideName.c_str(), value.data(), size);
        value.resize(size);
        pushUtf8(L, value);
#else
        const char* value = getenv(name);
        if (value) lua_pushstring(L, value);
        else lua_pushnil(L);
#endif
        return 1;
    }

    // os.setEnv(name: string, value: string?), nil removes the variable
    static int os_setEnv(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        const char* value = luaL_optstring(L, 2, nullptr);
#if defined(_WIN32)
        BOOL ok = SetEnvironmentVariableW(toWide(name).c_str(), value ? toWide(value).c_str() : nullptr);
        if (!ok && value) luaL_error(L, "could not set the environment variable %s", name);
#else
        int result = value ? setenv(name, value, 1) : unsetenv(name);
        if (result != 0) luaL_error(L, "could not set the environment variable %s", name);
#endif
        return 0;
    }

    // os.environ() -> { [string]: string }
    static int os_environ(lua_State* L) {
        lua_newtable(L);
#if defined(_WIN32)
        LPWCH block = GetEnvironmentStringsW();
        if (!block) return 1;
        for (LPWCH entry = block; *entry; entry += wcslen(entry) + 1) {
            std::wstring line(entry);
            // Entries such as "=C:=C:\" name drives' directories, they aren't variables
            size_t equals = line.find(L'=', 1);
            if (line[0] == L'=' || equals == std::wstring::npos) continue;
            pushUtf8(L, line.substr(0, equals));
            pushUtf8(L, line.substr(equals + 1));
            lua_settable(L, -3);
        }
        FreeEnvironmentStringsW(block);
#else
#if defined(__APPLE__)
        // A module is a bundle, which can't link to environ on macOS. Only a whole program can
        char** environment = *_NSGetEnviron();
#else
        char** environment = environ;
#endif
        for (char** entry = environment; entry && *entry; ++entry) {
            const char* equals = strchr(*entry, '=');
            if (!equals) continue;
            lua_pushlstring(L, *entry, equals - *entry);
            lua_pushstring(L, equals + 1);
            lua_settable(L, -3);
        }
#endif
        return 1;
    }

    // System

    static int os_platform(lua_State* L) {
#if defined(_WIN32)
        lua_pushstring(L, "windows");
#elif defined(__APPLE__)
        lua_pushstring(L, "macos");
#else
        lua_pushstring(L, "linux");
#endif
        return 1;
    }

    static int os_arch(lua_State* L) {
#if defined(_M_ARM64) || defined(__aarch64__)
        lua_pushstring(L, "arm64");
#elif defined(_M_X64) || defined(__x86_64__)
        lua_pushstring(L, "x64");
#else
        lua_pushstring(L, "x86");
#endif
        return 1;
    }

    static int os_hostName(lua_State* L) {
#if defined(_WIN32)
        wchar_t name[MAX_COMPUTERNAME_LENGTH + 64];
        DWORD size = static_cast<DWORD>(std::size(name));
        if (!GetComputerNameExW(ComputerNameDnsHostname, name, &size)) luaL_error(L, "could not get the host name");
        pushUtf8(L, std::wstring(name, size));
#else
        char name[HOST_NAME_MAX + 1] = {};
        if (gethostname(name, sizeof(name) - 1) != 0) luaL_error(L, "could not get the host name");
        lua_pushstring(L, name);
#endif
        return 1;
    }

    // The user's home folder, with forward slashes on Windows too
    static int os_homeDirectory(lua_State* L) {
#if defined(_WIN32)
        DWORD size = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
        if (size == 0) luaL_error(L, "could not get the home directory");
        std::wstring value(size, L'\0');
        value.resize(GetEnvironmentVariableW(L"USERPROFILE", value.data(), size));
        for (wchar_t& c : value) if (c == L'\\') c = L'/';
        pushUtf8(L, value);
#else
        const char* home = getenv("HOME");
        if (!home) {
            passwd* entry = getpwuid(getuid());
            home = entry ? entry->pw_dir : nullptr;
        }
        if (!home) luaL_error(L, "could not get the home directory");
        lua_pushstring(L, home);
#endif
        return 1;
    }

    // The folder for temporary files, with forward slashes and without a trailing one
    static int os_tempDirectory(lua_State* L) {
#if defined(_WIN32)
        wchar_t path[MAX_PATH + 1];
        DWORD size = GetTempPathW(static_cast<DWORD>(std::size(path)), path);
        if (size == 0) luaL_error(L, "could not get the temporary directory");
        std::wstring value(path, size);
        for (wchar_t& c : value) if (c == L'\\') c = L'/';
        while (value.size() > 3 && value.back() == L'/') value.pop_back();
        pushUtf8(L, value);
#else
        const char* temp = getenv("TMPDIR");
        std::string value = temp ? temp : "/tmp";
        while (value.size() > 1 && value.back() == '/') value.pop_back();
        lua_pushlstring(L, value.data(), value.size());
#endif
        return 1;
    }

    // Memory

    static const char* const gcOptions[] = { "collect", "count", "step", "isrunning", "stop", "restart", "setgoal", "setstepmul", "setstepsize", nullptr };

    // os.collectGarbage(option?, argument?), like Lua's collectgarbage. "collect" (the default) runs a full
    // cycle and returns the heap's size in KB, "count" returns it without collecting.
    static int os_collectGarbage(lua_State* L) {
        int option = luaL_checkoption(L, 1, "collect", gcOptions);
        int argument = luaL_optinteger(L, 2, 0);
        switch (option) {
            case 0:
                lua_gc(L, LUA_GCCOLLECT, 0);
                lua_pushnumber(L, lua_gc(L, LUA_GCCOUNT, 0) + lua_gc(L, LUA_GCCOUNTB, 0) / 1024.0);
                return 1;
            case 1:
                lua_pushnumber(L, lua_gc(L, LUA_GCCOUNT, 0) + lua_gc(L, LUA_GCCOUNTB, 0) / 1024.0);
                return 1;
            case 2:
                lua_pushboolean(L, lua_gc(L, LUA_GCSTEP, argument));
                return 1;
            case 3:
                lua_pushboolean(L, lua_gc(L, LUA_GCISRUNNING, 0));
                return 1;
            case 4:
                lua_gc(L, LUA_GCSTOP, 0);
                return 0;
            case 5:
                lua_gc(L, LUA_GCRESTART, 0);
                return 0;
            case 6:
                lua_pushinteger(L, lua_gc(L, LUA_GCSETGOAL, argument));
                return 1;
            case 7:
                lua_pushinteger(L, lua_gc(L, LUA_GCSETSTEPMUL, argument));
                return 1;
            default:
                lua_pushinteger(L, lua_gc(L, LUA_GCSETSTEPSIZE, argument));
                return 1;
        }
    }

    // The process's memory: the Luau heap, and what the system counts for it
    static void pushMemoryUsage(lua_State* L) {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, (lua_gc(L, LUA_GCCOUNT, 0) * 1024.0) + lua_gc(L, LUA_GCCOUNTB, 0));
        lua_setfield(L, -2, "heap");
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
            lua_pushnumber(L, static_cast<double>(counters.WorkingSetSize));
            lua_setfield(L, -2, "workingSet");
            lua_pushnumber(L, static_cast<double>(counters.PrivateUsage));
            lua_setfield(L, -2, "private");
        }
#elif defined(__APPLE__)
        mach_task_basic_info info{};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
            lua_pushnumber(L, static_cast<double>(info.resident_size));
            lua_setfield(L, -2, "workingSet");
            lua_pushnumber(L, static_cast<double>(info.virtual_size));
            lua_setfield(L, -2, "private");
        }
#else
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            lua_pushnumber(L, static_cast<double>(usage.ru_maxrss) * 1024.0);
            lua_setfield(L, -2, "workingSet");
        }
#endif
    }

    // os.memoryUsage() -> { heap: number, workingSet: number?, private: number? }, in bytes
    static int os_memoryUsage(lua_State* L) {
        pushMemoryUsage(L);
        return 1;
    }

    // os.releaseMemory() -> { heap, workingSet, private }: collects the Luau garbage, then gives the memory
    // the heaps no longer use back to the system. Pages trimmed
    // from the working set come back as they're used.
    static int os_releaseMemory(lua_State* L) {
        lua_gc(L, LUA_GCCOLLECT, 0);
        lua_gc(L, LUA_GCCOLLECT, 0);
#if defined(_WIN32)
        _heapmin();
        DWORD count = GetProcessHeaps(0, nullptr);
        std::vector<HANDLE> heaps(count);
        count = GetProcessHeaps(count, heaps.data());
        for (DWORD i = 0; i < count; ++i) HeapCompact(heaps[i], 0);
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#elif defined(__APPLE__)
        malloc_zone_pressure_relief(nullptr, 0);
#endif
        pushMemoryUsage(L);
        return 1;
    }

    // Registry, Windows only

#if defined(_WIN32)
    // "HKEY_CURRENT_USER\Software\Classes", or "HKCU/Software/Classes": the root and the subkey
    static HKEY splitKey(lua_State* L, int index, std::wstring& subkey) {
        std::string path = luaL_checkstring(L, index);
        for (char& c : path) if (c == '/') c = '\\';
        size_t slash = path.find('\\');
        std::string root = path.substr(0, slash);
        subkey = slash == std::string::npos ? L"" : toWide(path.substr(slash + 1));
        for (char& c : root) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        if (root == "HKEY_CURRENT_USER" || root == "HKCU") return HKEY_CURRENT_USER;
        if (root == "HKEY_LOCAL_MACHINE" || root == "HKLM") return HKEY_LOCAL_MACHINE;
        if (root == "HKEY_CLASSES_ROOT" || root == "HKCR") return HKEY_CLASSES_ROOT;
        if (root == "HKEY_USERS" || root == "HKU") return HKEY_USERS;
        if (root == "HKEY_CURRENT_CONFIG" || root == "HKCC") return HKEY_CURRENT_CONFIG;
        luaL_error(L, "unknown registry root %s", root.c_str());
        return nullptr;
    }
#endif

    // os.getRegistryValue(key: string, name: string?) -> string | number | nil. No name reads the default value.
    static int os_getRegistryValue(lua_State* L) {
#if defined(_WIN32)
        std::wstring subkey;
        HKEY root = splitKey(L, 1, subkey);
        std::wstring name = toWide(luaL_optstring(L, 2, ""));

        HKEY key;
        if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
            lua_pushnil(L);
            return 1;
        }
        DWORD type = 0, size = 0;
        LSTATUS status = RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &size);
        std::vector<BYTE> data(size + sizeof(wchar_t));
        if (status == ERROR_SUCCESS) status = RegQueryValueExW(key, name.c_str(), nullptr, &type, data.data(), &size);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) {
            lua_pushnil(L);
            return 1;
        }

        if (type == REG_SZ || type == REG_EXPAND_SZ) {
            std::wstring value(reinterpret_cast<wchar_t*>(data.data()), size / sizeof(wchar_t));
            while (!value.empty() && value.back() == L'\0') value.pop_back();
            pushUtf8(L, value);
        } else if (type == REG_DWORD) {
            lua_pushnumber(L, *reinterpret_cast<DWORD*>(data.data()));
        } else if (type == REG_QWORD) {
            lua_pushnumber(L, static_cast<double>(*reinterpret_cast<ULONGLONG*>(data.data())));
        } else {
            lua_pushlstring(L, reinterpret_cast<char*>(data.data()), size);
        }
        return 1;
#else
        lua_pushnil(L);
        return 1;
#endif
    }

    // os.setRegistryValue(key: string, name: string?, value: string | number), the key is made when it
    // doesn't exist. A string is stored as REG_SZ, a number as REG_DWORD.
    static int os_setRegistryValue(lua_State* L) {
#if defined(_WIN32)
        std::wstring subkey;
        HKEY root = splitKey(L, 1, subkey);
        std::wstring name = toWide(luaL_optstring(L, 2, ""));

        HKEY key;
        if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
            luaL_error(L, "could not open the registry key %s", luaL_checkstring(L, 1));
        }
        LSTATUS status;
        if (lua_type(L, 3) == LUA_TNUMBER) {
            DWORD value = static_cast<DWORD>(lua_tonumber(L, 3));
            status = RegSetValueExW(key, name.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        } else {
            std::wstring value = toWide(luaL_checkstring(L, 3));
            status = RegSetValueExW(key, name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) luaL_error(L, "could not set the registry value");
#else
        luaL_error(L, "the registry only exists on Windows");
#endif
        return 0;
    }

    // os.createRegistryKey(key: string), with the keys above it. A key that exists is left as it is.
    static int os_createRegistryKey(lua_State* L) {
#if defined(_WIN32)
        std::wstring subkey;
        HKEY root = splitKey(L, 1, subkey);
        HKEY key;
        if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
            luaL_error(L, "could not create the registry key %s", luaL_checkstring(L, 1));
        }
        RegCloseKey(key);
#else
        luaL_error(L, "the registry only exists on Windows");
#endif
        return 0;
    }

    // os.deleteRegistryKey(key: string), with its subkeys. Deleting a key that doesn't exist does nothing.
    static int os_deleteRegistryKey(lua_State* L) {
#if defined(_WIN32)
        std::wstring subkey;
        HKEY root = splitKey(L, 1, subkey);
        if (subkey.empty()) luaL_error(L, "a registry root can't be deleted");
        LSTATUS status = RegDeleteTreeW(root, subkey.c_str());
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) luaL_error(L, "could not delete the registry key");
#else
        luaL_error(L, "the registry only exists on Windows");
#endif
        return 0;
    }

    const char* Library::getModuleName() const {
        return "os";
    }

    const char* Library::getModuleAlias() const {
        return "Luwow";
    }

    static LuauExport exports[] = {
        { "getEnv", os_getEnv },
        { "setEnv", os_setEnv },
        { "environ", os_environ },
        { "platform", os_platform },
        { "arch", os_arch },
        { "hostName", os_hostName },
        { "homeDirectory", os_homeDirectory },
        { "tempDirectory", os_tempDirectory },
        { "collectGarbage", os_collectGarbage },
        { "memoryUsage", os_memoryUsage },
        { "releaseMemory", os_releaseMemory },
        { "getRegistryValue", os_getRegistryValue },
        { "setRegistryValue", os_setRegistryValue },
        { "createRegistryKey", os_createRegistryKey },
        { "deleteRegistryKey", os_deleteRegistryKey },
        { nullptr, nullptr }
    };

    const LuauExport* Library::getExports() const {
        return exports;
    }
} // namespace Luwow::Os

LUWOW_REGISTER_MODULE(Luwow::Os::Library)
