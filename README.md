# OS
An operating system library for Luwow-Project. 
The environment, system folders, memory and the Windows registry.

This repository is not stand-alone. You must build it externally from within another
repository like https://github.com/Luwow-Project/Release

## Project Overview

This project contains the sources for Luwow OS library. Strings are UTF-8, and on Windows they're converted to and from UTF-16 for the system. Paths are given with forward slashes. The library only supports the `SERIAL` run mode.

## Library Documentation

### Environment

#### `os.getEnv(name: string) -> string?`

One of the process's environment variables, such as `LOCALAPPDATA`, nil when it isn't set. Names are case-insensitive on Windows.

#### `os.setEnv(name: string, value: string?)`

Sets an environment variable, nil removes it. Programs started afterwards inherit it.

#### `os.environ() -> { [string]: string }`

Every environment variable.

### System

#### `os.platform() -> "windows" | "macos" | "linux"`

#### `os.arch() -> "x64" | "arm64" | "x86"`

The architecture the program was built for.

#### `os.hostName() -> string`

#### `os.homeDirectory() -> string`

The user's home folder.

#### `os.tempDirectory() -> string`

The folder for temporary files, without a trailing slash.

### Memory

#### `os.collectGarbage(option: string?, argument: number?)`

Controls the Luau garbage collector, like Lua's `collectgarbage`. `"collect"` (the default) runs a full cycle and returns the heap's size in KB, `"count"` returns it without collecting. `"step"`, `"isrunning"`, `"stop"`, `"restart"`, `"setgoal"`, `"setstepmul"` and `"setstepsize"` are also supported.

#### `os.memoryUsage() -> { heap: number, workingSet: number?, private: number? }`

The Luau heap's size and what the system counts for the process, in bytes.

#### `os.releaseMemory() -> { heap: number, workingSet: number?, private: number? }`

Collects the Luau garbage twice, then gives the memory the C heaps no longer use back to the system and trims the working set. Call it after dropping a lot of data. Returns the memory data afterwards.

### Registry

Windows only. Keys are written as `"HKCU/Software/Classes/XXX"`, with the root in full or short (`HKCU`, `HKLM`, `HKCR`, `HKU`, `HKCC`) and `/` or `\` between the parts.

#### `os.getRegistryValue(key: string, name: string?) -> string | number | nil`

A value, the key's default one when `name` is nil. Strings come back as strings, DWORDs and QWORDs as numbers, other types as their raw bytes. Nil when it doesn't exist, and always on other systems.

#### `os.setRegistryValue(key: string, name: string?, value: string | number)`

Writes a value, making the key if needed. Strings are stored as `REG_SZ`, numbers as `REG_DWORD`.

#### `os.createRegistryKey(key: string)`

Makes a key and the keys above it, without values. An existing key is left as it is.

#### `os.deleteRegistryKey(key: string)`

Deletes a key with its subkeys. A missing key is ignored.
