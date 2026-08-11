local dumper = {}

local ITEM_NAME_PROP  = "ItemName_51_B88648C048EE5BC2885E4E95F3E13F0A"
local STACK_PROP      = "StackSize_47_D124F11B4B6D9766B2B33699795845A9"
local WEAPON_PROP     = "IsWeapon_63_57F6A703413EA260B1455CA81F2D4911"
local EQUIP_PROP      = "EquipmentData_100_576D05464F36104AFE501B878255E318"
local EQUIP_SLOT_PROP = "EquipSlot_5_7DAF59D54ADD37B8594D91A65C47292E"
local LIQUID_PROP     = "LiquidData_110_4D07F09C483C1E65B39024ABC7032FA0"
local LIQUID_MAX_PROP = "MaxLiquid_16_80D4968B4CACEDD3D4018E87DA67E8B4"
local LIQUID_ALLOWED  = "AllowedLiquids_7_1DF3EB8C43F49DA3A1E4A2AF908148D3"
local CONSUME_PROP    = "ConsumableData_84_757B6B114FF23016981BEF888A31C670"
local CONSUME_TIME    = "TimeToConsume_32_8034526445B400D3948365AF202D7D0E"
local CLASS_PROP      = "ItemClass_7_A94CF32A438C7DF014B8E2AF8D01B0AB"

local function safe(fn, default)
    local ok, res = pcall(fn)
    if ok then return res end
    return default
end

-- Stringify an FText/display name or any userdata/primitive.
local function toDisplayName(value)
    if value == nil then return "" end
    if type(value) == "userdata" and value.ToString then
        return safe(function() return tostring(value:ToString()) end, "")
    end
    return tostring(value)
end

-- Class path from a TSoftClassPtr field.
local function toClassPath(value)
    if value == nil then return "" end
    if type(value) == "userdata" then
        return safe(function() return value:GetPathName() end,
               safe(function() return tostring(value) end, ""))
    end
    return tostring(value)
end

local function dumpRow(row, rowName)
    local entry = {
        name = toDisplayName(row[ITEM_NAME_PROP]),
        stack = safe(function() return row[STACK_PROP] end, 0),
        weapon = safe(function() return row[WEAPON_PROP] end, false),
        equipSlot = safe(function() return row[EQUIP_PROP][EQUIP_SLOT_PROP] end, 0),
        maxLiquid = safe(function() return row[LIQUID_PROP][LIQUID_MAX_PROP] end, 0),
        liquids = {},
        consume = safe(function() return row[CONSUME_PROP][CONSUME_TIME] end, 0),
        itemClass = toClassPath(row[CLASS_PROP]),
    }

    safe(function()
        local arr = row[LIQUID_PROP][LIQUID_ALLOWED]
        if arr then
            for i = 1, #arr do
                table.insert(entry.liquids, arr[i])
            end
        end
    end)

    return entry
end

local function formatLiquidArray(liquids)
    if #liquids == 0 then return "{}" end
    local parts = {}
    for i = 1, #liquids do
        parts[i] = tostring(liquids[i])
    end
    return "{" .. table.concat(parts, ",") .. "}"
end

local function formatEntry(entry)
    return string.format(
        "{ name=%q, stack=%d, weapon=%s, equipSlot=%d, maxLiquid=%d, liquids=%s, consume=%g, itemClass=%q }",
        entry.name, entry.stack, tostring(entry.weapon), entry.equipSlot,
        entry.maxLiquid, formatLiquidArray(entry.liquids), entry.consume,
        entry.itemClass)
end

function dumper.dumpTables(tables, globalDt, outputPath)
    local lines = {}
    table.insert(lines, "return {")

    for _, dt in ipairs(tables) do
        local path = safe(function() return dt:GetPathName() end, "")
        if path == "" then path = tostring(dt) end
        table.insert(lines, string.format("[%q] = {", path))

        local rowNames = safe(function() return dt:GetRowNames() end, nil)
        if rowNames then
            for i = 1, #rowNames do
                local rowName = rowNames[i]
                local row = safe(function() return dt:FindRow(rowName) end, nil)
                if row and row:IsValid() then
                    local name = tostring(rowName)
                    table.insert(lines, string.format("  [%q] = %s,", name, formatEntry(dumpRow(row, name))))
                end
            end
        end

        table.insert(lines, "},")
    end

    if globalDt then
        local path = safe(function() return globalDt:GetPathName() end, "")
        if path == "" then path = tostring(globalDt) end
        table.insert(lines, string.format("[%q] = {", path))
        local rowNames = safe(function() return globalDt:GetRowNames() end, nil)
        if rowNames then
            for i = 1, #rowNames do
                local rowName = rowNames[i]
                local row = safe(function() return globalDt:FindRow(rowName) end, nil)
                if row and row:IsValid() then
                    local name = tostring(rowName)
                    table.insert(lines, string.format("  [%q] = %s,", name, formatEntry(dumpRow(row, name))))
                end
            end
        end
        table.insert(lines, "},")
    end

    table.insert(lines, "}")

    local file = io.open(outputPath, "w")
    if not file then
        print("[SA] Dump: could not write " .. outputPath .. "\n")
        return false
    end
    file:write(table.concat(lines, "\n") .. "\n")
    file:close()
    print("[SA] Dump: wrote " .. #lines .. " lines to " .. outputPath .. "\n")
    return true
end

return dumper
