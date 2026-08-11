local tweaker = {}

local GLOBAL_TABLE_PATH = "/Game/Blueprints/Items/ItemTable_Global.ItemTable_Global"

local KNOWN_TABLE_PATHS = {
    "/Game/Blueprints/Items/ItemTable_Craftables.ItemTable_Craftables",
    "/Game/Blueprints/Items/ItemTable_Deployables.ItemTable_Deployables",
    "/Game/Blueprints/Items/ItemTable_Deployables_CraftingBenches.ItemTable_Deployables_CraftingBenches",
    "/Game/Blueprints/Items/ItemTable_Deployables_Small.ItemTable_Deployables_Small",
    "/Game/Blueprints/Items/ItemTable_FoodAndGibs.ItemTable_FoodAndGibs",
    "/Game/Blueprints/Items/ItemTable_Gear.ItemTable_Gear",
    "/Game/Blueprints/Items/ItemTable_Pickups.ItemTable_Pickups",
    "/Game/Blueprints/Items/ItemTable_Plants.ItemTable_Plants",
    "/Game/Blueprints/Items/ItemTable_Weapons.ItemTable_Weapons",
}

function tweaker.discoverItemTables()
    local seen = {}
    local results = {}

    for _, path in ipairs(KNOWN_TABLE_PATHS) do
        LoadAsset(path)
        local dt = StaticFindObject(path)
        if dt and dt.FindRow then
            seen[path] = true
            table.insert(results, dt)
        end
    end

    local allTables = FindAllOf("DataTable")
    for i = 1, #allTables do
        local ok, path = pcall(function()
            local dt = allTables[i]
            if dt and dt.FindRow then
                return dt:GetPathName()
            end
            return nil
        end)
        if ok and path and not seen[path] and path:find("/Game/Blueprints/Items/ItemTable_") and not path:find("ItemTable_Global") then
            seen[path] = true
            table.insert(results, StaticFindObject(path))
        end
    end

    return results
end

function tweaker.getGlobalTable()
    LoadAsset(GLOBAL_TABLE_PATH)
    return StaticFindObject(GLOBAL_TABLE_PATH)
end

return tweaker
