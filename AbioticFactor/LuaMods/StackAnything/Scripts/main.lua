local tweaker = require("tweaker")
local dumper = require("dumper")

-- Debug: set true and restart to dump all item rows to item_dump.lua.
local DUMP_ITEMS = false

local function NormalizePath(path)
    if type(path) ~= "string" then return nil end
    return path:gsub("\\", "/")
end

-- Mod root dir from the script location (avoids hard-coding the folder name).
local function GetModPath()
    local info = debug.getinfo(1, "S")
    if info and info.source then
        local src = NormalizePath(info.source)
        local modPath = src and src:match("^@(.*/)[Ss]cripts/")
        if modPath and modPath ~= "" then
            return modPath
        end
    end

    return string.format("Mods/StackAnything/")
end

ExecuteInGameThread(function()
    local globalDt = tweaker.getGlobalTable()
    if not globalDt then
        print("[SA] WARNING: Could not find ItemTable_Global\n")
    end

    local tables = tweaker.discoverItemTables()

    if DUMP_ITEMS then
        print("[SA] Dumping item list to \"item_dump.lua\"")
        dumper.dumpTables(tables, globalDt, GetModPath() .. "item_dump.lua")
        print("[SA] Item dump complete\n")
    end

end)
