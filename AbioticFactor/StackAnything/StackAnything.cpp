#include "StackAnything.hpp"
#include "GameTypes.hpp"

#include <DynamicOutput/DynamicOutput.hpp>
#include <Constructs/Loop.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/FSoftObjectPath.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

using namespace RC::Unreal;

// BP script-hook out/ref params are NOT readable from the params buffer — the
// real value lives at the FOutParmRec destination. Mirror the Lua mod's use of
// FindOutParamValueAddress. Returns nullptr if the param isn't an out-param.
static void* getOutParamValue(UnrealScriptFunctionCallableContext& Ctx, const wchar_t* name) {
    UFunction* fn = Ctx.TheStack.Node();
    if (!fn) {
        return nullptr;
    }
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        if (prop->GetName() == name) {
            return FindOutParamValueAddress(Ctx.TheStack, prop);
        }
    }
    return nullptr;
}

static bool g_debug = false;

static bool isEnergyLiquidType(const int32_t t) {
    return t == E_LiquidType::Energy || t == E_LiquidType::LaserEnergy;
}

static bool shouldSkipRow(const FSAItemRow* row) {
    if (!row) {
        return false;
    }
    const int32_t slot = row->EquipmentData_100.EquipSlot_5.GetValue();
    return slot == E_InventorySlotType::EquipmentSlotHacker ||
           slot == E_InventorySlotType::EquipmentSlotCompanion;
}

static void raiseStackInTable(UDataTable* dt, const std::vector<ConfigEntry>& config) {
    if (!dt) {
        return;
    }
    TArray<FName> rowNames = dt->GetRowNames();
    for (int32_t i = 0; i < rowNames.Num(); ++i) {
        const FName& name = rowNames[i];
        int maxStack = 0;
        if (!matchRowName(config, name.ToString(), maxStack)) {
            continue;
        }
        uint8* data = dt->FindRowUnchecked(name);
        auto* row = reinterpret_cast<FSAItemRow*>(data);
        if (!row || shouldSkipRow(row)) {
            continue;
        }
        if (row->StackSize_47 <= maxStack) {
            row->StackSize_47 = maxStack;
        }
    }
}

void applyStackTweaks(const std::vector<ConfigEntry>& config) {
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
    };

    std::vector<UDataTable*> tables;
    for (const wchar_t* path : kKnownPaths) {
        (void)FSoftObjectPath(FString(path)).TryLoad();
        auto* dt = UObjectGlobals::StaticFindObject<UDataTable*>(nullptr, nullptr, path);
        if (dt) {
            tables.push_back(dt);
        }
    }

    UObjectGlobals::ForEachUObject([&](UObject* obj, int32_t, int32_t) -> RC::LoopAction {
        if (!obj->IsA<UDataTable>()) {
            return RC::LoopAction::Continue;
        }
        const std::wstring p = obj->GetPathName();
        if (p.find(L"/Game/Blueprints/Items/ItemTable_") == std::wstring::npos ||
            p.find(L"ItemTable_Global") != std::wstring::npos ||
            p.find(L"ItemTable_Pets") != std::wstring::npos) {
            return RC::LoopAction::Continue;
        }
        auto* dt = static_cast<UDataTable*>(obj);
        if (std::ranges::find(tables, dt) == tables.end()) {
            tables.push_back(dt);
        }
        return RC::LoopAction::Continue;
    });

    for (UDataTable* dt : tables) {
        raiseStackInTable(dt, config);
    }

    auto* globalDt = UObjectGlobals::StaticFindObject<UDataTable*>(
        nullptr, nullptr, L"/Game/Blueprints/Items/ItemTable_Global.ItemTable_Global");
    if (globalDt) {
        raiseStackInTable(globalDt, config);
    }

    if (g_debug) {
        RC::Output::send<RC::LogLevel::Verbose>(STR("[StackAnything] Applied stack size overrides across {} item tables\n"), tables.size());
    }
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

bool isSlotEmpty(const FAbiotic_InventoryItemSlotStruct& slot) {
    FName rn = slot.ItemDataTable_18.RowName;
    if (rn.IsNone()) {
        return true;
    }
    std::wstring s = rn.ToString();
    return s.empty() || s == L"None" || s == L"Empty";
}

struct ScaledUpgradeCostArray {
    TArray<FAbioticItemCount_Struct>* array;
    const void* data;
    std::vector<int32_t> originalCounts;
};

struct UpgradeCostState {
    int32_t multiplier = 1;
    AAbiotic_PlayerCharacter_C* player = nullptr;
    std::vector<ScaledUpgradeCostArray> scaledArrays;
};

}

static UpgradeCostState g_upgradeCostState;

static int32_t getUpgradeBatchSize(UObject* bench) {
    if (!bench) {
        return 1;
    }

    constexpr size_t kUpgradeItemInventoryOffset = 0x918;
    auto* inventory = *reinterpret_cast<UAbiotic_InventoryComponent_C**>(
        reinterpret_cast<char*>(bench) + kUpgradeItemInventoryOffset);
    if (!inventory) {
        return 1;
    }

    auto& slots = reinterpret_cast<SInventoryView*>(inventory)->CurrentInventory;
    for (int32_t i = 0; i < slots.Num(); ++i) {
        if (!isSlotEmpty(slots[i])) {
            return std::max(1, slots[i].ChangeableData_12.CurrentStack_9);
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

static void scaleUpgradeCostArray(TArray<FAbioticItemCount_Struct>& items,
                                  int32_t multiplier) {
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

static int32_t getMaxStack(const FAbiotic_InventoryItemSlotStruct& slot) {
    UDataTable* dt = slot.ItemDataTable_18.DataTable;
    const FName rn = slot.ItemDataTable_18.RowName;
    if (!dt || rn.IsNone()) {
        return 0;
    }
    auto* row = reinterpret_cast<FSAItemRow*>(dt->FindRowUnchecked(rn));
    return row ? row->StackSize_47 : 0;
}

namespace {
struct SplitTarget {
    UAbiotic_InventoryComponent_C* inventory;
    int32_t index;
    bool mergeInto;
};

}

static SplitTarget findSplitTarget(AAbiotic_PlayerCharacter_C* player,
                                   UAbiotic_InventoryComponent_C* givenInventory,
                                   const FName& rowName, int32_t liquidType, int32_t liquidLevel,
                                   int32_t amountToAdd, UAbiotic_InventoryComponent_C* skipInventory,
                                   int32_t skipIndex) {
    std::vector<UAbiotic_InventoryComponent_C*> candidates;
    if (givenInventory) {
        candidates.push_back(givenInventory);
    }
    if (player) {
        auto* view = reinterpret_cast<SActorPlayerView*>(player);
        if (view->CharacterInventory) {
            candidates.push_back(view->CharacterInventory);
        }
        if (view->CharacterHotbarInventory) {
            candidates.push_back(view->CharacterHotbarInventory);
        }
    }

    for (UAbiotic_InventoryComponent_C* inv : candidates) {
        if (!inv) {
            continue;
        }
        auto* iv = reinterpret_cast<SInventoryView*>(inv);
        TArray<FAbiotic_InventoryItemSlotStruct>& slots = iv->CurrentInventory;
        for (int32_t i = 0; i < slots.Num(); ++i) {
            if (skipInventory && skipIndex >= 0 && inv == skipInventory && i == skipIndex) {
                continue;
            }
            FAbiotic_InventoryItemSlotStruct& slot = slots[i];
            if (isSlotEmpty(slot) || slot.ItemDataTable_18.RowName != rowName) {
                continue;
            }
            FAbiotic_InventoryChangeableDataStruct& c = slot.ChangeableData_12;
            const int32_t maxStack = getMaxStack(slot);
            if (maxStack > 0 && c.CurrentStack_9 > 0 && c.CurrentStack_9 + amountToAdd <= maxStack &&
                c.LiquidLevel_46 == liquidLevel &&
                static_cast<int32_t>(c.CurrentLiquid_19.GetValue()) == liquidType) {
                return {.inventory = inv, .index = i, .mergeInto = true};
            }
        }
    }

    for (UAbiotic_InventoryComponent_C* inv : candidates) {
        if (!inv) {
            continue;
        }
        auto* iv = reinterpret_cast<SInventoryView*>(inv);
        TArray<FAbiotic_InventoryItemSlotStruct>& slots = iv->CurrentInventory;
        for (int32_t i = 0; i < slots.Num(); ++i) {
            if (skipInventory && skipIndex >= 0 && inv == skipInventory && i == skipIndex) {
                continue;
            }
            if (isSlotEmpty(slots[i])) {
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

static void refundSourceContainer(ADeployed_LiquidContainer_ParentBP_C* source,
                                  AAbiotic_PlayerCharacter_C* player,
                                  int32_t liquidType, int32_t amount) {
    if (!source || !player || amount <= 0) {
        return;
    }
    auto* view = reinterpret_cast<SDeployedContainerView*>(source);
    if (view->InfiniteSource) {
        return;
    }
    const int32_t current = view->Liquid_FillLevel;
    const int32_t maxFill = view->Liquid_MaxFill;
    const int32_t newLevel = std::min(current + amount, maxFill);
    if (newLevel == current) {
        return;
    }
    auto* obj = reinterpret_cast<UObject*>(source);
    if (auto* fn = obj->GetFunctionByNameInChain(STR("Server_ModifyFillState"))) {
        FServer_ModifyFillState_Params params;
        params.Liquid = static_cast<E_LiquidType::Type>(liquidType);
        params.NewLiquidValue = newLevel;
        params.SkipSave = false;
        params.FillInstigator = player;
        obj->ProcessEvent(fn, &params);
    }
}

static void post_TryChangeValueInLiquidContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    auto& p = Ctx.GetParams<FServer_TryChangeValueInLiquidContainer_Params>();
    UAbiotic_InventoryComponent_C* inventory = p.Inventory;
    if (!inventory) {
        return;
    }
    UObject* player = Ctx.Context;
    const int32_t slotIndex = p.SlotIndex;
    if (slotIndex < 0) {
        return;
    }
    g_pendingRefund = {};

    if (!p.OptionalItemRow.IsNone()) {
        return; // row-converting fill (e.g. empty bowl -> soup): don't split
    }

    auto* iv = reinterpret_cast<SInventoryView*>(inventory);
    TArray<FAbiotic_InventoryItemSlotStruct>& slots = iv->CurrentInventory;
    if (slotIndex >= slots.Num()) {
        return;
    }
    FAbiotic_InventoryChangeableDataStruct& changeable = slots[slotIndex].ChangeableData_12;

    const int32_t liquidType = p.LiquidType.GetValue();
    if (isEnergyLiquidType(liquidType)) {
        return;
    }

    const int32_t stack = changeable.CurrentStack_9;
    if (stack <= 1) {
        return;
    }

    if (g_pendingDrain.player == reinterpret_cast<AAbiotic_PlayerCharacter_C*>(player) &&
        g_pendingDrain.inventory == inventory && g_pendingDrain.slotIndex == slotIndex) {
        if (p.NewLiquidValue < g_pendingDrain.preLiquid) {
            return; // drain direction: handled in TryFill_FROM_PlayerContainer post-hook
        }
        g_pendingDrain = {};
    }

    if (p.NewLiquidValue <= 0 || changeable.LiquidLevel_46 <= 0) {
        return;
    }

    const FName rowName = slots[slotIndex].ItemDataTable_18.RowName;
    SplitTarget target = findSplitTarget(reinterpret_cast<AAbiotic_PlayerCharacter_C*>(player),
                                         inventory, rowName, liquidType, p.NewLiquidValue, 1,
                                         inventory, slotIndex);
    if (!target.inventory) {
        changeable.CurrentLiquid_19 = E_LiquidType::None;
        changeable.LiquidLevel_46 = 0;
        refreshInventory(inventory);
        g_pendingRefund = {
            .player = reinterpret_cast<AAbiotic_PlayerCharacter_C*>(player),
            .liquidType = liquidType,
            .amount = p.NewLiquidValue,
        };
        return;
    }

    FAbiotic_InventoryItemSlotStruct& targetSlot =
        reinterpret_cast<SInventoryView*>(target.inventory)->CurrentInventory[target.index];
    if (target.mergeInto) {
        targetSlot.ChangeableData_12.CurrentStack_9 += 1;
    } else {
        targetSlot.ItemDataTable_18.DataTable = slots[slotIndex].ItemDataTable_18.DataTable;
        targetSlot.ItemDataTable_18.RowName = slots[slotIndex].ItemDataTable_18.RowName;
        targetSlot.ChangeableData_12.CurrentStack_9 = 1;
        targetSlot.ChangeableData_12.CurrentItemDurability_4 = changeable.CurrentItemDurability_4;
        targetSlot.ChangeableData_12.MaxItemDurability_6 = changeable.MaxItemDurability_6;
        targetSlot.ChangeableData_12.LiquidLevel_46 = p.NewLiquidValue;
        targetSlot.ChangeableData_12.CurrentLiquid_19 = p.LiquidType;
    }

    changeable.CurrentStack_9 = stack - 1;
    changeable.CurrentLiquid_19 = E_LiquidType::None;
    changeable.LiquidLevel_46 = 0;

    refreshInventory(inventory);
    if (target.inventory != inventory) {
        refreshInventory(target.inventory);
    }
}

static void post_IsHoldingLiquidContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_pendingDrain = {};
    UObject* player = Ctx.Context;
    if (!player) {
        return;
    }
    auto* view = reinterpret_cast<SActorPlayerView*>(player);
    FInventorySlotSelected_Struct& selected = view->Server_LastSelectedHotbarItem;
    if (!selected.Inventory_2 || selected.Index_5 < 0) {
        return;
    }
    int32_t* fillAddr = static_cast<int32_t*>(getOutParamValue(Ctx, L"CurrentFillAmount"));
    auto* ltypeAddr = static_cast<TEnumAsByte<E_LiquidType::Type>*>(getOutParamValue(Ctx, L"CurrentLiquidType"));
    int32_t fill = fillAddr ? *fillAddr : 0;
    int32_t ltype = ltypeAddr ? static_cast<int32_t>(ltypeAddr->GetValue()) : 0;
    g_pendingDrain = {
        .player = reinterpret_cast<AAbiotic_PlayerCharacter_C*>(player),
        .inventory = selected.Inventory_2,
        .slotIndex = selected.Index_5,
        .preLiquid = fill,
        .liquidType = ltype,
    };
}

static void post_TryFill_TO_PlayerContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    auto& p = Ctx.GetParams<FTryFill_TO_PlayerContainer_Params>();
    if (p.OnlyCheck) {
        g_pendingRefund = {};
        return;
    }
    if (!g_pendingRefund.player) {
        return;
    }
    if (p.PlayerCharacter != g_pendingRefund.player) {
        g_pendingRefund = {};
        return;
    }
    auto* source = reinterpret_cast<ADeployed_LiquidContainer_ParentBP_C*>(Ctx.Context);
    if (!source) {
        g_pendingRefund = {};
        return;
    }
    AAbiotic_PlayerCharacter_C* player = g_pendingRefund.player;
    int32_t liquidType = g_pendingRefund.liquidType;
    int32_t amount = g_pendingRefund.amount;
    g_pendingRefund = {};
    refundSourceContainer(source, player, liquidType, amount);
}

static void post_TryFill_FROM_PlayerContainer(UnrealScriptFunctionCallableContext& Ctx, void*) {
    auto& p = Ctx.GetParams<FTryFill_FROM_PlayerContainer_Params>();
    if (p.OnlyCheck) {
        g_pendingDrain = {};
        return;
    }
    if (!g_pendingDrain.player) {
        return;
    }
    if (p.PlayerCharacter != g_pendingDrain.player) {
        g_pendingDrain = {};
        return;
    }
    AAbiotic_PlayerCharacter_C* player = g_pendingDrain.player;
    UAbiotic_InventoryComponent_C* inventory = g_pendingDrain.inventory;
    int32_t slotIndex = g_pendingDrain.slotIndex;
    int32_t preLiquid = g_pendingDrain.preLiquid;
    int32_t preLiquidType = g_pendingDrain.liquidType;
    g_pendingDrain = {};

    if (!inventory || slotIndex < 0 || preLiquid <= 0) {
        return;
    }
    if (isEnergyLiquidType(preLiquidType)) {
        return;
    }
    auto* iv = reinterpret_cast<SInventoryView*>(inventory);
    TArray<FAbiotic_InventoryItemSlotStruct>& slots = iv->CurrentInventory;
    if (slotIndex >= slots.Num()) {
        return;
    }
    FAbiotic_InventoryChangeableDataStruct& changeable = slots[slotIndex].ChangeableData_12;
    const int32_t stack = changeable.CurrentStack_9;
    if (stack <= 1 || changeable.LiquidLevel_46 >= preLiquid) {
        return;
    }

    const FName rowName = slots[slotIndex].ItemDataTable_18.RowName;
    const int32_t curLiquidType = changeable.CurrentLiquid_19.GetValue();
    const int32_t curLiquid = changeable.LiquidLevel_46;
    SplitTarget target = findSplitTarget(player, inventory, rowName, curLiquidType, curLiquid, 1,
                                         inventory, slotIndex);
    if (!target.inventory) {
        return;
    }

    FAbiotic_InventoryItemSlotStruct& targetSlot =
        reinterpret_cast<SInventoryView*>(target.inventory)->CurrentInventory[target.index];
    if (target.mergeInto) {
        targetSlot.ChangeableData_12.CurrentStack_9 += 1;
    } else {
        targetSlot.ItemDataTable_18.DataTable = slots[slotIndex].ItemDataTable_18.DataTable;
        targetSlot.ItemDataTable_18.RowName = slots[slotIndex].ItemDataTable_18.RowName;
        targetSlot.ChangeableData_12.CurrentStack_9 = 1;
        targetSlot.ChangeableData_12.CurrentItemDurability_4 = changeable.CurrentItemDurability_4;
        targetSlot.ChangeableData_12.MaxItemDurability_6 = changeable.MaxItemDurability_6;
        targetSlot.ChangeableData_12.LiquidLevel_46 = curLiquid;
        targetSlot.ChangeableData_12.CurrentLiquid_19 = changeable.CurrentLiquid_19;
    }

    changeable.CurrentStack_9 = stack - 1;
    changeable.LiquidLevel_46 = preLiquid;
    changeable.CurrentLiquid_19 = static_cast<E_LiquidType::Type>(preLiquidType);

    refreshInventory(inventory);
    if (target.inventory != inventory) {
        refreshInventory(target.inventory);
    }
}

static std::wstring getVariantName(const FAbiotic_InventoryChangeableDataStruct& c) {
    const FName rn = c.TextureVariantRow_28.RowName;
    if (rn.IsNone()) {
        return {};
    }
    const std::wstring s = rn.ToString();
    return s == L"None" || s == L"Empty" ? std::wstring{} : s;
}

static void post_AreItemsStackable(UnrealScriptFunctionCallableContext& Ctx, void*) {
    auto* slot1 = static_cast<FAbiotic_InventoryItemSlotStruct*>(getOutParamValue(Ctx, L"Item1"));
    auto* slot2 = static_cast<FAbiotic_InventoryItemSlotStruct*>(getOutParamValue(Ctx, L"Item2"));
    if (!slot1 || !slot2) {
        return;
    }
    const FAbiotic_InventoryChangeableDataStruct& c1 = slot1->ChangeableData_12;
    const FAbiotic_InventoryChangeableDataStruct& c2 = slot2->ChangeableData_12;
    const int32_t l1 = c1.CurrentLiquid_19.GetValue();
    const int32_t l2 = c2.CurrentLiquid_19.GetValue();
    if (isEnergyLiquidType(l1) || isEnergyLiquidType(l2)) {
        return; // batteries/lasers keep vanilla stack-charge behavior
    }
    if (l1 != l2 || c1.LiquidLevel_46 != c2.LiquidLevel_46) {
        Ctx.SetReturnValue(false);
        return;
    }
    if (getVariantName(c1) != getVariantName(c2)) {
        Ctx.SetReturnValue(false);
    }
}

static UAbiotic_InventoryComponent_C* g_sortInventory = nullptr;

static void post_CreateSortedItemData(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_sortInventory = reinterpret_cast<UAbiotic_InventoryComponent_C*>(Ctx.Context);
}

static void pre_SortInventoryArray(UnrealScriptFunctionCallableContext& Ctx, void*) {
    UAbiotic_InventoryComponent_C* inv = g_sortInventory;
    g_sortInventory = nullptr;
    if (!inv) {
        return;
    }
    auto& p = Ctx.GetParams<FSortInventoryArray_Params>();
    TArray<FSortedItem>& arr = p.InventoryArray;
    auto* iv = reinterpret_cast<SInventoryView*>(inv);
    TArray<FAbiotic_InventoryItemSlotStruct>& slots = iv->CurrentInventory;

    for (int32_t i = 0; i < arr.Num(); ++i) {
        FSortedItem& item = arr[i];
        if (item.SourceIndex < 0 || item.SourceIndex >= slots.Num()) {
            continue;
        }
        FAbiotic_InventoryChangeableDataStruct& c = slots[item.SourceIndex].ChangeableData_12;
        std::wstring suffix;
        const int32_t ltype = c.CurrentLiquid_19.GetValue();
        const int32_t lvl = c.LiquidLevel_46;
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
    auto& p = Ctx.GetParams<FIsInventoryFull_Params>();
    if (p.IgnoreStackSize || !Ctx.Context) {
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

    p.IgnoreStackSize = true;
}

static void pre_Server_TryCraftUpgrade(UnrealScriptFunctionCallableContext& Ctx, void*) {
    clearUpgradeCostState();
    auto& p = Ctx.GetParams<FServer_TryCraftUpgrade_Params>();
    g_upgradeCostState.player = p.Player;
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
    auto& p = Ctx.GetParams<FAreItemsAvailable_Params>();
    if (g_upgradeCostState.player && p.Player != g_upgradeCostState.player) {
        return;
    }
    auto* items = static_cast<TArray<FAbioticItemCount_Struct>*>(
        getOutParamValue(Ctx, L"Items"));
    if (!items) {
        return;
    }
    scaleUpgradeCostArray(*items, g_upgradeCostState.multiplier);
}

static void pre_ConsumeCraftingRecipe(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_upgradeCostState.multiplier <= 1 ||
        (g_upgradeCostState.player &&
         Ctx.Context != reinterpret_cast<UObject*>(g_upgradeCostState.player))) {
        return;
    }
    auto* items = static_cast<TArray<FAbioticItemCount_Struct>*>(
        getOutParamValue(Ctx, L"ItemsToConsume"));
    if (!items) {
        return;
    }
    scaleUpgradeCostArray(*items, g_upgradeCostState.multiplier);
}

namespace {
struct UpgradeUiScaleFrame {
    TArray<FNativeItemCountView>* requiredItems = nullptr;
    const void* data = nullptr;
    std::vector<int32_t> originalCounts;
};

}

static std::vector<UpgradeUiScaleFrame> g_upgradeUiScaleStack;

static void pre_UpdateSelectedRecipeItem(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_upgradeUiScaleStack.push_back({});
    auto& frame = g_upgradeUiScaleStack.back();
    auto* widget = Ctx.Context;
    if (!widget) {
        return;
    }

    constexpr size_t kItemUpgradeEntryOffset = 0x320;
    constexpr size_t kBenchRefOffset = 0x380;
    auto* bench = *reinterpret_cast<AAbioticDeployed_CraftingBench_ParentBP_C**>(
        reinterpret_cast<char*>(widget) + kBenchRefOffset);
    const int32_t batch = getUpgradeBatchSize(reinterpret_cast<UObject*>(bench));
    if (batch <= 1) {
        return;
    }

    auto* requiredItems = reinterpret_cast<TArray<FNativeItemCountView>*>(
        reinterpret_cast<char*>(widget) + kItemUpgradeEntryOffset);
    if (!requiredItems || requiredItems->Num() <= 0 || !requiredItems->GetData()) {
        return;
    }

    frame.requiredItems = requiredItems;
    frame.data = requiredItems->GetData();
    frame.originalCounts.reserve(requiredItems->Num());
    for (int32_t i = 0; i < requiredItems->Num(); ++i) {
        frame.originalCounts.push_back(requiredItems->operator[](i).Count);
        requiredItems->operator[](i).Count = scaleCostCount(
            requiredItems->operator[](i).Count, batch);
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

    if (frame.requiredItems->GetData()) {
        const int32_t count = std::min<int32_t>(
            frame.requiredItems->Num(),
            static_cast<int32_t>(frame.originalCounts.size()));
        for (int32_t i = 0; i < count; ++i) {
            (*frame.requiredItems)[i].Count = frame.originalCounts[i];
        }
    }
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
        nullptr, post_TryChangeValueInLiquidContainer);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:IsHoldingLiquidContainer"),
        nullptr, post_IsHoldingLiquidContainer);
    count += registerHook(
        STR("/Game/Blueprints/DeployedObjects/Furniture/Deployed_LiquidContainer_ParentBP.Deployed_LiquidContainer_ParentBP_C:TryFill_TO_PlayerContainer"),
        nullptr, post_TryFill_TO_PlayerContainer);
    count += registerHook(
        STR("/Game/Blueprints/DeployedObjects/Furniture/Deployed_LiquidContainer_ParentBP.Deployed_LiquidContainer_ParentBP_C:TryFill_FROM_PlayerContainer"),
        nullptr, post_TryFill_FROM_PlayerContainer);
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
