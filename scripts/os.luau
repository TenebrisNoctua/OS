-- This is the type system for the OS API as it exists in C++ code.

--[=[
	The process's memory in bytes, returned by `os.memoryUsage` and `os.releaseMemory`.
	`heap` is what the Luau heap holds, `workingSet` what the process has in physical memory,
	and `private` what it has committed. The last two are nil where the system doesn't tell.
]=]
export type MemoryUsage = {
	heap: number,
	workingSet: number?,
	private: number?
}

export type GarbageOption = "collect" | "count" | "step" | "isrunning" | "stop" | "restart" | "setgoal" | "setstepmul" | "setstepsize"

local os = {}

--[=[
	Reads one of the process's environment variables, such as LOCALAPPDATA.

	@param name The variable's name. Case doesn't matter on Windows.
	@return Its value, nil when it isn't set.
]=]
function os.getEnv(name: string): string?
    return nil :: any
end

--[=[
	Sets one of the process's environment variables. Programs started afterwards inherit it.

	@param name The variable's name.
	@param value Its new value, nil removes the variable.
]=]
function os.setEnv(name: string, value: string?): ()
    return nil :: any
end

--[=[
	Every environment variable of the process.
]=]
function os.environ(): { [string]: string }
    return nil :: any
end

--[=[
	The system the program runs on.
]=]
function os.platform(): "windows" | "macos" | "linux"
    return nil :: any
end

--[=[
	The processor architecture the program was built for.
]=]
function os.arch(): "x64" | "arm64" | "x86"
    return nil :: any
end

--[=[
	The computer's host name.
]=]
function os.hostName(): string
    return nil :: any
end

--[=[
	The user's home folder, with forward slashes.
]=]
function os.homeDirectory(): string
    return nil :: any
end

--[=[
	The folder for temporary files, with forward slashes and without a trailing one.
]=]
function os.tempDirectory(): string
    return nil :: any
end

--[=[
	Controls the garbage collector, like Lua's `collectgarbage`.

	@param option "collect" (the default) runs a full cycle and returns the heap's size in KB.
	"count" returns the size without collecting. "step" runs a step of the given size and returns whether
	a cycle finished. "isrunning", "stop" and "restart" control whether it runs on its own. "setgoal",
	"setstepmul" and "setstepsize" tune it and return the previous value.
	@param argument The value for "step" and the "set" options.
]=]
function os.collectGarbage(option: GarbageOption?, argument: number?): number | boolean | nil
    return nil :: any
end

--[=[
	The process's memory, see `MemoryUsage`.
]=]
function os.memoryUsage(): MemoryUsage
    return nil :: any
end

--[=[
	Collects the Luau garbage, then hands the memory the heaps no longer use back to the system and trims
	the working set. Worth calling after dropping a lot of data, such as
	once a big download is parsed or a window closes. Trimmed pages come back as they're used.

	@return The memory afterwards.
]=]
function os.releaseMemory(): MemoryUsage
    return nil :: any
end

--[=[
	Reads a registry value, Windows only.

	@param key The key's path. The root may be written
	in full (HKEY_CURRENT_USER) or short (HKCU, HKLM, HKCR, HKU, HKCC), with / or \ between the parts.
	@param name The value's name, nil for the key's default value.
	@return A string for REG_SZ and REG_EXPAND_SZ, a number for REG_DWORD and REG_QWORD, the raw bytes as
	a string otherwise, nil when the key or value doesn't exist or on other systems.
]=]
function os.getRegistryValue(key: string, name: string?): (string | number)?
    return nil :: any
end

--[=[
	Writes a registry value, making the key when it doesn't exist. Windows only.

	@param key The key's path, as for `getRegistryValue`.
	@param name The value's name, nil for the key's default value.
	@param value A string is stored as REG_SZ, a number as REG_DWORD.
]=]
function os.setRegistryValue(key: string, name: string?, value: string | number): ()
    return nil :: any
end

--[=[
	Makes a registry key and the keys above it, without values. A key that exists is left as it is.
	Windows only.

	@param key The key's path, as for `getRegistryValue`.
]=]
function os.createRegistryKey(key: string): ()
    return nil :: any
end

--[=[
	Deletes a registry key with its subkeys. Deleting one that doesn't exist does nothing. Windows only.

	@param key The key's path, as for `getRegistryValue`.
]=]
function os.deleteRegistryKey(key: string): ()
    return nil :: any
end

return table.freeze(os)
