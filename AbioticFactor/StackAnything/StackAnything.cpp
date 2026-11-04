#include "StackAnything.hpp"
#include "GameTypes.hpp"

#include <DynamicOutput/DynamicOutput.hpp>
#include <Constructs/Loop.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/FSoftObjectPath.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/SoftObjectPtr.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <limits>
#include <memory>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace RC::Unreal;

static bool g_debug = false;

// ---------------------------------------------------------------------------
// Reflection helpers. Game object fields and function params are resolved by
// name at runtime, so moved fields need no rebuild, and renamed or removed
// ones log an error instead of reading garbage.
// ---------------------------------------------------------------------------

// Blueprint user-defined struct members carry a suffix: "Index_5_6BDC7B3944A5DE0B319F9FA20720872F"
// (base + '_' + digits + '_' + 32 hex chars, so at least base + 35 chars).
static bool matchesBlueprintMemberName(const std::wstring& full, std::wstring_view base) {
    if (full.size() < base.size() + 35 || full.compare(0, base.size(), base) != 0 || full[base.size()] != L'_') {
        return false;
    }
    size_t i = base.size() + 1;
    const size_t digits = i;
    while (i < full.size() && std::iswdigit(full[i])) {
        ++i;
    }
    if (i == digits || i >= full.size() || full[i] != L'_' || full.size() - i - 1 != 32) {
        return false;
    }
    return std::all_of(full.begin() + static_cast<std::ptrdiff_t>(i) + 1, full.end(),
                       [](wchar_t c) { return std::iswxdigit(c) != 0; });
}

static FProperty* findProperty(UStruct* owner, std::wstring_view name) {
    if (!owner) {
        return nullptr;
    }
    FProperty* bpMatch = nullptr;
    for (FProperty* prop : TFieldRange<FProperty>(owner, EFieldIterationFlags::IncludeSuper)) {
        const std::wstring propName = prop->GetName();
        if (propName == name) {
            return prop;
        }
        if (!bpMatch && matchesBlueprintMemberName(propName, name)) {
            bpMatch = prop;
        }
    }
    return bpMatch;
}

static int32_t propertyOffset(UStruct* owner, std::wstring_view name) {
    FProperty* prop = findProperty(owner, name);
    return prop ? prop->GetOffset_Internal() : -1;
}

// Offset of a named field, cached for the last class seen. All hooked objects of
// one role share a class (or a subclass with identical layout), so this is one
// property scan per role, not per call.
namespace {
class CachedField {
public:
    explicit CachedField(const wchar_t* name) : m_name(name) {}

    template <typename T>
    T* in(UObject* obj) {
        if (!obj) {
            return nullptr;
        }
        UClass* cls = obj->GetClassPrivate();
        if (cls != m_class) {
            m_class = cls;
            m_offset = propertyOffset(cls, m_name);
            if (m_offset < 0) {
                RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Field '{}' not found on {}\n"),
                                                      m_name, cls->GetName());
            }
        }
        return m_offset < 0 ? nullptr : reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + m_offset);
    }

private:
    const wchar_t* m_name;
    UClass* m_class = nullptr;
    int32_t m_offset = -1;
};
}

// Address of a hooked function's parameter. BP out/ref params live at their
// FOutParmRec destination, not in the locals buffer, so prefer that address.
template <typename T>
static T* param(UnrealScriptFunctionCallableContext& Ctx, const wchar_t* name) {
    UFunction* fn = Ctx.TheStack.Node();
    FProperty* prop = findProperty(fn, name);
    if (!prop) {
        if (fn) {
            RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Param '{}' not found on {}\n"),
                                                  name, fn->GetName());
        }
        return nullptr;
    }
    if (void* out = FindOutParamValueAddress(Ctx.TheStack, prop)) {
        return static_cast<T*>(out);
    }
    return reinterpret_cast<T*>(Ctx.TheStack.Locals() + prop->GetOffset_Internal());
}

static UScriptStruct* findScriptStruct(const wchar_t* path) {
    return UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr, nullptr, path);
}

// ---------------------------------------------------------------------------
// Layout validation
// ---------------------------------------------------------------------------

namespace {
struct ExpectedField {
    const wchar_t* name;
    int32_t offset;
};

struct GameLayout {
    UScriptStruct* itemRowStruct = nullptr;  // /Script/AbioticFactor.AbioticInventoryItem
    int32_t rowStackSize = -1;               // FAbioticInventoryItem::StackSize
    int32_t rowEquipSlot = -1;               // FAbioticInventoryItem::EquipmentData.EquipSlot
    UScriptStruct* slotStruct = nullptr;     // /Script/AbioticFactor.AbioticInventoryItemSlot (deep copies)
    bool mirrorsValid = false;               // GameTypes.hpp mirrors match the game
};
GameLayout g_layout;
}

static bool checkStruct(const wchar_t* path, int32_t expectedSize, std::initializer_list<ExpectedField> fields) {
    UScriptStruct* s = findScriptStruct(path);
    if (!s) {
        RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Layout check: struct {} not found\n"), path);
        return false;
    }
    bool ok = true;
    if (s->GetStructureSize() != expectedSize) {
        RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Layout check: {} size 0x{:X}, expected 0x{:X}\n"),
                                              path, s->GetStructureSize(), expectedSize);
        ok = false;
    }
    for (const auto& f : fields) {
        const int32_t actual = propertyOffset(s, f.name);
        if (actual != f.offset) {
            RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Layout check: {}.{} at {}, expected 0x{:X}\n"),
                                                  path, f.name, actual, f.offset);
            ok = false;
        }
    }
    return ok;
}

#define SA_FIELD(Struct, Member) {STR(#Member), static_cast<int32_t>(offsetof(Struct, Member))}

bool initGameLayout() {
    g_layout = {};

    g_layout.itemRowStruct = findScriptStruct(STR("/Script/AbioticFactor.AbioticInventoryItem"));
    if (g_layout.itemRowStruct) {
        g_layout.rowStackSize = propertyOffset(g_layout.itemRowStruct, STR("StackSize"));
        if (auto* equip = CastField<FStructProperty>(findProperty(g_layout.itemRowStruct, STR("EquipmentData")))) {
            const int32_t slot = propertyOffset(equip->GetStruct().Get(), STR("EquipSlot"));
            if (slot >= 0) {
                g_layout.rowEquipSlot = equip->GetOffset_Internal() + slot;
            }
        }
    }
    if (g_layout.rowStackSize < 0 || g_layout.rowEquipSlot < 0) {
        RC::Output::send<RC::LogLevel::Error>(
            STR("[StackAnything] Item row layout not resolved (StackSize={}, EquipSlot={}); stack sizes unchanged\n"),
            g_layout.rowStackSize, g_layout.rowEquipSlot);
    }

    bool ok = true;
    ok &= checkStruct(STR("/Script/AbioticFactor.RowHandle"), sizeof(FRowHandle),
                      {SA_FIELD(FRowHandle, RowName)});
    ok &= checkStruct(STR("/Script/AbioticFactor.AbioticInventoryItemSlot"), sizeof(FAbioticInventoryItemSlot),
                      {SA_FIELD(FAbioticInventoryItemSlot, ItemDataTable),
                       SA_FIELD(FAbioticInventoryItemSlot, ChangeableData)});
    ok &= checkStruct(STR("/Script/AbioticFactor.AbioticInventoryChangeableData"), sizeof(FAbioticInventoryChangeableData),
                      {SA_FIELD(FAbioticInventoryChangeableData, CurrentItemDurability),
                       SA_FIELD(FAbioticInventoryChangeableData, MaxItemDurability),
                       SA_FIELD(FAbioticInventoryChangeableData, CurrentStack),
                       SA_FIELD(FAbioticInventoryChangeableData, LiquidLevel),
                       SA_FIELD(FAbioticInventoryChangeableData, CurrentLiquid),
                       SA_FIELD(FAbioticInventoryChangeableData, TextureVariantRow)});
    ok &= checkStruct(STR("/Script/AbioticFactor.AbioticItemInput"), sizeof(FAbioticItemInput),
                      {SA_FIELD(FAbioticItemInput, Count)});
    ok &= checkStruct(STR("/Script/AbioticFactor.SortedItem"), sizeof(FSortedItem),
                      {SA_FIELD(FSortedItem, RowName), SA_FIELD(FSortedItem, SourceIndex)});
    g_layout.slotStruct = findScriptStruct(STR("/Script/AbioticFactor.AbioticInventoryItemSlot"));
    ok &= g_layout.slotStruct != nullptr;
    g_layout.mirrorsValid = ok;

    if (!ok) {
        RC::Output::send<RC::LogLevel::Error>(
            STR("[StackAnything] Game struct layout changed (game update?); stack-safety hooks disabled\n"));
    }
    return ok;
}

#undef SA_FIELD

// ---------------------------------------------------------------------------
// Item tables and stack sizes
// ---------------------------------------------------------------------------

static std::vector<ConfigEntry> g_config;
static std::vector<UDataTable*> g_itemTables;
static UDataTable* g_globalItemTable = nullptr;
static std::wstring g_dumpPath; // empty = item dump disabled

static void dumpItemTables();

static bool shouldSkipRow(const uint8_t* row) {
    const uint8_t slot = row[g_layout.rowEquipSlot];
    return slot == EInventorySlotType::EquipmentSlot_Hacker || slot == EInventorySlotType::EquipmentSlot_Companion;
}

static int32_t* rowStackSize(uint8_t* row) {
    return reinterpret_cast<int32_t*>(row + g_layout.rowStackSize);
}

static bool isItemTable(UDataTable* dt) {
    return dt && g_layout.itemRowStruct && dt->GetRowStruct().Get() == g_layout.itemRowStruct;
}

static void raiseStackInTable(UDataTable* dt) {
    TArray<FName> rowNames = dt->GetRowNames();
    for (int32_t i = 0; i < rowNames.Num(); ++i) {
        const FName& name = rowNames[i];
        int maxStack = 0;
        if (!matchRowName(g_config, name.ToString(), maxStack)) {
            continue;
        }
        uint8_t* row = dt->FindRowUnchecked(name);
        if (!row || shouldSkipRow(row)) {
            continue;
        }
        if (*rowStackSize(row) <= maxStack) {
            *rowStackSize(row) = maxStack;
        }
    }
}

// Idempotent (only raises). Runs at startup and again on every player spawn,
// because official (mod.io) mods bring their own item tables
// (UAbioticModDataTable) that load with the world, after the main menu.
static void applyStackTweaksNow() {
    if (!g_layout.itemRowStruct || g_layout.rowStackSize < 0 || g_layout.rowEquipSlot < 0) {
        return;
    }

    static const wchar_t* kKnownPaths[] = {
        L"/Game/Blueprints/Items/ItemTable_Craftables.ItemTable_Craftables",
        L"/Game/Blueprints/Items/ItemTable_Deployables.ItemTable_Deployables",
        L"/Game/Blueprints/Items/ItemTable_Deployables_CraftingBenches.ItemTable_Deployables_CraftingBenches",
        L"/Game/Blueprints/Items/ItemTable_Deployables_Small.ItemTable_Deployables_Small",
        L"/Game/Blueprints/Items/ItemTable_FoodAndGibs.ItemTable_FoodAndGibs",
        L"/Game/Blueprints/Items/ItemTable_Gear.ItemTable_Gear",
        L"/Game/Blueprints/Items/ItemTable_Pickups.ItemTable_Pickups",
        L"/Game/Blueprints/Items/ItemTable_Plants.ItemTable_Plants",
        L"/Game/Blueprints/Items/ItemTable_Weapons.ItemTable_Weapons",
        L"/Game/Blueprints/Items/ItemTable_Global.ItemTable_Global",
    };
    for (const wchar_t* path : kKnownPaths) {
        (void)FSoftObjectPath(FString(path)).TryLoad();
    }
    g_globalItemTable = UObjectGlobals::StaticFindObject<UDataTable*>(
        nullptr, nullptr, L"/Game/Blueprints/Items/ItemTable_Global.ItemTable_Global");

    // Every loaded table with item rows — base and mod tables alike — instead
    // of a fixed path list. Pets keep their vanilla stacks.
    std::vector<UDataTable*> tables;
    UObjectGlobals::ForEachUObject([&](UObject* obj, int32_t, int32_t) -> RC::LoopAction {
        if (!obj->IsA<UDataTable>()) {
            return RC::LoopAction::Continue;
        }
        auto* dt = static_cast<UDataTable*>(obj);
        if (!isItemTable(dt) || dt == g_globalItemTable) {
            return RC::LoopAction::Continue;
        }
        if (obj->GetPathName().find(L"ItemTable_Pets") != std::wstring::npos) {
            return RC::LoopAction::Continue;
        }
        tables.push_back(dt);
        return RC::LoopAction::Continue;
    });

    for (UDataTable* dt : tables) {
        raiseStackInTable(dt);
    }
    if (isItemTable(g_globalItemTable)) {
        raiseStackInTable(g_globalItemTable);
    }
    g_itemTables = std::move(tables);
    dumpItemTables();

    if (g_debug) {
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[StackAnything] Applied stack size overrides across {} item tables\n"), g_itemTables.size() + 1);
    }
}

// ---------------------------------------------------------------------------
// Item dump (config `dumpItems = true`): every item row of every item table,
// written as a Lua table so users can find IDs for their stack patterns.
// ---------------------------------------------------------------------------

namespace {
struct DumpLayout {
    int32_t name = -1, weapon = -1, maxLiquid = -1, allowedLiquids = -1, consumeTime = -1, itemClass = -1;
};
}

// Offset of a field nested in struct-typed fields, e.g. {"LiquidData", "MaxLiquid"}.
static int32_t nestedOffset(UStruct* owner, std::initializer_list<const wchar_t*> path) {
    int32_t total = 0;
    size_t i = 0;
    for (const wchar_t* name : path) {
        FProperty* prop = findProperty(owner, name);
        if (!prop) {
            return -1;
        }
        total += prop->GetOffset_Internal();
        if (++i < path.size()) {
            auto* structProp = CastField<FStructProperty>(prop);
            if (!structProp) {
                return -1;
            }
            owner = structProp->GetStruct().Get();
        }
    }
    return total;
}

static void appendUtf8(std::string& out, std::wstring_view text) {
    for (size_t i = 0; i < text.size(); ++i) {
        uint32_t cp = text[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (text[++i] - 0xDC00);
        }
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
}

// Lua %q-style quoted string.
static void appendLuaString(std::string& out, std::wstring_view text) {
    out += '"';
    std::wstring escaped;
    escaped.reserve(text.size());
    for (wchar_t c : text) {
        switch (c) {
            case L'"': escaped += L"\\\""; break;
            case L'\\': escaped += L"\\\\"; break;
            case L'\n': escaped += L"\\n"; break;
            case L'\r': escaped += L"\\r"; break;
            case L'\t': escaped += L"\\t"; break;
            default:
                if (c < 0x20) {
                    wchar_t code[8];
                    std::swprintf(code, 8, L"\\%03d", static_cast<int>(c)); // Lua \ddd, padded
                    escaped += code;
                } else {
                    escaped += c;
                }
        }
    }
    appendUtf8(out, escaped);
    out += '"';
}

static void dumpTable(std::string& out, UDataTable* dt, const DumpLayout& l) {
    out += "  [";
    appendLuaString(out, dt->GetPathName());
    out += "] = {\n";
    TArray<FName> rowNames = dt->GetRowNames();
    for (int32_t i = 0; i < rowNames.Num(); ++i) {
        uint8_t* row = dt->FindRowUnchecked(rowNames[i]);
        if (!row) {
            continue;
        }
        out += "    [";
        appendLuaString(out, rowNames[i].ToString());
        out += "] = { name=";
        appendLuaString(out, l.name >= 0 ? reinterpret_cast<const FText*>(row + l.name)->ToString() : std::wstring{});
        out += ", stack=" + std::to_string(*rowStackSize(row));
        out += std::string(", weapon=") + (l.weapon >= 0 && row[l.weapon] ? "true" : "false");
        out += ", equipSlot=" + std::to_string(row[g_layout.rowEquipSlot]);
        out += ", maxLiquid=" + std::to_string(l.maxLiquid >= 0 ? *reinterpret_cast<const int32_t*>(row + l.maxLiquid) : 0);
        out += ", liquids={";
        if (l.allowedLiquids >= 0) {
            const auto& liquids = *reinterpret_cast<const TArray<uint8_t>*>(row + l.allowedLiquids);
            for (int32_t j = 0; j < liquids.Num(); ++j) {
                out += (j ? "," : "") + std::to_string(liquids[j]);
            }
        }
        out += "}, consume=";
        char consume[32];
        std::snprintf(consume, sizeof(consume), "%g",
                      l.consumeTime >= 0 ? *reinterpret_cast<const double*>(row + l.consumeTime) : 0.0);
        out += consume;
        out += ", itemClass=";
        if (l.itemClass >= 0) {
            const FString path = reinterpret_cast<const FSoftObjectPtr*>(row + l.itemClass)->ToString();
            appendLuaString(out, *path ? std::wstring_view(*path) : std::wstring_view{});
        } else {
            out += "\"\"";
        }
        out += " },\n";
    }
    out += "  },\n";
}

static void dumpItemTables() {
    if (g_dumpPath.empty()) {
        return;
    }
    UScriptStruct* rowStruct = g_layout.itemRowStruct;
    const DumpLayout l{
        .name = nestedOffset(rowStruct, {STR("ItemName")}),
        .weapon = nestedOffset(rowStruct, {STR("IsWeapon")}),
        .maxLiquid = nestedOffset(rowStruct, {STR("LiquidData"), STR("MaxLiquid")}),
        .allowedLiquids = nestedOffset(rowStruct, {STR("LiquidData"), STR("AllowedLiquids")}),
        .consumeTime = nestedOffset(rowStruct, {STR("ConsumableData"), STR("TimeToConsume")}),
        .itemClass = nestedOffset(rowStruct, {STR("ItemClass")}),
    };

    std::string out = "-- StackAnything item dump: [table path] = { [item ID] = {...} }.\n"
                      "-- Item IDs (the keys) are what config.lua patterns match. stack = current max stack.\n"
                      "return {\n";
    for (UDataTable* dt : g_itemTables) {
        dumpTable(out, dt, l);
    }
    if (isItemTable(g_globalItemTable)) {
        dumpTable(out, g_globalItemTable, l);
    }
    out += "}\n";

    std::ofstream file(g_dumpPath, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(out.data(), static_cast<std::streamsize>(out.size()))) {
        RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Item dump: could not write {}\n"), g_dumpPath);
        return;
    }
    RC::Output::send<RC::LogLevel::Verbose>(STR("[StackAnything] Item dump: {} tables written to {}\n"),
                                            g_itemTables.size() + (g_globalItemTable ? 1 : 0), g_dumpPath);
}

void applyStackTweaks(const std::vector<ConfigEntry>& config, const std::wstring& dumpPath) {
    g_config = config;
    g_dumpPath = dumpPath;
    applyStackTweaksNow();
}

static bool isEmptyRowName(const FName& rn) {
    if (rn.IsNone()) {
        return true;
    }
    const std::wstring s = rn.ToString();
    return s.empty() || s == L"None" || s == L"Empty";
}

// Item row by name. ItemTable_Global holds every item; the per-category
// tables (including mod tables) are the fallback.
static uint8_t* findItemRow(const FName& rowName) {
    if (isEmptyRowName(rowName)) {
        return nullptr;
    }
    if (g_globalItemTable) {
        if (uint8_t* row = g_globalItemTable->FindRowUnchecked(rowName)) {
            return row;
        }
    }
    for (UDataTable* dt : g_itemTables) {
        if (uint8_t* row = dt->FindRowUnchecked(rowName)) {
            return row;
        }
    }
    return nullptr;
}

static int32_t getMaxStack(const FAbioticInventoryItemSlot& slot) {
    if (g_layout.rowStackSize < 0) {
        return 0;
    }
    uint8_t* row = findItemRow(slot.ItemDataTable.RowName);
    return row ? *rowStackSize(row) : 0;
}

// ---------------------------------------------------------------------------
// Game object accessors
// ---------------------------------------------------------------------------

static CachedField g_fCurrentInventory{STR("CurrentInventory")};
static CachedField g_fCharacterInventory{STR("CharacterInventory")};
static CachedField g_fCharacterHotbarInventory{STR("CharacterHotbarInventory")};
static CachedField g_fLastSelectedHotbarItem{STR("Server_LastSelectedHotbarItem")};
static CachedField g_fLiquidFillLevel{STR("Liquid_FillLevel")};
static CachedField g_fLiquidMaxFill{STR("Liquid_MaxFill")};
static CachedField g_fInfiniteSource{STR("InfiniteSource")};
static CachedField g_fUpgradeItemInventory{STR("UpgradeItemInventory")};
static CachedField g_fItemUpgradeEntry{STR("ItemUpgradeEntry")};
static CachedField g_fBenchRef{STR("BenchRef")};

static TArray<FAbioticInventoryItemSlot>* slotsOf(UAbiotic_InventoryComponent_C* inv) {
    return g_fCurrentInventory.in<TArray<FAbioticInventoryItemSlot>>(reinterpret_cast<UObject*>(inv));
}

static UAbiotic_InventoryComponent_C* inventoryField(CachedField& field, UObject* owner) {
    auto** ptr = field.in<UAbiotic_InventoryComponent_C*>(owner);
    return ptr ? *ptr : nullptr;
}

namespace {
struct PendingRefund {
    AAbiotic_PlayerCharacter_C* player;
    int32_t liquidType;
    int32_t amount;
};
struct PendingDrain {
    AAbiotic_PlayerCharacter_C* player;
    UAbiotic_InventoryComponent_C* inventory;
    int32_t slotIndex;
    int32_t preLiquid;
    int32_t liquidType;
};
PendingRefund g_pendingRefund{};
PendingDrain g_pendingDrain{};

bool isSlotEmpty(const FAbioticInventoryItemSlot& slot) {
    return isEmptyRowName(slot.ItemDataTable.RowName);
}

struct ScaledUpgradeCostArray {
    TArray<FAbioticItemInput>* array;
    const void* data;
    std::vector<int32_t> originalCounts;
};

struct UpgradeCostState {
    int32_t multiplier = 1;
    UObject* player = nullptr;
    std::vector<ScaledUpgradeCostArray> scaledArrays;
};

}

static UpgradeCostState g_upgradeCostState;

static bool isEnergyLiquidType(const int32_t t) {
    return t == ELiquidType::Battery || t == ELiquidType::Laser;
}

static int32_t getUpgradeBatchSize(UObject* bench) {
    auto* slots = slotsOf(inventoryField(g_fUpgradeItemInventory, bench));
    if (!slots) {
        return 1;
    }
    for (int32_t i = 0; i < slots->Num(); ++i) {
        if (!isSlotEmpty((*slots)[i])) {
            return std::max(1, (*slots)[i].ChangeableData.CurrentStack);
        }
    }
    return 1;
}

static int32_t scaleCostCount(int32_t count, int32_t multiplier) {
    if (count <= 0 || multiplier <= 1) {
        return count;
    }
    const int64_t scaled = static_cast<int64_t>(count) * multiplier;
    return static_cast<int32_t>(std::min<int64_t>(
        scaled, std::numeric_limits<int32_t>::max()));
}

static void clearUpgradeCostState() {
    for (auto& scaled : g_upgradeCostState.scaledArrays) {
        if (!scaled.array || scaled.array->GetData() != scaled.data) {
            continue;
        }
        const int32_t count = std::min<int32_t>(
            scaled.array->Num(),
            static_cast<int32_t>(scaled.originalCounts.size()));
        for (int32_t i = 0; i < count; ++i) {
            (*scaled.array)[i].Count = scaled.originalCounts[i];
        }
    }
    g_upgradeCostState = {};
}

static void scaleUpgradeCostArray(TArray<FAbioticItemInput>& items, int32_t multiplier) {
    if (multiplier <= 1 || items.Num() <= 0 || !items.GetData()) {
        return;
    }

    const void* data = items.GetData();
    const auto alreadyScaled = std::ranges::find_if(
        g_upgradeCostState.scaledArrays,
        [data](const ScaledUpgradeCostArray& scaled) { return scaled.data == data; });
    if (alreadyScaled != g_upgradeCostState.scaledArrays.end()) {
        return;
    }

    ScaledUpgradeCostArray scaled{
        .array = &items,
        .data = data,
        .originalCounts = {},
    };
    scaled.originalCounts.reserve(items.Num());
    for (int32_t i = 0; i < items.Num(); ++i) {
        scaled.originalCounts.push_back(items[i].Count);
        items[i].Count = scaleCostCount(items[i].Count, multiplier);
    }
    g_upgradeCostState.scaledArrays.push_back(std::move(scaled));
    if (g_debug) {
        RC::Output::send<RC::LogLevel::Verbose>(
        STR("[SA][UPGRADE] scaled {} costs by {}\n"), items.Num(), multiplier);
    }
}

namespace {
struct SplitTarget {
    UAbiotic_InventoryComponent_C* inventory;
    int32_t index;
    bool mergeInto;
};

}

static bool sameString(const FString& a, const FString& b);
static std::wstring getVariantName(const FAbioticInventoryChangeableData& c);

// Liquid level for stacking comparisons. Weapon-fired projectiles carry the class
// default LiquidLevel -1 (no liquid) until the dropped item's BeginPlay resets it
// to 0, and an instant auto-pickup (ProjectilePickup perk) can run before that:
// treat "-1, no liquid" as the "0, no liquid" every inventory item has.
static int32_t effectiveLiquidLevel(const FAbioticInventoryChangeableData& c) {
    return c.CurrentLiquid == ELiquidType::Empty && c.LiquidLevel < 0 ? 0 : c.LiquidLevel;
}

// A slot that can take `amountToAdd` items shaped like `item`: a matching stack
// (same row, liquid, soup recipe and texture variant) with room, else an empty slot.
static SplitTarget findSplitTarget(AAbiotic_PlayerCharacter_C* player,
                                   UAbiotic_InventoryComponent_C* givenInventory,
                                   const FAbioticInventoryItemSlot& item,
                                   int32_t amountToAdd, UAbiotic_InventoryComponent_C* skipInventory,
                                   int32_t skipIndex, bool mergeOnly = false) {
    const FName& rowName = item.ItemDataTable.RowName;
    const FAbioticInventoryChangeableData& want = item.ChangeableData;
    std::vector<UAbiotic_InventoryComponent_C*> candidates;
    if (givenInventory) {
        candidates.push_back(givenInventory);
    }
    if (player) {
        auto* playerObj = reinterpret_cast<UObject*>(player);
        if (auto* inv = inventoryField(g_fCharacterInventory, playerObj)) {
            candidates.push_back(inv);
        }
        if (auto* inv = inventoryField(g_fCharacterHotbarInventory, playerObj)) {
            candidates.push_back(inv);
        }
    }

    for (UAbiotic_InventoryComponent_C* inv : candidates) {
        auto* slots = slotsOf(inv);
        if (!slots) {
            continue;
        }
        for (int32_t i = 0; i < slots->Num(); ++i) {
            if (skipInventory && skipIndex >= 0 && inv == skipInventory && i == skipIndex) {
                continue;
            }
            FAbioticInventoryItemSlot& slot = (*slots)[i];
            if (isSlotEmpty(slot) || slot.ItemDataTable.RowName != rowName) {
                continue;
            }
            FAbioticInventoryChangeableData& c = slot.ChangeableData;
            const int32_t maxStack = getMaxStack(slot);
            if (maxStack > 0 && c.CurrentStack > 0 && c.CurrentStack + amountToAdd <= maxStack &&
                effectiveLiquidLevel(c) == effectiveLiquidLevel(want) && c.CurrentLiquid == want.CurrentLiquid &&
                sameString(c.PlayerMadeString, want.PlayerMadeString) &&
                getVariantName(c) == getVariantName(want)) {
                return {.inventory = inv, .index = i, .mergeInto = true};
            }
        }
    }

    if (mergeOnly) {
        return {.inventory = nullptr, .index = -1, .mergeInto = false};
    }

    for (UAbiotic_InventoryComponent_C* inv : candidates) {
        auto* slots = slotsOf(inv);
        if (!slots) {
            continue;
        }
        for (int32_t i = 0; i < slots->Num(); ++i) {
            if (skipInventory && skipIndex >= 0 && inv == skipInventory && i == skipIndex) {
                continue;
            }
            if (isSlotEmpty((*slots)[i])) {
                return {.inventory = inv, .index = i, .mergeInto = false};
            }
        }
    }
    return {.inventory = nullptr, .index = -1, .mergeInto = false};
}

static void refreshInventory(UAbiotic_InventoryComponent_C* inv) {
    if (!inv) {
        return;
    }
    auto* obj = reinterpret_cast<UObject*>(inv);
    if (auto* fn = obj->GetFunctionByNameInChain(STR("OnRep_CurrentInventory"))) {
        obj->ProcessEvent(fn, nullptr);
    }
}

// Writes a POD param into a ProcessEvent buffer by param name.
template <typename T>
static bool setParam(UFunction* fn, std::vector<uint8_t>& buffer, const wchar_t* name, const T& value) {
    FProperty* prop = findProperty(fn, name);
    if (!prop || prop->GetOffset_Internal() + sizeof(T) > buffer.size()) {
        RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Param '{}' not found on {}\n"), name, fn->GetName());
        return false;
    }
    std::memcpy(buffer.data() + prop->GetOffset_Internal(), &value, sizeof(T));
    return true;
}

static void refundSourceContainer(UObject* source, AAbiotic_PlayerCharacter_C* player,
                                  int32_t liquidType, int32_t amount) {
    if (!source || !player || amount <= 0) {
        return;
    }
    auto* infinite = g_fInfiniteSource.in<bool>(source);
    auto* fillLevel = g_fLiquidFillLevel.in<int32_t>(source);
    auto* maxFill = g_fLiquidMaxFill.in<int32_t>(source);
    if (!infinite || !fillLevel || !maxFill || *infinite) {
        return;
    }
    const int32_t current = *fillLevel;
    const int32_t newLevel = std::min(current + amount, *maxFill);
    if (newLevel == current) {
        return;
    }
    UFunction* fn = source->GetFunctionByNameInChain(STR("Server_ModifyFillState"));
    if (!fn) {
        return;
    }
    std::vector<uint8_t> buffer(static_cast<size_t>(fn->GetStructureSize()), 0);
    if (setParam(fn, buffer, STR("Liquid"), static_cast<uint8_t>(liquidType)) &&
        setParam(fn, buffer, STR("NewLiquidValue"), newLevel) &&
        setParam(fn, buffer, STR("SkipSave"), false) &&
        setParam(fn, buffer, STR("FillInstigator"), player)) {
        source->ProcessEvent(fn, buffer.data());
    }
}

static bool sameString(const FString& a, const FString& b) {
    const int32_t len = a.Len();
    return len == b.Len() && (len == 0 || std::wmemcmp(*a, *b, static_cast<size_t>(len)) == 0);
}

// Vanilla gives every item that lands in an empty slot its own AssetID (a GUID
// in EGuidFormats::Digits form). The stove finds held cookware by AssetID and
// never matches "-1", so a split-off single needs a fresh one.
static FString newAssetId() {
    static std::mt19937_64 rng{std::random_device{}()};
    wchar_t buf[33];
    const uint64_t hi = rng();
    const uint64_t lo = rng();
    std::swprintf(buf, 33, L"%016llX%016llX", static_cast<unsigned long long>(hi), static_cast<unsigned long long>(lo));
    return FString(static_cast<const TCHAR*>(buf));
}

// Deep copy of an inventory slot (FStrings, tag and property arrays), owned by
// the engine allocator through the slot's UScriptStruct.
class SlotCopy {
public:
    SlotCopy() = default;
    SlotCopy(const SlotCopy&) = delete;
    SlotCopy& operator=(const SlotCopy&) = delete;
    SlotCopy(SlotCopy&& o) noexcept : m_storage(std::move(o.m_storage)) {}
    ~SlotCopy() { reset(); }

    bool capture(const FAbioticInventoryItemSlot& slot) {
        reset();
        if (!g_layout.slotStruct) {
            return false;
        }
        m_storage = std::make_unique<uint64_t[]>((sizeof(FAbioticInventoryItemSlot) + 7) / 8);
        g_layout.slotStruct->InitializeStruct(m_storage.get());
        g_layout.slotStruct->CopyScriptStruct(m_storage.get(), &slot);
        return true;
    }
    const FAbioticInventoryItemSlot* get() const {
        return reinterpret_cast<const FAbioticInventoryItemSlot*>(m_storage.get());
    }
    FAbioticInventoryItemSlot* get() {
        return reinterpret_cast<FAbioticInventoryItemSlot*>(m_storage.get());
    }
    void reset() {
        if (m_storage && g_layout.slotStruct) {
            g_layout.slotStruct->DestroyStruct(m_storage.get());
        }
        m_storage.reset();
    }

private:
    std::unique_ptr<uint64_t[]> m_storage;
};

// Moves `count` items shaped like `item` into targetSlot (merge, or a new stack
// that is a full copy of `item` with its own AssetID).
static void moveInto(FAbioticInventoryItemSlot& targetSlot, bool mergeInto,
                     const FAbioticInventoryItemSlot& item, int32_t count) {
    if (mergeInto) {
        targetSlot.ChangeableData.CurrentStack += count;
        return;
    }
    g_layout.slotStruct->CopyScriptStruct(&targetSlot, &item);
    targetSlot.ChangeableData.CurrentStack = count;
    targetSlot.ChangeableData.AssetID = newAssetId();
}

// Liquid edits through Server_TryChangeValueInLiquidContainer apply to the whole
// slot, so on a stack they fill, drain or flavor every item at once. The pre-hook
// snapshots the slot; the post-hook leaves the one changed item in the slot and
// moves the rest, unchanged, to another stack. The slot must keep the changed
// item: vanilla also wrote the new liquid state into the held item actor
// (ItemInHand_BP), and the next drink/fill computes from that copy.
static void syncHeldItem(UObject* player, UAbiotic_InventoryComponent_C* inventory, int32_t slotIndex);

static int32_t g_tryFillFromDepth = 0; // > 0 while TryFill_FROM_PlayerContainer runs

namespace {
struct LiquidEditFrame {
    UAbiotic_InventoryComponent_C* inventory = nullptr;
    int32_t slotIndex = -1;
    SlotCopy before;
};
std::vector<LiquidEditFrame> g_liquidEdits;
}

static void pre_TryChangeValueInLiquidContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    LiquidEditFrame frame;
    auto** inventoryParam = param<UAbiotic_InventoryComponent_C*>(Ctx, STR("Inventory"));
    auto* slotIndexParam = param<int32_t>(Ctx, STR("SlotIndex"));
    if (inventoryParam && *inventoryParam && slotIndexParam && *slotIndexParam >= 0) {
        auto* slots = slotsOf(*inventoryParam);
        if (slots && *slotIndexParam < slots->Num()) {
            const FAbioticInventoryItemSlot& slot = (*slots)[*slotIndexParam];
            if (!isSlotEmpty(slot) && slot.ChangeableData.CurrentStack > 1 && frame.before.capture(slot)) {
                frame.inventory = *inventoryParam;
                frame.slotIndex = *slotIndexParam;
            }
        }
    }
    g_liquidEdits.push_back(std::move(frame)); // always push: the post-hook always pops
}

static void post_TryChangeValueInLiquidContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_liquidEdits.empty()) {
        return;
    }
    LiquidEditFrame frame = std::move(g_liquidEdits.back());
    g_liquidEdits.pop_back();

    auto* newLiquidParam = param<int32_t>(Ctx, STR("NewLiquidValue"));
    auto* liquidTypeParam = param<uint8_t>(Ctx, STR("LiquidType"));
    if (!frame.inventory || !newLiquidParam || !liquidTypeParam) {
        return;
    }
    auto* player = reinterpret_cast<AAbiotic_PlayerCharacter_C*>(Ctx.Context);
    UAbiotic_InventoryComponent_C* inventory = frame.inventory;
    const int32_t slotIndex = frame.slotIndex;
    const int32_t newLiquidValue = *newLiquidParam;
    const int32_t liquidType = *liquidTypeParam;
    g_pendingRefund = {};

    auto* slots = slotsOf(inventory);
    if (!slots || slotIndex >= slots->Num()) {
        return;
    }
    const FAbioticInventoryItemSlot& before = *frame.before.get();
    const int32_t stack = before.ChangeableData.CurrentStack;
    FAbioticInventoryItemSlot& sourceSlot = (*slots)[slotIndex];
    FAbioticInventoryChangeableData& changeable = sourceSlot.ChangeableData;

    // Item.Pot rows are swapped for one empty pot (CurrentStack = 1): move the
    // other stack-1 pots, unchanged, to another slot.
    if (sourceSlot.ItemDataTable.RowName != before.ItemDataTable.RowName) {
        if (isSlotEmpty(sourceSlot)) {
            return;
        }
        SplitTarget target = findSplitTarget(player, inventory, before, stack - 1, inventory, slotIndex);
        if (!target.inventory) {
            // No room: keep the pots and give up the converted single.
            g_layout.slotStruct->CopyScriptStruct(&sourceSlot, &before);
            changeable.CurrentStack = stack - 1;
            refreshInventory(inventory);
            RC::Output::send<RC::LogLevel::Warning>(
                STR("[StackAnything] No free slot for the rest of a pot stack; converted pot discarded\n"));
            return;
        }
        moveInto((*slotsOf(target.inventory))[target.index], target.mergeInto, before, stack - 1);
        refreshInventory(inventory);
        if (target.inventory != inventory) {
            refreshInventory(target.inventory);
        }
        return;
    }

    if (isEnergyLiquidType(liquidType) || isEnergyLiquidType(before.ChangeableData.CurrentLiquid)) {
        return;
    }
    if (changeable.LiquidLevel == before.ChangeableData.LiquidLevel &&
        changeable.CurrentLiquid == before.ChangeableData.CurrentLiquid &&
        sameString(changeable.PlayerMadeString, before.ChangeableData.PlayerMadeString)) {
        return; // nothing changed
    }

    // g_pendingDrain is set by every IsHoldingLiquidContainer call (looking at a
    // sink, barrel or stove) and can be stale; only a drain that really runs
    // inside TryFill_FROM_PlayerContainer is left to that function's post-hook.
    if (g_tryFillFromDepth > 0 && g_pendingDrain.player == player &&
        g_pendingDrain.inventory == inventory && g_pendingDrain.slotIndex == slotIndex) {
        if (newLiquidValue < g_pendingDrain.preLiquid) {
            return; // drain direction: handled in TryFill_FROM_PlayerContainer post-hook
        }
        g_pendingDrain = {};
    }

    const bool filling = effectiveLiquidLevel(changeable) > effectiveLiquidLevel(before.ChangeableData);
    if (filling) {
        // A filled item joins an existing stack of the same liquid/soup if one has
        // room; the rest stays in hand as it was, and the held actor is resynced.
        SplitTarget same = findSplitTarget(player, inventory, sourceSlot, 1, inventory, slotIndex, true);
        if (same.inventory) {
            moveInto((*slotsOf(same.inventory))[same.index], true, sourceSlot, 1);
            changeable.CurrentStack = stack - 1;
            changeable.LiquidLevel = before.ChangeableData.LiquidLevel;
            changeable.CurrentLiquid = before.ChangeableData.CurrentLiquid;
            changeable.PlayerMadeString = before.ChangeableData.PlayerMadeString;
            refreshInventory(inventory);
            if (same.inventory != inventory) {
                refreshInventory(same.inventory);
            }
            syncHeldItem(reinterpret_cast<UObject*>(player), inventory, slotIndex);
            return;
        }
    }
    SplitTarget target = findSplitTarget(player, inventory, before, stack - 1, inventory, slotIndex);
    if (!target.inventory) {
        if (!filling) {
            return; // drinking/eating with a full inventory: keep vanilla (whole stack reduced), never duplicate
        }
        // Filling with a full inventory: undo the fill and refund the source.
        const int32_t filled = effectiveLiquidLevel(changeable) - effectiveLiquidLevel(before.ChangeableData);
        changeable.LiquidLevel = before.ChangeableData.LiquidLevel;
        changeable.CurrentLiquid = before.ChangeableData.CurrentLiquid;
        changeable.PlayerMadeString = before.ChangeableData.PlayerMadeString;
        refreshInventory(inventory);
        syncHeldItem(reinterpret_cast<UObject*>(player), inventory, slotIndex);
        g_pendingRefund = {
            .player = player,
            .liquidType = liquidType,
            .amount = filled,
        };
        return;
    }

    moveInto((*slotsOf(target.inventory))[target.index], target.mergeInto, before, stack - 1);
    changeable.CurrentStack = 1;

    refreshInventory(inventory);
    if (target.inventory != inventory) {
        refreshInventory(target.inventory);
    }
}

// Server_LastSelectedHotbarItem is the BP struct InventorySlotSelected_Struct,
// whose member names carry GUID suffixes; resolve its two offsets once.
static bool readSelectedSlot(UObject* player, UAbiotic_InventoryComponent_C*& inventory, int32_t& index) {
    auto* selected = g_fLastSelectedHotbarItem.in<uint8_t>(player);
    if (!selected) {
        return false;
    }
    static int32_t s_inventoryOffset = -2;
    static int32_t s_indexOffset = -2;
    if (s_inventoryOffset == -2) {
        auto* prop = CastField<FStructProperty>(findProperty(player->GetClassPrivate(), STR("Server_LastSelectedHotbarItem")));
        UScriptStruct* s = prop ? prop->GetStruct().Get() : nullptr;
        s_inventoryOffset = propertyOffset(s, STR("Inventory"));
        s_indexOffset = propertyOffset(s, STR("Index"));
        if (s_inventoryOffset < 0 || s_indexOffset < 0) {
            RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] InventorySlotSelected_Struct layout not resolved\n"));
        }
    }
    if (s_inventoryOffset < 0 || s_indexOffset < 0) {
        return false;
    }
    inventory = *reinterpret_cast<UAbiotic_InventoryComponent_C**>(selected + s_inventoryOffset);
    index = *reinterpret_cast<int32_t*>(selected + s_indexOffset);
    return true;
}

static CachedField g_fItemInHand{STR("ItemInHand_BP")};
static CachedField g_fItemChangeableData{STR("ChangeableData")};

// Copies the slot's liquid state into the held item actor when that slot is the
// one in hand, so the actor's next drink/fill starts from the slot's real value.
static void syncHeldItem(UObject* player, UAbiotic_InventoryComponent_C* inventory, int32_t slotIndex) {
    UAbiotic_InventoryComponent_C* heldInventory = nullptr;
    int32_t heldIndex = -1;
    if (!player || !readSelectedSlot(player, heldInventory, heldIndex) ||
        heldInventory != inventory || heldIndex != slotIndex) {
        return;
    }
    auto** actor = g_fItemInHand.in<UObject*>(player);
    auto* slots = slotsOf(inventory);
    if (!actor || !*actor || !slots || slotIndex >= slots->Num()) {
        return;
    }
    auto* data = g_fItemChangeableData.in<FAbioticInventoryChangeableData>(*actor);
    if (!data) {
        return;
    }
    const FAbioticInventoryChangeableData& slot = (*slots)[slotIndex].ChangeableData;
    data->LiquidLevel = slot.LiquidLevel;
    data->CurrentLiquid = slot.CurrentLiquid;
    data->PlayerMadeString = slot.PlayerMadeString;
}

static void post_IsHoldingLiquidContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_pendingDrain = {};
    UObject* player = Ctx.Context;
    if (!player) {
        return;
    }
    UAbiotic_InventoryComponent_C* inventory = nullptr;
    int32_t index = -1;
    if (!readSelectedSlot(player, inventory, index) || !inventory || index < 0) {
        return;
    }
    auto* fillAddr = param<int32_t>(Ctx, STR("CurrentFillAmount"));
    auto* ltypeAddr = param<uint8_t>(Ctx, STR("CurrentLiquidType"));
    g_pendingDrain = {
        .player = reinterpret_cast<AAbiotic_PlayerCharacter_C*>(player),
        .inventory = inventory,
        .slotIndex = index,
        .preLiquid = fillAddr ? *fillAddr : 0,
        .liquidType = ltypeAddr ? static_cast<int32_t>(*ltypeAddr) : 0,
    };
}

static void post_TryFill_TO_PlayerContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    auto* onlyCheck = param<bool>(Ctx, STR("OnlyCheck"));
    auto** playerParam = param<AAbiotic_PlayerCharacter_C*>(Ctx, STR("PlayerCharacter"));
    if (!onlyCheck || !playerParam || *onlyCheck) {
        g_pendingRefund = {};
        return;
    }
    if (!g_pendingRefund.player) {
        return;
    }
    if (*playerParam != g_pendingRefund.player || !Ctx.Context) {
        g_pendingRefund = {};
        return;
    }
    const PendingRefund refund = g_pendingRefund;
    g_pendingRefund = {};
    refundSourceContainer(Ctx.Context, refund.player, refund.liquidType, refund.amount);
}

static void pre_TryFill_FROM_PlayerContainer(UnrealScriptFunctionCallableContext&, void*) {
    ++g_tryFillFromDepth;
}

static void post_TryFill_FROM_PlayerContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_tryFillFromDepth > 0) {
        --g_tryFillFromDepth;
    }
    auto* onlyCheck = param<bool>(Ctx, STR("OnlyCheck"));
    auto** playerParam = param<AAbiotic_PlayerCharacter_C*>(Ctx, STR("PlayerCharacter"));
    if (!onlyCheck || !playerParam || *onlyCheck) {
        g_pendingDrain = {};
        return;
    }
    if (!g_pendingDrain.player) {
        return;
    }
    if (*playerParam != g_pendingDrain.player) {
        g_pendingDrain = {};
        return;
    }
    const PendingDrain drain = g_pendingDrain;
    g_pendingDrain = {};

    if (!drain.inventory || drain.slotIndex < 0 || drain.preLiquid <= 0) {
        return;
    }
    if (isEnergyLiquidType(drain.liquidType)) {
        return;
    }
    auto* slots = slotsOf(drain.inventory);
    if (!slots || drain.slotIndex >= slots->Num()) {
        return;
    }
    FAbioticInventoryItemSlot& sourceSlot = (*slots)[drain.slotIndex];
    FAbioticInventoryChangeableData& changeable = sourceSlot.ChangeableData;
    const int32_t stack = changeable.CurrentStack;
    if (stack <= 1 || changeable.LiquidLevel >= drain.preLiquid) {
        return;
    }

    // The drained single stays in hand (the held actor already has its state);
    // the rest, still at the pre-drain level, moves to another stack.
    SlotCopy rest;
    if (!rest.capture(sourceSlot)) {
        return;
    }
    rest.get()->ChangeableData.LiquidLevel = drain.preLiquid;
    rest.get()->ChangeableData.CurrentLiquid = static_cast<ELiquidType::Type>(drain.liquidType);
    SplitTarget target = findSplitTarget(drain.player, drain.inventory, *rest.get(), stack - 1,
                                         drain.inventory, drain.slotIndex);
    if (!target.inventory) {
        return;
    }

    moveInto((*slotsOf(target.inventory))[target.index], target.mergeInto, *rest.get(), stack - 1);
    changeable.CurrentStack = 1;

    refreshInventory(drain.inventory);
    if (target.inventory != drain.inventory) {
        refreshInventory(target.inventory);
    }
}

static std::wstring getVariantName(const FAbioticInventoryChangeableData& c) {
    const FName rn = c.TextureVariantRow.RowName;
    return isEmptyRowName(rn) ? std::wstring{} : rn.ToString();
}

static void post_AreItemsStackable(UnrealScriptFunctionCallableContext& Ctx, void*) {
    auto* slot1 = param<FAbioticInventoryItemSlot>(Ctx, STR("Item1"));
    auto* slot2 = param<FAbioticInventoryItemSlot>(Ctx, STR("Item2"));
    if (!slot1 || !slot2) {
        return;
    }
    const FAbioticInventoryChangeableData& c1 = slot1->ChangeableData;
    const FAbioticInventoryChangeableData& c2 = slot2->ChangeableData;
    const int32_t l1 = c1.CurrentLiquid;
    const int32_t l2 = c2.CurrentLiquid;
    if (isEnergyLiquidType(l1) || isEnergyLiquidType(l2)) {
        return; // batteries/lasers keep vanilla stack-charge behavior
    }
    if (l1 != l2 || effectiveLiquidLevel(c1) != effectiveLiquidLevel(c2)) {
        Ctx.SetReturnValue(false);
        return;
    }
    if (getVariantName(c1) != getVariantName(c2) || !sameString(c1.PlayerMadeString, c2.PlayerMadeString)) {
        Ctx.SetReturnValue(false); // texture variants and soup recipes stay apart
    }
}

static UAbiotic_InventoryComponent_C* g_sortInventory = nullptr;

static void post_CreateSortedItemData(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_sortInventory = reinterpret_cast<UAbiotic_InventoryComponent_C*>(Ctx.Context);
}

static void pre_SortInventoryArray(UnrealScriptFunctionCallableContext& Ctx, void*) {
    UAbiotic_InventoryComponent_C* inv = g_sortInventory;
    g_sortInventory = nullptr;
    auto* slots = slotsOf(inv);
    if (!slots) {
        return;
    }
    auto& p = Ctx.GetParams<FSortInventoryArray_Params>();
    TArray<FSortedItem>& arr = p.InventoryArray;

    for (int32_t i = 0; i < arr.Num(); ++i) {
        FSortedItem& item = arr[i];
        if (item.SourceIndex < 0 || item.SourceIndex >= slots->Num()) {
            continue;
        }
        FAbioticInventoryChangeableData& c = (*slots)[item.SourceIndex].ChangeableData;
        std::wstring suffix;
        const int32_t ltype = c.CurrentLiquid;
        const int32_t lvl = c.LiquidLevel;
        if (lvl > 0 && !isEnergyLiquidType(ltype)) {
            suffix += L"_L" + std::to_wstring(ltype) + L"_" + std::to_wstring(lvl);
        }
        const std::wstring variant = getVariantName(c);
        if (!variant.empty()) {
            suffix += L"_V" + variant;
        }
        if (!suffix.empty()) {
            item.RowName = FName((item.RowName.ToString() + suffix).c_str());
        }
    }
}

static void pre_IsInventoryFull(UnrealScriptFunctionCallableContext& Ctx, void*) {
    auto* ignoreStackSize = param<bool>(Ctx, STR("IgnoreStackSize"));
    if (!ignoreStackSize || *ignoreStackSize || !Ctx.Context) {
        return;
    }

    constexpr wchar_t suffix[] = L".UpgradeItemInventory";
    constexpr size_t suffixLength = sizeof(suffix) / sizeof(wchar_t) - 1;
    const std::wstring inventoryPath = Ctx.Context->GetPathName();
    if (inventoryPath.size() < suffixLength ||
        inventoryPath.compare(inventoryPath.size() - suffixLength,
                              suffixLength, suffix) != 0) {
        return;
    }

    *ignoreStackSize = true;
}

static void pre_Server_TryCraftUpgrade(UnrealScriptFunctionCallableContext& Ctx, void*) {
    clearUpgradeCostState();
    auto** player = param<UObject*>(Ctx, STR("Player"));
    g_upgradeCostState.player = player ? *player : nullptr;
    const int32_t batch = getUpgradeBatchSize(Ctx.Context);
    if (batch <= 1) {
        return;
    }
    g_upgradeCostState.multiplier = batch;
    if (g_debug) {
    RC::Output::send<RC::LogLevel::Verbose>(
    STR("[SA][UPGRADE] batch size={}\n"), batch);
    }
}

static void post_Server_TryCraftUpgrade(UnrealScriptFunctionCallableContext&, void*) {
    clearUpgradeCostState();
}

static void pre_AreItemsAvailable(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_upgradeCostState.multiplier <= 1) {
        return;
    }
    auto** player = param<UObject*>(Ctx, STR("Player"));
    if (g_upgradeCostState.player && (!player || *player != g_upgradeCostState.player)) {
        return;
    }
    if (auto* items = param<TArray<FAbioticItemInput>>(Ctx, STR("Items"))) {
        scaleUpgradeCostArray(*items, g_upgradeCostState.multiplier);
    }
}

static void pre_ConsumeCraftingRecipe(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_upgradeCostState.multiplier <= 1 ||
        (g_upgradeCostState.player && Ctx.Context != g_upgradeCostState.player)) {
        return;
    }
    if (auto* items = param<TArray<FAbioticItemInput>>(Ctx, STR("ItemsToConsume"))) {
        scaleUpgradeCostArray(*items, g_upgradeCostState.multiplier);
    }
}

namespace {
struct UpgradeUiScaleFrame {
    TArray<FAbioticItemInput>* requiredItems = nullptr;
    const void* data = nullptr;
    std::vector<int32_t> originalCounts;
};

}

static std::vector<UpgradeUiScaleFrame> g_upgradeUiScaleStack;

// FItemUpgradeEntry::RequiredItems inside the widget's ItemUpgradeEntry field.
static TArray<FAbioticItemInput>* upgradeRequiredItems(UObject* widget) {
    auto* entry = g_fItemUpgradeEntry.in<uint8_t>(widget);
    if (!entry) {
        return nullptr;
    }
    static int32_t s_requiredItemsOffset = -2;
    if (s_requiredItemsOffset == -2) {
        auto* prop = CastField<FStructProperty>(findProperty(widget->GetClassPrivate(), STR("ItemUpgradeEntry")));
        s_requiredItemsOffset = propertyOffset(prop ? prop->GetStruct().Get() : nullptr, STR("RequiredItems"));
        if (s_requiredItemsOffset < 0) {
            RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] FItemUpgradeEntry.RequiredItems not resolved\n"));
        }
    }
    return s_requiredItemsOffset < 0 ? nullptr
                                     : reinterpret_cast<TArray<FAbioticItemInput>*>(entry + s_requiredItemsOffset);
}

static void pre_UpdateSelectedRecipeItem(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_upgradeUiScaleStack.push_back({});
    auto& frame = g_upgradeUiScaleStack.back();
    auto* widget = Ctx.Context;
    if (!widget) {
        return;
    }

    auto** bench = g_fBenchRef.in<UObject*>(widget);
    const int32_t batch = getUpgradeBatchSize(bench ? *bench : nullptr);
    if (batch <= 1) {
        return;
    }

    auto* requiredItems = upgradeRequiredItems(widget);
    if (!requiredItems || requiredItems->Num() <= 0 || !requiredItems->GetData()) {
        return;
    }

    frame.requiredItems = requiredItems;
    frame.data = requiredItems->GetData();
    frame.originalCounts.reserve(requiredItems->Num());
    for (int32_t i = 0; i < requiredItems->Num(); ++i) {
        frame.originalCounts.push_back((*requiredItems)[i].Count);
        (*requiredItems)[i].Count = scaleCostCount((*requiredItems)[i].Count, batch);
    }
    if (g_debug) {
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[SA][UPGRADE] scaled UI requirements by {}\n"), batch);
    }
}

static void post_UpdateSelectedRecipeItem(UnrealScriptFunctionCallableContext&, void*) {
    if (g_upgradeUiScaleStack.empty()) {
        return;
    }

    UpgradeUiScaleFrame frame = std::move(g_upgradeUiScaleStack.back());
    g_upgradeUiScaleStack.pop_back();
    if (!frame.requiredItems || frame.requiredItems->GetData() != frame.data) {
        return;
    }

    const int32_t count = std::min<int32_t>(
        frame.requiredItems->Num(),
        static_cast<int32_t>(frame.originalCounts.size()));
    for (int32_t i = 0; i < count; ++i) {
        (*frame.requiredItems)[i].Count = frame.originalCounts[i];
    }
}

static void post_PlayerBeginPlay(UnrealScriptFunctionCallableContext&, void*) {
    applyStackTweaksNow();
}

static void noopHook(UnrealScriptFunctionCallableContext&, void*) {}

static bool registerHook(const wchar_t* fullName,
                         UnrealScriptFunctionCallable pre,
                         UnrealScriptFunctionCallable post) {
    try {
        if (!pre) {
            pre = noopHook;
        }
        if (!post) {
            post = noopHook;
        }
        UObjectGlobals::RegisterHook(fullName, pre, post, nullptr);
        RC::Output::send<RC::LogLevel::Verbose>(STR("[StackAnything] Hook registered: {}\n"), fullName);
        return true;
    } catch (const std::exception& e) {
        const char* what = e.what();
        std::wstring msg(what, what + std::strlen(what));
        RC::Output::send<RC::LogLevel::Error>(STR("[StackAnything] Hook FAILED: {} — {}\n"), fullName, msg);
        return false;
    }
}

int registerHooks() {
    int count = 0;
    // Re-apply stack sizes once the world's mod item tables have loaded.
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:ReceiveBeginPlay"),
        nullptr, post_PlayerBeginPlay);

    // The remaining hooks read and write inventory structs directly; skip them
    // all when the layout check failed rather than corrupt inventories.
    if (!g_layout.mirrorsValid) {
        return count;
    }
    count += registerHook(
        STR("/Game/Blueprints/DeployedObjects/Furniture/Deployed_CraftingBench_Upgrade.Deployed_CraftingBench_Upgrade_C:Server_TryCraftUpgrade"),
        pre_Server_TryCraftUpgrade, post_Server_TryCraftUpgrade);
    count += registerHook(
        STR("/Game/Blueprints/Libraries/ItemFunctionLibrary.ItemFunctionLibrary_C:AreItemsAvailable"),
        pre_AreItemsAvailable, nullptr);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:ConsumeCraftingRecipe"),
        pre_ConsumeCraftingRecipe, nullptr);
    count += registerHook(
        STR("/Game/Blueprints/Widgets/Inventory/W_ItemUpgradeEntry.W_ItemUpgradeEntry_C:UpdateSelectedRecipeItem"),
        pre_UpdateSelectedRecipeItem, post_UpdateSelectedRecipeItem);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_InventoryComponent.Abiotic_InventoryComponent_C:IsInventoryFull"),
        pre_IsInventoryFull, nullptr);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:Server_TryChangeValueInLiquidContainer"),
        pre_TryChangeValueInLiquidContainer, post_TryChangeValueInLiquidContainer);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:IsHoldingLiquidContainer"),
        nullptr, post_IsHoldingLiquidContainer);
    count += registerHook(
        STR("/Game/Blueprints/DeployedObjects/Furniture/Deployed_LiquidContainer_ParentBP.Deployed_LiquidContainer_ParentBP_C:TryFill_TO_PlayerContainer"),
        nullptr, post_TryFill_TO_PlayerContainer);
    count += registerHook(
        STR("/Game/Blueprints/DeployedObjects/Furniture/Deployed_LiquidContainer_ParentBP.Deployed_LiquidContainer_ParentBP_C:TryFill_FROM_PlayerContainer"),
        pre_TryFill_FROM_PlayerContainer, post_TryFill_FROM_PlayerContainer);
    count += registerHook(
        STR("/Game/Blueprints/Libraries/ItemFunctionLibrary.ItemFunctionLibrary_C:AreItemsStackable"),
        nullptr, post_AreItemsStackable);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_InventoryComponent.Abiotic_InventoryComponent_C:CreateSortedItemData"),
        nullptr, post_CreateSortedItemData);
    count += registerHook(
        STR("/Script/AbioticFactor.AbioticFunctionLibrary:SortInventoryArray"),
        pre_SortInventoryArray, nullptr);
    return count;
}
