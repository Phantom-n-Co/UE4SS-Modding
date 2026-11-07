#include "RandomTweaks.hpp"

#include <DynamicOutput/DynamicOutput.hpp>
#include <Constructs/Loop.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FSoftObjectPath.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/UnrealCoreStructs.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/FWeakObjectPtr.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/Core/Containers/Map.hpp>
#include "teleporter_names.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <random>
#include <vector>

using namespace RC::Unreal;

static const wchar_t* kPlayerClassPath =
    L"/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C";
static const wchar_t* kBeginPlayPath =
    L"/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:ReceiveBeginPlay";
static const wchar_t* kCorpseDamagePath =
    L"/Game/Blueprints/Environment/Special/CharacterCorpse_ParentBP.CharacterCorpse_ParentBP_C:ReceivePointDamage";
static const wchar_t* kProcessDamagePath =
    L"/Game/Blueprints/Characters/Abiotic_Character_ParentBP.Abiotic_Character_ParentBP_C:ProcessDamage";
static const wchar_t* kIsInventorySlotEmptyPath =
    L"/Game/Blueprints/Characters/Abiotic_InventoryComponent.Abiotic_InventoryComponent_C:IsInventorySlotEmpty";
static const wchar_t* kShelfClassPath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_WishingShelf.Deployed_WishingShelf_C";
static const wchar_t* kWishingShelfItemsTablePath =
    L"/Game/Blueprints/DataTables/DT_WishingShelfItems.DT_WishingShelfItems";
static const wchar_t* kUpdateGearVisualPath =
    L"/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:Update Gear Visual Status";
static const wchar_t* kUpdateFPMeshVisibilityPath =
    L"/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:UpdateFPMeshVisibility";
static const wchar_t* kTrinketClassPath =
    L"/Game/Blueprints/Items/Gear/Gear_Trinket_ParentBP.Gear_Trinket_ParentBP_C";
static const wchar_t* kStartAttachTrinketPath =
    L"/Game/Blueprints/Items/Gear/Gear_Trinket_ParentBP.Gear_Trinket_ParentBP_C:StartAttachTrinket";
static const wchar_t* kTransmogSlotClassPath =
    L"/Game/Blueprints/Widgets/Inventory/W_TransmogItemSlot.W_TransmogItemSlot_C";
static const wchar_t* kTransmogSlotEmptyClickPath =
    L"/Game/Blueprints/Widgets/Inventory/W_TransmogItemSlot.W_TransmogItemSlot_C:OnEmptySlotClicked";
static constexpr double kNightLightZ = 1000.2581;
// E_InventorySlotType::EquipmentSlot_Suit / _Backpack. Transmog arrays are indexed by
// SlotTypeToIndex, which is (slot - 3).
static constexpr uint8_t kSuitSlot = 8;
static constexpr uint8_t kBackpackSlot = 6;
static constexpr int32_t kBackpackTransmogIndex = kBackpackSlot - 3;
static constexpr int32_t kSuitTransmogIndex = kSuitSlot - 3;

static TweakConfig g_config{};
static UClass* g_playerClass = nullptr;

// Per-stocking cache of the linked Wishing Shelf actor, so the expensive object
// scan only runs once per stocking (re-scanned if the shelf is destroyed or the
// link changes). FWeakObjectPtr resolves safely even after the shelf is freed.
struct ShelfCache {
    FWeakObjectPtr stocking;
    FWeakObjectPtr shelf;
};
static std::vector<ShelfCache> g_shelfCache;

static void log(const std::wstring& msg) {
    if (g_config.debugLogging) {
        RC::Output::send<RC::LogLevel::Verbose>(STR("[RandomTweaks] {}\n"), msg.c_str());
    }
}

// Writes a numeric tweak via the matching property type. No-ops if the property
// doesn't exist on the object (mirrors the Lua's pcall-skip behavior).
static void setScalarProp(UObject* obj, const wchar_t* name, double value) {
    if (!obj) {
        return;
    }
    FProperty* prop = obj->GetPropertyByNameInChain(name);
    if (!prop) {
        return;
    }
    if (auto* f = CastField<FFloatProperty>(prop)) {
        f->SetPropertyValueInContainer(obj, static_cast<float>(value));
    } else if (auto* d = CastField<FDoubleProperty>(prop)) {
        d->SetPropertyValueInContainer(obj, value);
    } else if (auto* i = CastField<FIntProperty>(prop)) {
        i->SetPropertyValueInContainer(obj, static_cast<int32_t>(value));
    }
}

static void applyTweaksToObject(UObject* obj) {
    if (!obj) {
        return;
    }
    const TweakConfig& c = g_config;
    if (c.walkSpeed) {
        setScalarProp(obj, STR("BaseWalkSpeed"), *c.walkSpeed);
        setScalarProp(obj, STR("CurrentWalkSpeed"), *c.walkSpeed);
    }
    if (c.sprintSpeed) {
        setScalarProp(obj, STR("BaseSprintSpeed"), *c.sprintSpeed);
    }
    if (c.staminaDrainRate) {
        setScalarProp(obj, STR("Stamina_DrainRate"), *c.staminaDrainRate);
    }
    if (c.staminaRegainRate) {
        setScalarProp(obj, STR("Stamina_RegainRate"), *c.staminaRegainRate);
    }
    if (c.interactionRange) {
        setScalarProp(obj, STR("InteractionRange"), *c.interactionRange);
    }
    if (c.ladderClimbSpeed) {
        setScalarProp(obj, STR("LadderClimbSpeed"), *c.ladderClimbSpeed);
    }
    if (c.jumpHeight) {
        setScalarProp(obj, STR("CurrentJumpHeight"), *c.jumpHeight);
    }
    if (c.lightIntensity) {
        setScalarProp(obj, STR("Light_DefaultIntensity"), *c.lightIntensity);
    }
    if (c.lanternIntensity) {
        setScalarProp(obj, STR("Lantern_DefaultIntensity"), *c.lanternIntensity);
    }
    if (c.disableWatchLight) {
        if (auto* ptr = obj->GetValuePtrByPropertyNameInChain(STR("WristwatchPointLightLocation"))) {
            FVector::SetZ_Internal(*static_cast<FVector*>(ptr), kNightLightZ);
        }
    }
}

// Recompute stat-driven values from the native base props we just wrote.
static void refreshLivePlayer(UObject* player) {
    if (!player) {
        return;
    }
    if (auto* fn = player->GetFunctionByNameInChain(STR("LocalUpdateWalkSpeed"))) {
        player->ProcessEvent(fn, nullptr);
    }
    if (auto* fn = player->GetFunctionByNameInChain(STR("LocalUpdateJumpHeight"))) {
        player->ProcessEvent(fn, nullptr);
    }
}

// Reads a UObject* param by name from the function's params buffer. BP script
// out/ref params are stored in the frame locals; object refs are inline.
static UObject* getObjectParam(UnrealScriptFunctionCallableContext& Ctx, const wchar_t* name) {
    UFunction* fn = Ctx.TheStack.Node();
    if (!fn) {
        return nullptr;
    }
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        if (prop->GetName() != name) {
            continue;
        }
        auto* objProp = CastField<FObjectPropertyBase>(prop);
        if (!objProp) {
            return nullptr;
        }
        return objProp->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<void>(Ctx.TheStack.Locals()));
    }
    return nullptr;
}

static UObject* getObjectProperty(UObject* obj, const wchar_t* name) {
    if (!obj) {
        return nullptr;
    }
    FProperty* prop = obj->GetPropertyByNameInChain(name);
    auto* objProp = CastField<FObjectPropertyBase>(prop);
    if (!objProp) {
        return nullptr;
    }
    return objProp->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<void>(obj));
}

// Mirror of the game's FAbioticInventoryItemSlot (size 0x98), read directly off
// CurrentInventory. itemLayout() verifies it against reflection and disables the
// Wishing Shelf top-up (the only writer) on a mismatch.
struct FAbioticSlotRowHandle {
    void* VTable;      // 0x00 FRowHandle is polymorphic: not a UDataTable*, never dereference
    FName RowName;     // 0x08
};
struct FAbioticSlotChangeable {
    char AssetID[0x10];               // 0x00
    double CurrentItemDurability;     // 0x10
    double MaxItemDurability;         // 0x18
    int32_t CurrentStack;             // 0x20
    char Rest[0x88 - 0x24];           // 0x24..0x88
};
struct FAbioticSlotView {
    FAbioticSlotRowHandle ItemDataTable;   // 0x00
    FAbioticSlotChangeable Changeable;     // 0x10
};
static_assert(sizeof(FAbioticSlotRowHandle) == 0x10, "row handle size");
static_assert(sizeof(FAbioticSlotChangeable) == 0x88, "changeable size");
static_assert(offsetof(FAbioticSlotChangeable, CurrentStack) == 0x20, "CurrentStack offset");
static_assert(sizeof(FAbioticSlotView) == 0x98, "slot size");

static int32_t structFieldOffset(UStruct* owner, const wchar_t* name) {
    if (!owner) {
        return -1;
    }
    for (FProperty* prop : TFieldRange<FProperty>(owner, EFieldIterationFlags::IncludeSuper)) {
        if (prop->GetName() == name) {
            return prop->GetOffset_ForInternal();
        }
    }
    return -1;
}

// Resolved once from reflection: FAbioticInventoryItem::StackSize, and whether
// FAbioticSlotView still matches the game's slot struct.
struct ItemLayout {
    bool resolved = false;
    bool slotMirrorValid = false;
    int32_t rowStackSize = -1;
};
static ItemLayout g_itemLayout;

static const ItemLayout& itemLayout() {
    if (g_itemLayout.resolved) {
        return g_itemLayout;
    }
    auto* slot = UObjectGlobals::StaticFindObject<UScriptStruct*>(
        nullptr, nullptr, STR("/Script/AbioticFactor.AbioticInventoryItemSlot"));
    auto* changeable = UObjectGlobals::StaticFindObject<UScriptStruct*>(
        nullptr, nullptr, STR("/Script/AbioticFactor.AbioticInventoryChangeableData"));
    auto* row = UObjectGlobals::StaticFindObject<UScriptStruct*>(
        nullptr, nullptr, STR("/Script/AbioticFactor.AbioticInventoryItem"));
    if (!slot || !changeable || !row) {
        return g_itemLayout; // not loaded yet; retry on the next call
    }
    g_itemLayout.resolved = true;
    g_itemLayout.rowStackSize = structFieldOffset(row, STR("StackSize"));
    g_itemLayout.slotMirrorValid =
        slot->GetStructureSize() == static_cast<int32_t>(sizeof(FAbioticSlotView)) &&
        structFieldOffset(slot, STR("ChangeableData")) == static_cast<int32_t>(offsetof(FAbioticSlotView, Changeable)) &&
        changeable->GetStructureSize() == static_cast<int32_t>(sizeof(FAbioticSlotChangeable)) &&
        structFieldOffset(changeable, STR("CurrentStack")) == static_cast<int32_t>(offsetof(FAbioticSlotChangeable, CurrentStack));
    if (!g_itemLayout.slotMirrorValid || g_itemLayout.rowStackSize < 0) {
        RC::Output::send<RC::LogLevel::Error>(
            STR("[RandomTweaks] Inventory slot layout changed (game update?); Wishing Shelf top-up disabled\n"));
    }
    return g_itemLayout;
}

// Max stack of the item in `slot`, from its ItemTable_Global row. 0 if the row
// can't be resolved (callers then don't clamp).
static int32_t getMaxStack(const FAbioticSlotView& slot) {
    const ItemLayout& layout = itemLayout();
    if (layout.rowStackSize < 0 || slot.ItemDataTable.RowName.IsNone()) {
        return 0;
    }
    auto* dt = UObjectGlobals::StaticFindObject<UDataTable*>(
        nullptr, nullptr, STR("/Game/Blueprints/Items/ItemTable_Global.ItemTable_Global"));
    if (!dt) {
        return 0;
    }
    if (uint8_t* row = dt->FindRowUnchecked(slot.ItemDataTable.RowName)) {
        return *reinterpret_cast<int32_t*>(row + layout.rowStackSize);
    }
    return 0;
}

// Layout of a DT_WishingShelfItems row (BP UserDefinedStruct WishingShelfStats_Struct),
// resolved at runtime from the DataTable's RowStruct so a game update can't break offsets.
struct WishingShelfStatsOffsets {
    int32_t quantityMin = -1;
    int32_t quantityMax = -1;
    int32_t requiredFlag = -1;
};
static const WishingShelfStatsOffsets& getWishingShelfStatsOffsets(UDataTable* dt) {
    static UDataTable* cachedTable = nullptr;
    static WishingShelfStatsOffsets cached;
    if (cachedTable == dt) {
        return cached;
    }
    cached = {};
    cachedTable = dt;
    if (UScriptStruct* rowStruct = dt->GetRowStruct()) {
        for (FProperty* prop : TFieldRange<FProperty>(rowStruct, EFieldIterationFlags::IncludeDeprecated)) {
            std::wstring name = prop->GetName();
            if (name.find(L"QuantityMin") == 0) {
                cached.quantityMin = prop->GetOffset_ForInternal();
            } else if (name.find(L"QuantityMax") == 0) {
                cached.quantityMax = prop->GetOffset_ForInternal();
            } else if (name.find(L"RequiredFlag") == 0) {
                cached.requiredFlag = prop->GetOffset_ForInternal();
            }
        }
    }
    return cached;
}

// The game's WorldFlagSubsystem instance (a UWorldSubsystem, one per world).
// Cached weakly: the full object scan only reruns when the world changes.
static UObject* getWorldFlagSubsystem() {
    static FWeakObjectPtr s_cached;
    if (UObject* cached = s_cached.Get()) {
        return cached;
    }
    UObject* found = nullptr;
    UObjectGlobals::ForEachUObject([&](UObject* obj, int32_t, int32_t) -> RC::LoopAction {
        if (obj->GetClassPrivate()->GetPathName().find(L"WorldFlagSubsystem") != std::wstring::npos &&
            !obj->HasAnyFlags(RF_ClassDefaultObject)) {
            found = obj;
            return RC::LoopAction::Break;
        }
        return RC::LoopAction::Continue;
    });
    s_cached = FWeakObjectPtr(found);
    return found;
}

// Call HasWorldFlag(WorldContextObject, FWorldFlagRowHandle) on the subsystem via
// ProcessEvent, building the params buffer from the UFunction's reflection.
static bool worldHasFlag(UObject* worldContext, const uint8_t* flagHandle) {
    UObject* subsystem = getWorldFlagSubsystem();
    if (!subsystem || !flagHandle) {
        return false;
    }
    UFunction* fn = subsystem->GetFunctionByNameInChain(STR("HasWorldFlag"));
    if (!fn) {
        return false;
    }
    std::vector<uint8_t> params(fn->GetStructureSize());
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        void* at = params.data() + prop->GetOffset_ForInternal();
        std::wstring name = prop->GetName();
        if (name == L"WorldContextObject") {
            if (auto* objProp = CastField<FObjectPropertyBase>(prop)) {
                objProp->SetObjectPropertyValue(at, worldContext);
            }
        } else if (name == L"WorldFlag") {
            // FWorldFlagRowHandle (0x10 since v1.5.0, was 0x20): copy exactly the
            // param's size, or the copy runs past the end of the params buffer.
            std::memcpy(at, flagHandle, static_cast<size_t>(prop->GetElementSize()));
        } else if (name == L"ReturnValue") {
            auto* b = reinterpret_cast<bool*>(at);
            *b = false;
        }
    }
    subsystem->ProcessEvent(fn, params.data());
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        if (prop->GetName() == L"ReturnValue") {
            return *reinterpret_cast<bool*>(params.data() + prop->GetOffset_ForInternal());
        }
    }
    return false;
}

// How many of an item TrySpawnGifts would place, mirroring its decision:
// not in DT_WishingShelfItems -> 1; in table -> check RequiredFlag (valid+unset
// world flag -> 0, i.e. don't spawn at all), else RandomIntegerInRange(QuantityMin, QuantityMax).
static int32_t getWishAmount(UObject* worldContext, FName rowName) {
    UDataTable* dt = UObjectGlobals::StaticFindObject<UDataTable*>(nullptr, nullptr, kWishingShelfItemsTablePath);
    if (!dt) {
        return 1;
    }
    uint8_t* row = dt->FindRowUnchecked(rowName);
    if (!row) {
        return 1; // not in DT_WishingShelfItems: Amount = 1
    }
    const WishingShelfStatsOffsets& offs = getWishingShelfStatsOffsets(dt);
    if (offs.requiredFlag >= 0) {
        // FWorldFlagRowHandle (FRowHandle): RowName at +0x08. Valid means non-none.
        FName flagRow = *reinterpret_cast<FName*>(row + offs.requiredFlag + 0x08);
        if (!flagRow.IsNone() && !worldHasFlag(worldContext, row + offs.requiredFlag)) {
            return 0; // required world flag not set: shelf skips this item
        }
    }
    if (offs.quantityMin < 0 || offs.quantityMax < 0) {
        return 1;
    }
    int32_t min = *reinterpret_cast<int32_t*>(row + offs.quantityMin);
    int32_t max = *reinterpret_cast<int32_t*>(row + offs.quantityMax);
    if (max <= min) {
        return min;
    }
    static std::mt19937 rng{std::random_device{}()};
    return min + static_cast<int32_t>(rng() % static_cast<uint32_t>(max - min + 1));
}

// Row name of the item in slot `index` of an inventory component's CurrentInventory.
// NAME_None if anything is missing or the index is out of range.
static FName getSlotRowName(UObject* inventory, int32_t index) {
    if (!inventory || index < 0) {
        return {};
    }
    FProperty* arrProp = inventory->GetPropertyByNameInChain(STR("CurrentInventory"));
    auto* arr = CastField<FArrayProperty>(arrProp);
    if (!arr) {
        return {};
    }
    const auto* slots = reinterpret_cast<const TArray<FAbioticSlotView>*>(
        arrProp->ContainerPtrToValuePtr<void>(inventory));
    if (index >= slots->Num()) {
        return {};
    }
    return (*slots)[index].ItemDataTable.RowName;
}

// The stocking actor linked to a Wishing Shelf via its StockingContainer property.
static UObject* readStocking(UObject* shelf) {
    if (!shelf) {
        return nullptr;
    }
    FProperty* prop = shelf->GetPropertyByNameInChain(STR("StockingContainer"));
    auto* objProp = CastField<FObjectPropertyBase>(prop);
    if (!objProp) {
        return nullptr;
    }
    return objProp->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<void>(shelf));
}

static UObject* scanShelfFor(UObject* stocking) {
    UClass* shelfClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, kShelfClassPath);
    if (!shelfClass) {
        return nullptr;
    }
    UObject* found = nullptr;
    UObjectGlobals::ForEachUObject([&](UObject* obj, int32_t, int32_t) -> RC::LoopAction {
        if (obj->IsA(shelfClass) && readStocking(obj) == stocking) {
            found = obj;
            return RC::LoopAction::Break;
        }
        return RC::LoopAction::Continue;
    });
    return found;
}

static UObject* getShelfFor(UObject* stocking) {
    // Drop entries whose stocking is gone; otherwise the cache only ever grows.
    std::erase_if(g_shelfCache, [](const ShelfCache& e) { return !e.stocking.Get(); });
    for (auto& entry : g_shelfCache) {
        if (entry.stocking.Get() != stocking) {
            continue;
        }
        if (UObject* shelf = entry.shelf.Get(); shelf && readStocking(shelf) == stocking) {
            return shelf;
        }
        // Shelf gone or relinked (or never found): rescan into this same entry.
        UObject* shelf = scanShelfFor(stocking);
        entry.shelf = FWeakObjectPtr(shelf);
        return shelf;
    }
    UObject* shelf = scanShelfFor(stocking);
    g_shelfCache.push_back({FWeakObjectPtr(stocking), FWeakObjectPtr(shelf)});
    return shelf;
}

static bool readBoolProp(UObject* obj, const wchar_t* name) {
    if (!obj) {
        return false;
    }
    FProperty* prop = obj->GetPropertyByNameInChain(name);
    auto* boolProp = CastField<FBoolProperty>(prop);
    return boolProp && boolProp->GetPropertyValueInContainer(obj);
}

static int32_t readIntProp(UObject* obj, const wchar_t* name) {
    if (!obj) {
        return 0;
    }
    FProperty* prop = obj->GetPropertyByNameInChain(name);
    auto* intProp = CastField<FIntProperty>(prop);
    return intProp ? intProp->GetPropertyValueInContainer(obj) : 0;
}

// Vanilla gibs a corpse on the fifth cut from a damage type that can gib.
static void pre_CorpseDamage(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (!g_config.instantButcher) {
        return;
    }
    UObject* corpse = Ctx.Context;
    if (!corpse) {
        return;
    }
    const bool canBeGibbed = readBoolProp(corpse, STR("CanBeGibbed"));
    const bool gibbed = readBoolProp(corpse, STR("IsGibbed"));
    if (!canBeGibbed || gibbed) {
        log(L"butcher: '" + corpse->GetName() + L"' skipped (CanBeGibbed=" +
            std::to_wstring(canBeGibbed) + L", IsGibbed=" + std::to_wstring(gibbed) + L")");
        return;
    }
    UObject* damageType = getObjectParam(Ctx, L"DamageType");
    if (!damageType) {
        return;
    }
    FProperty* prop = damageType->GetPropertyByNameInChain(STR("CanGib"));
    auto* canGib = CastField<FBoolProperty>(prop);
    if (!canGib || !canGib->GetPropertyValueInContainer(damageType)) {
        log(L"butcher: '" + corpse->GetName() + L"' not cut by '" + damageType->GetName() +
            L"' (CanGib=false)");
        return;
    }
    const int32_t maxCuts = readIntProp(corpse, STR("MaxGibCuts"));
    const int32_t cuts = readIntProp(corpse, STR("CurrentGibCuts"));
    setScalarProp(corpse, STR("CurrentGibCuts"), maxCuts > 0 ? maxCuts : 1);
    log(L"butcher: '" + corpse->GetName() + L"' cuts " + std::to_wstring(cuts) + L" -> " +
        std::to_wstring(maxCuts > 0 ? maxCuts : 1) + L" by '" + damageType->GetName() + L"'");
}

static void pre_ReceiveBeginPlay(UnrealScriptFunctionCallableContext& Ctx, void*) {
    applyTweaksToObject(Ctx.Context);
    refreshLivePlayer(Ctx.Context);
}

static void post_LocalUpdateWalkSpeed(UnrealScriptFunctionCallableContext& Ctx, void*) {
    applyTweaksToObject(Ctx.Context);
}

static void post_LocalUpdateJumpHeight(UnrealScriptFunctionCallableContext& Ctx, void*) {
    applyTweaksToObject(Ctx.Context);
}

static int32_t getIntParam(UnrealScriptFunctionCallableContext& Ctx, const wchar_t* name) {
    UFunction* fn = Ctx.TheStack.Node();
    if (!fn) {
        return -1;
    }
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        if (prop->GetName() != name) {
            continue;
        }
        auto* intProp = CastField<FIntProperty>(prop);
        if (!intProp) {
            return -1;
        }
        return intProp->GetPropertyValue(prop->ContainerPtrToValuePtr<int32_t>(Ctx.TheStack.Locals()));
    }
    return -1;
}

// The Wishing Shelf's TrySpawnGifts gate: it only spawns into stocking slots its
// inventory reports as empty. When a stocked slot already holds the same item as
// the shelf's matching slot, top it up directly — forcing the slot "empty" is not
// enough, because the shelf's later gates (ItemTable_Global lookup, tag query,
// DT_WishingShelfItems flags) still silently skip the spawn.
// Only acts when the current call came from TrySpawnGifts (walk the script frame
// stack rather than hooking TrySpawnGifts directly, which isn't loadable at boot).
static bool calledFromTrySpawnGifts(UnrealScriptFunctionCallableContext& Ctx) {
    for (FFrame* frame = &Ctx.TheStack; frame; frame = frame->PreviousFrame()) {
        UFunction* node = frame->Node();
        if (node && node->GetName() == L"TrySpawnGifts") {
            return true;
        }
    }
    return false;
}

// Refresh an inventory so the change is picked up by UI/replication.
static void refreshInventory(UObject* inventory) {
    if (!inventory) {
        return;
    }
    if (auto* fn = inventory->GetFunctionByNameInChain(STR("OnRep_CurrentInventory"))) {
        inventory->ProcessEvent(fn, nullptr);
    }
}

static void post_IsInventorySlotEmpty(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (!g_config.wishingShelfTopUp || !Ctx.Context || !calledFromTrySpawnGifts(Ctx)) {
        return;
    }
    if (!itemLayout().slotMirrorValid) {
        return; // slot layout unverified: never write CurrentStack blind
    }

    UObject* stocking = Ctx.Context->GetOuterPrivate();
    if (!stocking) {
        return;
    }
    if (stocking->GetClassPrivate()->GetPathName().find(L"WishingShelfStocking") == std::wstring::npos) {
        return; // not a stocking: normal path
    }

    int32_t index = getIntParam(Ctx, L"Index");
    if (index < 0) {
        return;
    }

    UObject* shelf = getShelfFor(stocking);
    if (!shelf) {
        return;
    }
    UObject* shelfInv = nullptr;
    if (FProperty* prop = shelf->GetPropertyByNameInChain(STR("ContainerInventory"))) {
        if (auto* objProp = CastField<FObjectPropertyBase>(prop)) {
            shelfInv = objProp->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<void>(shelf));
        }
    }
    if (!shelfInv) {
        return;
    }
    FName shelfRow = getSlotRowName(shelfInv, index);
    if (shelfRow.IsNone()) {
        return; // nothing in the matching shelf slot: spawn won't happen anyway
    }

    FProperty* arrProp = Ctx.Context->GetPropertyByNameInChain(STR("CurrentInventory"));
    auto* arr = CastField<FArrayProperty>(arrProp);
    if (!arr) {
        return;
    }
    auto* slots = reinterpret_cast<TArray<FAbioticSlotView>*>(
        arrProp->ContainerPtrToValuePtr<void>(Ctx.Context));
    if (index >= slots->Num()) {
        return;
    }
    FAbioticSlotView& slot = (*slots)[index];
    if (slot.ItemDataTable.RowName != shelfRow) {
        return; // slot holds a different item: leave it alone
    }

    int32_t amount = getWishAmount(Ctx.Context, shelfRow);
    if (amount <= 0) {
        return; // required world flag not held: shelf would not spawn this either
    }
    int32_t maxStack = getMaxStack(slot);
    if (maxStack > 0 && slot.Changeable.CurrentStack >= maxStack) {
        return; // already full: nothing to add
    }
    if (maxStack > 0 && slot.Changeable.CurrentStack + amount > maxStack) {
        amount = maxStack - slot.Changeable.CurrentStack; // clamp to stack size
    }
    slot.Changeable.CurrentStack += amount;
    refreshInventory(Ctx.Context);
    log(L"wishShelf: refilled matched slot " + std::to_wstring(index) +
        L" (+" + std::to_wstring(amount) +
        L" -> " + std::to_wstring(slot.Changeable.CurrentStack) + L")");
}

static void noopHook(UnrealScriptFunctionCallableContext&, void*) {}

// ---- Hide full suit via transmog ----
// Vanilla hides a gear piece when its transmog toggle (DisableTransmogArray[i],
// passed to GetVisualItemData as CheckForTransmog, so true = transmog on) is on
// AND the dresser slot is set to hidden (TransmogVisibility[i] == false). The
// suit branch of Update Gear Visual Status skips that check and the dresser's
// suit slot has AllowClickEmptyToHide = false; both are patched here. While the
// suit is hidden, Gear_SuitBP is nulled for the duration of the visual functions
// so the game runs its own "no suit" path (clothing, armor, FP legs, visor).

static bool setObjectProperty(UObject* obj, const wchar_t* name, UObject* value) {
    if (!obj) {
        return false;
    }
    FProperty* prop = obj->GetPropertyByNameInChain(name);
    auto* objProp = CastField<FObjectPropertyBase>(prop);
    if (!objProp) {
        return false;
    }
    objProp->SetObjectPropertyValue(prop->ContainerPtrToValuePtr<void>(obj), value);
    return true;
}

static bool getBoolArrayElement(UObject* obj, const wchar_t* name, int32_t index, bool fallback) {
    if (!obj) {
        return fallback;
    }
    FProperty* prop = obj->GetPropertyByNameInChain(name);
    if (!CastField<FArrayProperty>(prop)) {
        return fallback;
    }
    const auto* arr = reinterpret_cast<const TArray<bool>*>(prop->ContainerPtrToValuePtr<void>(obj));
    if (index < 0 || index >= arr->Num()) {
        return fallback;
    }
    return (*arr)[index];
}

static int32_t getByteParam(UnrealScriptFunctionCallableContext& Ctx, const wchar_t* name) {
    UFunction* fn = Ctx.TheStack.Node();
    if (!fn) {
        return -1;
    }
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        if (prop->GetName() == name) {
            return *prop->ContainerPtrToValuePtr<uint8_t>(Ctx.TheStack.Locals());
        }
    }
    return -1;
}

// Calls a function on `obj` whose params are all set by `fill(name, valuePtr, prop)`.
template <typename Fill>
static void callWithParams(UObject* obj, const wchar_t* fnName, Fill fill) {
    UFunction* fn = obj ? obj->GetFunctionByNameInChain(fnName) : nullptr;
    if (!fn) {
        return;
    }
    std::vector<uint8_t> params(fn->GetStructureSize());
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        fill(prop->GetName(), params.data() + prop->GetOffset_ForInternal(), prop);
    }
    obj->ProcessEvent(fn, params.data());
}

static bool isSuitHiddenByTransmog(UObject* player) {
    UObject* tmog = getObjectProperty(player, STR("TmogInventory"));
    if (!tmog) {
        return false;
    }
    const bool transmogOn = getBoolArrayElement(tmog, STR("DisableTransmogArray"), kSuitTransmogIndex, true);
    const bool visible = getBoolArrayElement(tmog, STR("TransmogVisibility"), kSuitTransmogIndex, true);
    return transmogOn && !visible;
}

// One entry per in-flight hooked visual call; `suit` is set when that call
// nulled Gear_SuitBP and must restore it. Nested calls see the null and skip.
struct SuitStash {
    UObject* player;
    UObject* suit;
    bool isSuitSlot;
};
static std::vector<SuitStash> g_suitStash;

// Suit hidden state last applied by an Update Gear Visual Status(Suit) call, per
// player. Other slots' updates (e.g. OnRep_TransmogVisibility -> slot All) don't
// touch the suit, so a mismatch triggers a suit refresh.
struct SuitApplied {
    FWeakObjectPtr player;
    bool hidden;
};
static std::vector<SuitApplied> g_suitApplied;

static bool* findSuitApplied(UObject* player) {
    for (auto& entry : g_suitApplied) {
        if (entry.player.Get() == player) {
            return &entry.hidden;
        }
    }
    return nullptr;
}

static void setSuitApplied(UObject* player, bool hidden) {
    if (bool* existing = findSuitApplied(player)) {
        *existing = hidden;
        return;
    }
    std::erase_if(g_suitApplied, [](const SuitApplied& e) { return !e.player.Get(); });
    g_suitApplied.push_back({FWeakObjectPtr(player), hidden});
}

static void beginSuitVisual(UObject* player, bool isSuitSlot) {
    SuitStash frame{player, nullptr, isSuitSlot};
    UObject* suit = g_config.transmogHideSuitAndBackpack ? getObjectProperty(player, STR("Gear_SuitBP")) : nullptr;
    if (suit) {
        const bool hidden = isSuitHiddenByTransmog(player);
        if (isSuitSlot) {
            setSuitApplied(player, hidden);
        }
        if (hidden) {
            if (isSuitSlot) {
                // The skipped suit branch would attach and hide the logic actor.
                callWithParams(player, STR("Attach Logic Actor and Hide It"),
                               [&](const std::wstring& name, void* at, FProperty* prop) {
                                   if (name == L"LogicActor") {
                                       if (auto* objProp = CastField<FObjectPropertyBase>(prop)) {
                                           objProp->SetObjectPropertyValue(at, suit);
                                       }
                                   }
                               });
                log(L"suit: hidden by transmog");
            }
            setObjectProperty(player, STR("Gear_SuitBP"), nullptr);
            frame.suit = suit;
        }
    }
    g_suitStash.push_back(frame);
}

static SuitStash endSuitVisual() {
    if (g_suitStash.empty()) {
        return {};
    }
    SuitStash frame = g_suitStash.back();
    g_suitStash.pop_back();
    if (frame.suit && !getObjectProperty(frame.player, STR("Gear_SuitBP"))) {
        setObjectProperty(frame.player, STR("Gear_SuitBP"), frame.suit);
    }
    return frame;
}

static void pre_UpdateGearVisualStatus(UnrealScriptFunctionCallableContext& Ctx, void*) {
    beginSuitVisual(Ctx.Context, getByteParam(Ctx, L"EquipSlot") == kSuitSlot);
}

static void post_UpdateGearVisualStatus(UnrealScriptFunctionCallableContext& Ctx, void*) {
    const SuitStash frame = endSuitVisual();
    UObject* player = Ctx.Context;
    if (!g_config.transmogHideSuitAndBackpack || !player || !g_suitStash.empty() || frame.isSuitSlot) {
        return;
    }
    if (!getObjectProperty(player, STR("Gear_SuitBP"))) {
        return;
    }
    const bool hidden = isSuitHiddenByTransmog(player);
    const bool* applied = findSuitApplied(player);
    if ((applied ? *applied : false) == hidden) {
        return;
    }
    callWithParams(player, STR("Update Gear Visual Status"),
                   [](const std::wstring& name, void* at, FProperty*) {
                       if (name == L"EquipSlot") {
                           *static_cast<uint8_t*>(at) = kSuitSlot;
                       }
                   });
}

static void pre_UpdateFPMeshVisibility(UnrealScriptFunctionCallableContext& Ctx, void*) {
    beginSuitVisual(Ctx.Context, false);
}

static void post_UpdateFPMeshVisibility(UnrealScriptFunctionCallableContext&, void*) {
    endSuitVisual();
}

// ---- Backpack trinkets ----
// The game hides a transmog-hidden backpack's mesh but leaves trinkets attached
// to its sockets, so they float. StartAttachTrinket picks the backpack socket
// only when IsValid(AttachedCharacter.Gear_BackpackBP), synchronously before
// its async mesh load; nulling that for the call makes trinkets use their
// no-backpack body attachment instead.
static bool isBackpackHiddenByTransmog(UObject* player) {
    UObject* tmog = getObjectProperty(player, STR("TmogInventory"));
    if (!tmog) {
        return false;
    }
    const bool transmogOn = getBoolArrayElement(tmog, STR("DisableTransmogArray"), kBackpackTransmogIndex, true);
    const bool visible = getBoolArrayElement(tmog, STR("TransmogVisibility"), kBackpackTransmogIndex, true);
    return transmogOn && !visible;
}

struct BackpackStash {
    UObject* player;
    UObject* backpack;
};
static std::vector<BackpackStash> g_backpackStash;

static void pre_StartAttachTrinket(UnrealScriptFunctionCallableContext& Ctx, void*) {
    BackpackStash frame{getObjectParam(Ctx, L"AttachedCharacter"), nullptr};
    if (g_config.transmogHideSuitAndBackpack && frame.player) {
        UObject* backpack = getObjectProperty(frame.player, STR("Gear_BackpackBP"));
        if (backpack && isBackpackHiddenByTransmog(frame.player)) {
            setObjectProperty(frame.player, STR("Gear_BackpackBP"), nullptr);
            frame.backpack = backpack;
            log(L"trinket: attaching to body, backpack hidden by transmog");
        }
    }
    g_backpackStash.push_back(frame);
}

static void post_StartAttachTrinket(UnrealScriptFunctionCallableContext&, void*) {
    if (g_backpackStash.empty()) {
        return;
    }
    BackpackStash frame = g_backpackStash.back();
    g_backpackStash.pop_back();
    if (frame.backpack && !getObjectProperty(frame.player, STR("Gear_BackpackBP"))) {
        setObjectProperty(frame.player, STR("Gear_BackpackBP"), frame.backpack);
    }
}

// Lets the dresser's empty suit and backpack slots toggle their hidden flag like
// the armor slots (vanilla sets AllowClickEmptyToHide = false on both). The
// game hides the backpack mesh itself; trinkets are handled above.
static void pre_TransmogSlotEmptyClicked(UnrealScriptFunctionCallableContext& Ctx, void*) {
    UObject* slot = Ctx.Context;
    if (!g_config.transmogHideSuitAndBackpack || !slot) {
        return;
    }
    auto* slotType = static_cast<uint8_t*>(slot->GetValuePtrByPropertyNameInChain(STR("SlotType")));
    if (!slotType || (*slotType != kSuitSlot && *slotType != kBackpackSlot)) {
        return;
    }
    if (auto* allow = CastField<FBoolProperty>(slot->GetPropertyByNameInChain(STR("AllowClickEmptyToHide")))) {
        allow->SetPropertyValueInContainer(slot, true);
    }
}

// ---- Nameable teleporter pads ----
// Vanilla pads pick one of 26 tags; the tag index is stored as the pad's
// TeleporterFrequency dynamic property and all linking is int-based
// ("teleporterpad<id>", GameState location "<id>"). A custom name is stored in
// ChangeableData.PlayerMadeString (saved and replicated with the pad; the field
// pre-1.0 pads used for typed names) and mapped to a stable id >= 1000, so
// vanilla links same-named pads by itself. The tag popup's text line is made
// editable; a typed name is sent as "<id>:<name>" through the vanilla string RPC.
// Without the mod, vanilla's AttemptSaveUpgrade converts the names to tags on load.
namespace tn = teleporter_names;

static const wchar_t* kPadClassPath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_TeleporterPad.Deployed_TeleporterPad_C";
static const wchar_t* kPadDeliverStringPath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_TeleporterPad.Deployed_TeleporterPad_C:DeliverString";
static const wchar_t* kPadOnLoadedFromSavePath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_TeleporterPad.Deployed_TeleporterPad_C:OnLoadedFromSave";
static const wchar_t* kPadGetItemNameTextPath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_TeleporterPad.Deployed_TeleporterPad_C:GetItemNameText";
static const wchar_t* kPadInteractPath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_TeleporterPad.Deployed_TeleporterPad_C:InteractWith_B_LocalFX";
static const wchar_t* kPadYesClickedPath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_TeleporterPad.Deployed_TeleporterPad_C:YesClicked_Event";
static const wchar_t* kTeleporterPopupClassPath =
    L"/Game/Blueprints/Widgets/MenuSystem/W_MenuPopup_TeleporterInput.W_MenuPopup_TeleporterInput_C";
static const wchar_t* kTeleporterPopupConstructPath =
    L"/Game/Blueprints/Widgets/MenuSystem/W_MenuPopup_TeleporterInput.W_MenuPopup_TeleporterInput_C:Construct";
static const wchar_t* kTeleporterPopupUpdateTextPath =
    L"/Game/Blueprints/Widgets/MenuSystem/W_MenuPopup_TeleporterInput.W_MenuPopup_TeleporterInput_C:UpdateText";
static const wchar_t* kTagOptionClassPath =
    L"/Game/Blueprints/Widgets/MenuSystem/W_TeleporterTag_Option.W_TeleporterTag_Option_C";
static const wchar_t* kTagOptionConstructPath =
    L"/Game/Blueprints/Widgets/MenuSystem/W_TeleporterTag_Option.W_TeleporterTag_Option_C:Construct";
static constexpr uint8_t kTeleporterFrequencyKey = 5; // EDynamicProperty::TeleporterFrequency

// ProcessEvent params, initialized through reflection and destroyed on scope
// exit, so FString/FText params and return values don't leak.
struct CallParams {
    UObject* obj = nullptr;
    UFunction* fn = nullptr;
    std::vector<uint8_t> data;
    std::vector<FProperty*> props;

    CallParams(UObject* o, const wchar_t* name) : obj(o) {
        fn = obj ? obj->GetFunctionByNameInChain(name) : nullptr;
        if (!fn) {
            return;
        }
        data.resize(static_cast<size_t>(fn->GetStructureSize()));
        for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
            if (prop->HasAnyPropertyFlags(CPF_Parm)) {
                prop->InitializeValue_InContainer(data.data());
                props.push_back(prop);
            }
        }
    }
    ~CallParams() {
        for (FProperty* prop : props) {
            prop->DestroyValue_InContainer(data.data());
        }
    }
    CallParams(const CallParams&) = delete;
    CallParams& operator=(const CallParams&) = delete;

    explicit operator bool() const { return fn != nullptr; }
    void* at(FProperty* prop) { return prop->ContainerPtrToValuePtr<void>(data.data()); }
    // First input param of field type T (return value excluded).
    template <typename T>
    FProperty* input() {
        for (FProperty* prop : props) {
            if (!prop->HasAnyPropertyFlags(CPF_ReturnParm) && CastField<T>(prop)) {
                return prop;
            }
        }
        return nullptr;
    }
    FProperty* ret() {
        for (FProperty* prop : props) {
            if (prop->HasAnyPropertyFlags(CPF_ReturnParm)) {
                return prop;
            }
        }
        return nullptr;
    }
    void setString(const std::wstring& value) {
        if (FProperty* prop = input<FStrProperty>()) {
            *static_cast<FString*>(at(prop)) = FString(value.c_str());
        }
    }
    void setText(const std::wstring& value) {
        if (FProperty* prop = input<FTextProperty>()) {
            FText text(value.c_str());
            prop->CopyCompleteValue(at(prop), &text);
        }
    }
    void setBool(bool value) {
        if (auto* prop = CastField<FBoolProperty>(input<FBoolProperty>())) {
            prop->SetPropertyValue(at(prop), value);
        }
    }
    void setObject(UObject* value) {
        if (auto* prop = CastField<FObjectPropertyBase>(input<FObjectPropertyBase>())) {
            prop->SetObjectPropertyValue(at(prop), value);
        }
    }
    void call() {
        if (fn) {
            obj->ProcessEvent(fn, data.data());
        }
    }
    bool retBool() {
        auto* prop = CastField<FBoolProperty>(ret());
        return prop && prop->GetPropertyValue(at(prop));
    }
    UObject* retObject() {
        auto* prop = CastField<FObjectPropertyBase>(ret());
        return prop ? prop->GetObjectPropertyValue(at(prop)) : nullptr;
    }
    std::wstring retText() {
        FProperty* prop = ret();
        return CastField<FTextProperty>(prop) ? static_cast<FText*>(at(prop))->ToString() : std::wstring{};
    }
};

// Monster bodies stay the character; vanilla gibs one only after enough post-death
// damage. The stack remembers IsDead from before each call: a killing blow sets it
// mid-call and must not count as post-death damage.
static bool g_gibbing = false;

struct DamageFrame {
    bool deadBefore = false;
};
static std::vector<DamageFrame> g_damageStack;

static void pre_ProcessDamage(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_damageStack.push_back({readBoolProp(Ctx.Context, STR("IsDead"))});
}

static void post_ProcessDamage(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_damageStack.empty()) {
        return;
    }
    const DamageFrame frame = g_damageStack.back();
    g_damageStack.pop_back();
    if (!g_config.instantButcher || g_gibbing || !Ctx.Context || !frame.deadBefore ||
        readBoolProp(Ctx.Context, STR("IsGibbed"))) {
        return;
    }
    UObject* damageType = getObjectParam(Ctx, L"DamageType");
    if (!damageType) {
        return;
    }
    auto* canGib = CastField<FBoolProperty>(damageType->GetPropertyByNameInChain(STR("CanGib")));
    if (!canGib || !canGib->GetPropertyValueInContainer(damageType)) {
        return;
    }
    CallParams call(Ctx.Context, L"TryGibbingNPC");
    if (!call) {
        return;
    }
    UObject* attacker = getObjectParam(Ctx, L"Instigator");
    for (FProperty* prop : call.props) {
        auto* objProp = CastField<FObjectPropertyBase>(prop);
        auto* boolProp = CastField<FBoolProperty>(prop);
        if (objProp && prop->GetName() == STR("DamageType")) {
            objProp->SetObjectPropertyValue(call.at(prop), damageType);
        } else if (objProp && prop->GetName() == STR("Attacker")) {
            objProp->SetObjectPropertyValue(call.at(prop), attacker);
        } else if (boolProp && prop->GetName() == STR("ForceGibNPC")) {
            boolProp->SetPropertyValue(call.at(prop), true);
        }
    }
    g_gibbing = true;
    call.call();
    g_gibbing = false;
    log(L"butcher: gibbed dead '" + Ctx.Context->GetName() + L"' with '" + damageType->GetName() + L"'");
}

// A ChangeableData struct value: of a live deployable or of a world-save entry.
struct ChangeableRef {
    UStruct* type = nullptr;
    void* data = nullptr;
    explicit operator bool() const { return type && data; }
};

// <owner>.<field> where <field> is a struct property; empty if missing.
static ChangeableRef structMember(UStruct* ownerType, void* owner, const wchar_t* field) {
    if (!ownerType || !owner) {
        return {};
    }
    for (FProperty* prop : TFieldRange<FProperty>(ownerType, EFieldIterationFlags::IncludeSuper)) {
        if (prop->GetName() == field) {
            auto* sp = CastField<FStructProperty>(prop);
            return sp ? ChangeableRef{sp->GetStruct().Get(), prop->ContainerPtrToValuePtr<void>(owner)} : ChangeableRef{};
        }
    }
    return {};
}

static ChangeableRef changeableOf(UObject* actor) {
    return actor ? structMember(actor->GetClassPrivate(), actor, STR("ChangeableData")) : ChangeableRef{};
}

static void* changeableField(ChangeableRef cd, const wchar_t* field, FProperty*& outProp) {
    outProp = nullptr;
    if (!cd) {
        return nullptr;
    }
    for (FProperty* prop : TFieldRange<FProperty>(cd.type, EFieldIterationFlags::IncludeSuper)) {
        if (prop->GetName() == field) {
            outProp = prop;
            return prop->ContainerPtrToValuePtr<void>(cd.data);
        }
    }
    return nullptr;
}

static FString* stringField(ChangeableRef cd, const wchar_t* field) {
    FProperty* prop = nullptr;
    void* at = changeableField(cd, field, prop);
    return CastField<FStrProperty>(prop) ? static_cast<FString*>(at) : nullptr;
}

static std::wstring readString(ChangeableRef cd, const wchar_t* field) {
    FString* value = stringField(cd, field);
    return value ? std::wstring(**value) : std::wstring{};
}

static FString* padNameField(UObject* pad) {
    return stringField(changeableOf(pad), STR("PlayerMadeString"));
}

static std::wstring getPadName(UObject* pad) {
    return readString(changeableOf(pad), STR("PlayerMadeString"));
}

static void setPadName(UObject* pad, const std::wstring& name) {
    if (FString* field = padNameField(pad)) {
        *field = FString(name.c_str());
    }
}

// The TeleporterFrequency dynamic property (tag index or custom-name id).
static bool frequencyOf(ChangeableRef cd, int32_t& value) {
    FProperty* prop = nullptr;
    void* at = changeableField(cd, STR("DynamicProperties"), prop);
    auto* arr = CastField<FArrayProperty>(prop);
    auto* inner = arr ? CastField<FStructProperty>(arr->GetInner()) : nullptr;
    if (!at || !inner) {
        return false;
    }
    UScriptStruct* entry = inner->GetStruct().Get();
    const int32_t keyOffset = structFieldOffset(entry, STR("Key"));
    const int32_t valueOffset = structFieldOffset(entry, STR("Value"));
    if (keyOffset < 0 || valueOffset < 0) {
        return false;
    }
    const auto* entries = static_cast<const TArray<uint8_t>*>(at);
    const int32_t stride = inner->GetElementSize();
    for (int32_t i = 0; i < entries->Num(); ++i) {
        const uint8_t* e = entries->GetData() + static_cast<size_t>(i) * stride;
        if (e[keyOffset] == kTeleporterFrequencyKey) {
            value = *reinterpret_cast<const int32_t*>(e + valueOffset);
            return true;
        }
    }
    return false;
}

static bool getPadFrequency(UObject* pad, int32_t& value) {
    return frequencyOf(changeableOf(pad), value);
}

// The custom name if set and the frequency is that name's id.
static std::wstring activeNameOf(ChangeableRef cd) {
    std::wstring name = readString(cd, STR("PlayerMadeString"));
    int32_t frequency = 0;
    if (name.empty() || !frequencyOf(cd, frequency) || frequency != tn::nameToId(name)) {
        return {};
    }
    return name;
}

static std::wstring getActivePadName(UObject* pad) {
    return activeNameOf(changeableOf(pad));
}

// Replicate a ChangeableData edit the way the pad's own BP does.
static void markChangeableDirty(UObject* pad) {
    if (CallParams flush{pad, STR("FlushNetDormancy")}) {
        flush.call();
    }
    auto* helpers = UObjectGlobals::StaticFindObject<UObject*>(
        nullptr, nullptr, STR("/Script/Engine.Default__NetPushModelHelpers"));
    CallParams mark{helpers, STR("MarkPropertyDirty")};
    if (!mark) {
        return;
    }
    mark.setObject(pad);
    if (FProperty* nameProp = mark.input<FNameProperty>()) {
        *static_cast<FName*>(mark.at(nameProp)) = FName(STR("ChangeableData"), FNAME_Add);
    }
    mark.call();
}

static void setPadLocationId(UObject* pad, int32_t id) {
    CallParams c{pad, STR("Set New Location ID")};
    c.setString(std::to_wstring(id));
    c.setBool(true);
    c.call();
}

static bool calledFrom(UnrealScriptFunctionCallableContext& Ctx, const wchar_t* fnName) {
    for (FFrame* frame = Ctx.TheStack.PreviousFrame(); frame; frame = frame->PreviousFrame()) {
        UFunction* node = frame->Node();
        if (node && node->GetName() == fnName) {
            return true;
        }
    }
    return false;
}

static std::wstring getWidgetText(UObject* widget) {
    CallParams c{widget, STR("GetText")};
    c.call();
    return c.retText();
}

static void setWidgetText(UObject* widget, const std::wstring& text) {
    CallParams c{widget, STR("SetText")};
    c.setText(text);
    c.call();
}

static int32_t* popupCurrentIndex(UObject* popup) {
    auto* prop = popup ? CastField<FIntProperty>(popup->GetPropertyByNameInChain(STR("CurrentIndex"))) : nullptr;
    return prop ? prop->ContainerPtrToValuePtr<int32_t>(popup) : nullptr;
}

// ---- Name list (replaces the premade tags in the popup) ----
// Every named pad: live pads (all a client knows), plus on the host the world
// save entries of every streaming level of the current map (pads in unloaded
// levels). Live state wins over the save, matched by AssetID.
static UObject* callStaticObject(const wchar_t* cdoPath, const wchar_t* fn, UObject* worldContext) {
    auto* cdo = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, cdoPath);
    CallParams c{cdo, fn};
    c.setObject(worldContext);
    c.call();
    return c.retObject();
}

static bool isServer(UObject* worldContext) {
    auto* lib = UObjectGlobals::StaticFindObject<UObject*>(
        nullptr, nullptr, STR("/Script/Engine.Default__KismetSystemLibrary"));
    CallParams c{lib, STR("IsServer")};
    c.setObject(worldContext);
    c.call();
    return c.retBool();
}

static std::wstring exportProperty(FProperty* prop, const void* value) {
    FString text;
    prop->ExportTextItem(text, value, nullptr, nullptr, 0);
    return std::wstring(*text);
}

// Short level names of the current map: "Facility" plus its streaming levels
// ("Facility_Office1", ...), the keys the game uses for its world saves.
static std::vector<std::wstring> mapLevelNames(UObject* gameState) {
    UObject* level = gameState ? gameState->GetOuterPrivate() : nullptr;
    UObject* world = level ? level->GetOuterPrivate() : nullptr;
    std::vector<std::wstring> names;
    if (!world) {
        return names;
    }
    names.push_back(world->GetName());
    auto* arr = CastField<FArrayProperty>(world->GetPropertyByNameInChain(STR("StreamingLevels")));
    if (!arr) {
        return names;
    }
    const auto* levels = arr->ContainerPtrToValuePtr<TArray<UObject*>>(world);
    for (UObject* streaming : *levels) {
        FProperty* asset = streaming ? streaming->GetPropertyByNameInChain(STR("WorldAsset")) : nullptr;
        if (!asset) {
            continue;
        }
        const std::wstring path = exportProperty(asset, asset->ContainerPtrToValuePtr<void>(streaming));
        const auto dot = path.find_last_of(L'.');
        if (dot != std::wstring::npos && dot + 1 < path.size()) {
            names.push_back(path.substr(dot + 1));
        }
    }
    return names;
}

// The cached world save of `level`; loads it from disk if it exists but isn't
// cached yet (the game does the same when the level streams in). Never creates one.
static UObject* worldSaveFor(UObject* gameInstance, const std::wstring& level) {
    auto* mapProp = CastField<FMapProperty>(gameInstance->GetPropertyByNameInChain(STR("WorldSaveMap")));
    if (mapProp && CastField<FStrProperty>(mapProp->GetKeyProp()) && CastField<FObjectPropertyBase>(mapProp->GetValueProp())) {
        auto* map = mapProp->ContainerPtrToValuePtr<FScriptMap>(gameInstance);
        const FScriptMapLayout& layout = mapProp->GetMapLayout();
        for (int32_t i = 0; i < map->GetMaxIndex(); ++i) {
            if (!map->IsValidIndex(i)) {
                continue;
            }
            auto* pair = static_cast<uint8_t*>(map->GetData(i, layout));
            if (tn::equalsIgnoreCase(std::wstring(**reinterpret_cast<FString*>(pair)), level)) {
                return *reinterpret_cast<UObject**>(pair + layout.ValueOffset);
            }
        }
    }
    CallParams exists{gameInstance, STR("DoesWorldSaveExist")};
    exists.setString(level);
    exists.call();
    if (!exists.retBool()) {
        return nullptr;
    }
    CallParams get{gameInstance, STR("GetWorldSave")};
    get.setString(level);
    get.call();
    return get.retObject();
}

// AssetID -> active custom name ("" = pad without one), from a world save.
static void collectSavedPadNames(UObject* save, std::map<std::wstring, std::wstring>& byAsset) {
    auto* mapProp = save ? CastField<FMapProperty>(save->GetPropertyByNameInChain(STR("DeployedObjectMap"))) : nullptr;
    auto* valueProp = mapProp ? CastField<FStructProperty>(mapProp->GetValueProp()) : nullptr;
    if (!valueProp) {
        return;
    }
    UScriptStruct* entryType = valueProp->GetStruct().Get();
    FProperty* classProp = nullptr;
    FProperty* destroyedProp = nullptr;
    for (FProperty* prop : TFieldRange<FProperty>(entryType, EFieldIterationFlags::IncludeSuper)) {
        if (prop->GetName() == L"DeployableClass") {
            classProp = prop;
        } else if (prop->GetName() == L"bDestroyed") {
            destroyedProp = prop;
        }
    }
    auto* map = mapProp->ContainerPtrToValuePtr<FScriptMap>(save);
    const FScriptMapLayout& layout = mapProp->GetMapLayout();
    for (int32_t i = 0; i < map->GetMaxIndex(); ++i) {
        if (!map->IsValidIndex(i)) {
            continue;
        }
        auto* entry = static_cast<uint8_t*>(map->GetData(i, layout)) + layout.ValueOffset;
        const ChangeableRef cd = structMember(entryType, entry, STR("ChangeableData"));
        const std::wstring name = activeNameOf(cd); // cheap filter before the class export
        if (name.empty()) {
            continue;
        }
        if (auto* destroyed = CastField<FBoolProperty>(destroyedProp);
            destroyed && destroyed->GetPropertyValue(destroyed->ContainerPtrToValuePtr<void>(entry))) {
            continue;
        }
        if (!classProp ||
            exportProperty(classProp, classProp->ContainerPtrToValuePtr<void>(entry)).find(L"Deployed_TeleporterPad") ==
                std::wstring::npos) {
            continue;
        }
        const std::wstring asset = readString(cd, STR("AssetID"));
        if (asset.empty() || asset == L"-1") {
            continue; // the game never respawns these either
        }
        byAsset[asset] = name;
    }
}

struct PadNameEntry {
    std::wstring name;
    int32_t count = 0;
};

static std::vector<PadNameEntry> collectPadNames(UObject* worldContext) {
    std::map<std::wstring, std::wstring> byAsset;
    if (worldContext && isServer(worldContext)) {
        UObject* gameInstance = callStaticObject(STR("/Script/Engine.Default__GameplayStatics"), STR("GetGameInstance"), worldContext);
        UObject* gameState = callStaticObject(STR("/Script/Engine.Default__GameplayStatics"), STR("GetGameState"), worldContext);
        if (gameInstance) {
            for (const std::wstring& level : mapLevelNames(gameState)) {
                collectSavedPadNames(worldSaveFor(gameInstance, level), byAsset);
            }
        }
    }
    UClass* padClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, kPadClassPath);
    std::vector<UObject*> live;
    if (padClass) {
        UObjectGlobals::ForEachUObject([&](UObject* obj, int32_t, int32_t) -> RC::LoopAction {
            if (obj->IsA(padClass) && !obj->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) {
                live.push_back(obj);
            }
            return RC::LoopAction::Continue;
        });
    }
    // Destroyed pads stay in the object array until GC. The engine's own IsValid
    // knows its garbage flag (UE4SS's EInternalObjectFlags has the UE4 bit layout).
    auto* kismet = UObjectGlobals::StaticFindObject<UObject*>(
        nullptr, nullptr, STR("/Script/Engine.Default__KismetSystemLibrary"));
    for (UObject* obj : live) {
        CallParams valid{kismet, STR("IsValid")};
        valid.setObject(obj);
        valid.call();
        if (!valid.retBool()) {
            continue;
        }
        std::wstring asset = readString(changeableOf(obj), STR("AssetID"));
        if (asset.empty() || asset == L"-1") {
            asset = obj->GetFullName(); // never saved: key by object
        }
        byAsset[asset] = getActivePadName(obj);
    }
    // Pads sharing a name (case-insensitive) are one entry, shown in its most used
    // spelling; ties go to the first in code-unit order, so the label is stable.
    std::map<int32_t, std::map<std::wstring, int32_t>> spellingsById;
    for (const auto& [asset, name] : byAsset) {
        if (!name.empty()) {
            ++spellingsById[tn::nameToId(name)][name];
        }
    }
    std::vector<PadNameEntry> out;
    for (const auto& [id, spellings] : spellingsById) {
        PadNameEntry e;
        int32_t best = 0;
        for (const auto& [spelling, n] : spellings) {
            e.count += n;
            if (n > best) {
                best = n;
                e.name = spelling;
            }
        }
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const PadNameEntry& a, const PadNameEntry& b) {
        std::wstring la = a.name, lb = b.name;
        for (auto& c : la) c = tn::asciiLower(c);
        for (auto& c : lb) c = tn::asciiLower(c);
        return la < lb;
    });
    return out;
}

// Client-side popup state. One teleporter popup is open at a time.
static FWeakObjectPtr g_openingPad;        // pad whose interaction opened the popup
static FWeakObjectPtr g_teleporterPopup;   // the open W_MenuPopup_TeleporterInput
static std::wstring g_lastTagText;         // what vanilla UpdateText wrote while opening
static std::vector<std::wstring> g_listNames; // name list shown in g_teleporterPopup (by option Index)
static bool g_listPicked = false;          // text line came from the name list
struct OptionLabel {
    FWeakObjectPtr option;
    std::wstring label;
};
static std::vector<OptionLabel> g_optionLabels;
struct UpdateTextFrame {
    bool listActive;
    bool keepTyped;
    std::wstring text;
};
static std::vector<UpdateTextFrame> g_updateTextStash;
static FWeakObjectPtr g_pendingNamePad;
static std::wstring g_pendingName;

static void pre_PadInteract(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_config.nameableTeleporters) {
        g_openingPad = FWeakObjectPtr(Ctx.Context);
    }
}

// A custom-id CurrentIndex would index past the tag list; show tag 0 selected instead.
static void pre_TeleporterPopupConstruct(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (!g_config.nameableTeleporters) {
        return;
    }
    g_teleporterPopup = FWeakObjectPtr();
    g_listNames.clear();
    if (int32_t* index = popupCurrentIndex(Ctx.Context); index && tn::isCustomId(*index)) {
        *index = 0;
    }
}

static void setOptionLabel(UObject* option, const std::wstring& label) {
    if (UObject* text = getObjectProperty(option, STR("TextName"))) {
        setWidgetText(text, label);
    }
}

// ---- Tag list height ----
// Vanilla sizes the list for its 26 premade tags: Scrollbox_Options sits in a SizeBox
// (HeightOverride 650) inside a fixed-size popup frame (Border_0, CanvasPanelSlot
// 560 x 804.628), so a short name list leaves most of the popup empty. Both are shrunk
// to the rows actually shown, at most kMaxListRows. Measured live: one option row is
// 46.125 units tall including the 5-unit slot padding both vanilla and this mod apply
// to the options, plus the scrollbox's 10+10 slot padding and 1 unit of slack so a
// fitted list never scrolls by a rounding pixel.
static constexpr float kOptionRowHeight = 46.125f;
static constexpr float kListPadding = 21.f;
static constexpr size_t kMaxListRows = 8;

static UObject* widgetSlot(UObject* widget) {
    return getObjectProperty(widget, STR("Slot"));
}

static UObject* parentWidget(UObject* widget) {
    return getObjectProperty(widgetSlot(widget), STR("Parent"));
}

// FAnchorData::Offsets (FMargin) field of a CanvasPanelSlot. The frame's slot is
// point-anchored, so Offsets.Right is its width and Offsets.Bottom its height.
static float* canvasSlotOffset(UObject* canvasSlot, const wchar_t* field) {
    auto* layout = CastField<FStructProperty>(
        canvasSlot ? canvasSlot->GetPropertyByNameInChain(STR("LayoutData")) : nullptr);
    auto* offsets = layout && layout->GetStruct()
                        ? CastField<FStructProperty>(layout->GetStruct()->GetPropertyByNameInChain(STR("Offsets")))
                        : nullptr;
    auto* prop = offsets && offsets->GetStruct()
                     ? CastField<FFloatProperty>(offsets->GetStruct()->GetPropertyByNameInChain(field))
                     : nullptr;
    if (!prop) {
        return nullptr;
    }
    void* anchorData = layout->ContainerPtrToValuePtr<void>(canvasSlot);
    return prop->ContainerPtrToValuePtr<float>(offsets->ContainerPtrToValuePtr<void>(anchorData));
}

// Sets an FVector2D UFunction param (double members in UE5, float before).
static void setVector2Param(FProperty* prop, void* value, float x, float y) {
    auto* vector = CastField<FStructProperty>(prop);
    UStruct* type = vector ? vector->GetStruct() : nullptr;
    if (!type) {
        return;
    }
    for (const wchar_t* axis : {L"X", L"Y"}) {
        FProperty* member = type->GetPropertyByNameInChain(axis);
        if (!member) {
            return;
        }
        void* atValue = member->ContainerPtrToValuePtr<void>(value);
        const float v = axis[0] == L'X' ? x : y;
        if (member->IsA<FDoubleProperty>()) {
            *static_cast<double*>(atValue) = v;
        } else if (member->IsA<FFloatProperty>()) {
            *static_cast<float*>(atValue) = v;
        }
    }
}

// List region and popup frame to `count` rows, and the scrollbar only while the list overflows
// (vanilla forces it on with AlwaysShowScrollbar, read by the Slate widget at build time, so
// the UMG setter is needed here, not a property write).
static void fitNameListHeight(UObject* scroll, size_t count) {
    const size_t rows = std::clamp<size_t>(count, 1, kMaxListRows);
    const float newList = std::ceil(kOptionRowHeight * static_cast<float>(rows)) + kListPadding;
    UObject* sizeBox = parentWidget(scroll);
    UObject* frameSlot = parentWidget(parentWidget(sizeBox));
    frameSlot = widgetSlot(frameSlot);
    auto* heightProp = CastField<FFloatProperty>(
        sizeBox ? sizeBox->GetPropertyByNameInChain(STR("HeightOverride")) : nullptr);
    float* width = canvasSlotOffset(frameSlot, STR("Right"));
    float* height = canvasSlotOffset(frameSlot, STR("Bottom"));
    if (!heightProp || !width || !height || *height <= 0.f) {
        log(L"teleporter: unexpected tag list layout, popup keeps its vanilla size");
        return;
    }
    CallParams bar{scroll, STR("SetAlwaysShowScrollbar")};
    bar.setBool(false);
    bar.call();
    CallParams setList{sizeBox, STR("SetHeightOverride")};
    auto* heightParam = CastField<FFloatProperty>(setList.input<FFloatProperty>());
    if (!heightParam) {
        return;
    }
    const float oldList = heightProp->GetPropertyValueInContainer(sizeBox);
    *static_cast<float*>(setList.at(heightParam)) = newList;
    setList.call();
    CallParams setFrame{frameSlot, STR("SetSize")};
    if (auto* sizeParam = setFrame.input<FStructProperty>()) {
        setVector2Param(sizeParam, setFrame.at(sizeParam), *width, *height + (newList - oldList));
        setFrame.call();
    }
    log(L"teleporter: tag list with " + std::to_wstring(count) + L" name(s), " +
        std::to_wstring(rows) + L" row(s) shown");
}

// Replaces the tag options vanilla just built with one option per pad name.
static void buildNameList(UObject* popup, const std::wstring& currentName) {
    UObject* scroll = getObjectProperty(popup, STR("Scrollbox_Options"));
    auto* optionClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, kTagOptionClassPath);
    auto* widgetLib = UObjectGlobals::StaticFindObject<UObject*>(
        nullptr, nullptr, STR("/Script/UMG.Default__WidgetBlueprintLibrary"));
    if (!scroll || !optionClass || !widgetLib) {
        return;
    }
    if (CallParams clear{scroll, STR("ClearChildren")}) {
        clear.call();
    }
    auto* lastSelected = CastField<FObjectPropertyBase>(popup->GetPropertyByNameInChain(STR("LastSelectedWidget")));
    if (lastSelected) {
        lastSelected->SetObjectPropertyValue(lastSelected->ContainerPtrToValuePtr<void>(popup), nullptr);
    }
    std::erase_if(g_optionLabels, [](const OptionLabel& o) { return !o.option.Get(); });
    const std::vector<PadNameEntry> names = collectPadNames(popup);
    for (size_t i = 0; i < names.size(); ++i) {
        CallParams create{widgetLib, STR("Create")};
        for (FProperty* prop : create.props) {
            if (prop->GetName() == L"WorldContextObject") {
                CastField<FObjectPropertyBase>(prop)->SetObjectPropertyValue(create.at(prop), popup);
            } else if (prop->GetName() == L"WidgetType") {
                CastField<FObjectPropertyBase>(prop)->SetObjectPropertyValue(create.at(prop), optionClass);
            }
        }
        create.call();
        UObject* option = create.retObject();
        if (!option) {
            continue;
        }
        const std::wstring label = names[i].name + L" (" + std::to_wstring(names[i].count) + L")";
        if (auto* nameProp = CastField<FStrProperty>(option->GetPropertyByNameInChain(STR("Name")))) {
            *nameProp->ContainerPtrToValuePtr<FString>(option) = FString(names[i].name.c_str());
        }
        if (auto* indexProp = CastField<FIntProperty>(option->GetPropertyByNameInChain(STR("Index")))) {
            indexProp->SetPropertyValueInContainer(option, static_cast<int32_t>(i));
        }
        g_optionLabels.push_back({FWeakObjectPtr(option), label});
        CallParams add{scroll, STR("AddChild")};
        add.setObject(option);
        add.call();
        CallParams padding{option, STR("SetPadding")};
        if (auto* margin = CastField<FStructProperty>(padding.input<FStructProperty>());
            margin && margin->GetElementSize() == 4 * sizeof(float)) {
            const float values[4] = {0.f, 5.f, 0.f, 0.f}; // vanilla: Top = 5
            std::memcpy(padding.at(margin), values, sizeof(values));
            padding.call();
        }
        setOptionLabel(option, label); // Construct may already have run with "{num} - {name}"
        g_listNames.push_back(names[i].name);
        if (!currentName.empty() && tn::equalsIgnoreCase(names[i].name, currentName)) {
            CallParams disable{option, STR("SetIsEnabled")}; // vanilla marks the selected option disabled
            disable.setBool(false);
            disable.call();
            if (lastSelected) {
                lastSelected->SetObjectPropertyValue(lastSelected->ContainerPtrToValuePtr<void>(popup), option);
            }
            if (int32_t* index = popupCurrentIndex(popup)) {
                *index = static_cast<int32_t>(i);
            }
        }
    }
    log(L"teleporter: name list with " + std::to_wstring(names.size()) + L" name(s)");
    fitNameListHeight(scroll, names.size());
}

static void post_TeleporterPopupConstruct(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (!g_config.nameableTeleporters) {
        return;
    }
    UObject* popup = Ctx.Context;
    UObject* textBox = getObjectProperty(popup, STR("TextBox_Line"));
    if (!textBox) {
        return;
    }
    // Vanilla shows the line disabled and read-only (a display of the selected tag).
    CallParams enable{textBox, STR("SetIsEnabled")};
    enable.setBool(true);
    enable.call();
    CallParams readOnly{textBox, STR("SetIsReadOnly")};
    readOnly.setBool(false);
    readOnly.call();
    const std::wstring name = getActivePadName(g_openingPad.Get());
    if (!name.empty()) {
        setWidgetText(textBox, name);
    }
    g_listPicked = false;
    buildNameList(popup, name);
    g_teleporterPopup = FWeakObjectPtr(popup); // from here on UpdateText indexes the name list
}

// Options built by buildNameList show "<name> (<count>)" instead of "{num} - {name}".
static void post_TagOptionConstruct(UnrealScriptFunctionCallableContext& Ctx, void*) {
    for (const OptionLabel& o : g_optionLabels) {
        if (o.option.Get() == Ctx.Context) {
            setOptionLabel(Ctx.Context, o.label);
            return;
        }
    }
}

// Vanilla UpdateText writes the tag of CurrentIndex into the text line. Once
// the name list is up, the index is a list index: write that name instead. Typing
// can trigger d-pad navigation (keybinds bubble up from the text box); keep the
// typed text then.
static void pre_TeleporterPopupUpdateText(UnrealScriptFunctionCallableContext& Ctx, void*) {
    UpdateTextFrame frame{false, false, {}};
    if (g_config.nameableTeleporters) {
        frame.listActive = Ctx.Context == g_teleporterPopup.Get();
        UObject* textBox = getObjectProperty(Ctx.Context, STR("TextBox_Line"));
        if (textBox && (frame.listActive || calledFrom(Ctx, L"WidgetAction_DPadDirection"))) {
            frame.text = getWidgetText(textBox);
            if (calledFrom(Ctx, L"WidgetAction_DPadDirection")) {
                CallParams focus{textBox, STR("HasKeyboardFocus")};
                focus.call();
                frame.keepTyped = focus.retBool();
            }
        }
    }
    g_updateTextStash.push_back(frame);
}

static void post_TeleporterPopupUpdateText(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_updateTextStash.empty()) {
        return;
    }
    const UpdateTextFrame frame = g_updateTextStash.back();
    g_updateTextStash.pop_back();
    if (!g_config.nameableTeleporters) {
        return;
    }
    UObject* textBox = getObjectProperty(Ctx.Context, STR("TextBox_Line"));
    if (!textBox) {
        return;
    }
    if (frame.keepTyped) {
        setWidgetText(textBox, frame.text);
        return;
    }
    if (!frame.listActive) {
        g_lastTagText = getWidgetText(textBox); // the opening UpdateText: the pad's tag
        return;
    }
    const int32_t index = getIntParam(Ctx, L"CurrentIndex");
    if (index >= 0 && index < static_cast<int32_t>(g_listNames.size())) {
        setWidgetText(textBox, g_listNames[static_cast<size_t>(index)]);
        g_listPicked = true;
    } else {
        setWidgetText(textBox, frame.text);
    }
}

// Accept: a text line that isn't the tag vanilla wrote is a custom name. Point
// the popup's CurrentIndex at the name's id, so the vanilla RPC already links
// the pad by name (even on a host without the mod), then send the name itself.
static void pre_PadYesClicked(UnrealScriptFunctionCallableContext& Ctx, void*) {
    g_pendingName.clear();
    if (!g_config.nameableTeleporters) {
        return;
    }
    UObject* popup = g_teleporterPopup.Get();
    UObject* textBox = getObjectProperty(popup, STR("TextBox_Line"));
    int32_t* index = popupCurrentIndex(popup);
    if (!textBox || !index) {
        return;
    }
    const std::wstring raw = getWidgetText(textBox);
    const std::wstring name = tn::normalizeName(raw);
    if (name.empty() || (!g_listPicked && tn::equalsIgnoreCase(tn::normalizeName(g_lastTagText), name))) {
        return; // the pad's tag, untouched: vanilla path
    }
    *index = tn::nameToId(name);
    g_pendingName = name;
    g_pendingNamePad = FWeakObjectPtr(Ctx.Context);
}

static void post_PadYesClicked(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (g_pendingName.empty() || g_pendingNamePad.Get() != Ctx.Context) {
        return;
    }
    const std::wstring name = g_pendingName;
    g_pendingName.clear();
    CallParams owner{g_teleporterPopup.Get(), STR("GetOwningPlayerPawn")};
    owner.call();
    CallParams rpc{owner.retObject(), STR("Request_UpdateStringContent")};
    if (!rpc) {
        return;
    }
    rpc.setString(tn::encodeNameMessage(name));
    rpc.setObject(Ctx.Context);
    rpc.call();
    log(L"teleporter: sent name '" + name + L"'");
}

// Server: "<id>:<name>" stores the name and hands vanilla the plain id; a plain
// id that isn't the stored name's (a tag pick) clears the name.
static void pre_PadDeliverString(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (!g_config.nameableTeleporters) {
        return;
    }
    FString* param = nullptr;
    if (UFunction* fn = Ctx.TheStack.Node()) {
        for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
            if (prop->GetName() == L"String" && CastField<FStrProperty>(prop)) {
                param = prop->ContainerPtrToValuePtr<FString>(Ctx.TheStack.Locals());
                break;
            }
        }
    }
    UObject* pad = Ctx.Context;
    if (!param || !padNameField(pad)) {
        return;
    }
    const std::wstring msg(**param);
    std::wstring name;
    int32_t id = 0;
    if (tn::decodeNameMessage(msg, name)) {
        setPadName(pad, name);
        *param = FString(std::to_wstring(tn::nameToId(name)).c_str());
        markChangeableDirty(pad);
        log(L"teleporter: named '" + name + L"'");
    } else if (tn::parsePlainId(msg, id)) {
        const std::wstring current = getPadName(pad);
        if (!current.empty() && id != tn::nameToId(current)) {
            setPadName(pad, L"");
            markChangeableDirty(pad);
            log(L"teleporter: name cleared (tag " + msg + L")");
        }
    }
}

// Vanilla OnLoadedFromSave runs AttemptSaveUpgrade on any non-empty
// PlayerMadeString, which turns the name into a tag. Hide the name for the call,
// then restore it and make sure the frequency matches it.
struct PadLoadStash {
    UObject* pad;
    std::wstring name;
};
static std::vector<PadLoadStash> g_padLoadStash;

static void pre_PadOnLoadedFromSave(UnrealScriptFunctionCallableContext& Ctx, void*) {
    PadLoadStash frame{Ctx.Context, {}};
    if (g_config.nameableTeleporters) {
        frame.name = getPadName(Ctx.Context);
        if (!frame.name.empty()) {
            setPadName(Ctx.Context, L"");
        }
    }
    g_padLoadStash.push_back(frame);
}

static void post_PadOnLoadedFromSave(UnrealScriptFunctionCallableContext&, void*) {
    if (g_padLoadStash.empty()) {
        return;
    }
    const PadLoadStash frame = g_padLoadStash.back();
    g_padLoadStash.pop_back();
    const std::wstring name = tn::normalizeName(frame.name);
    if (name.empty() || !getPadName(frame.pad).empty()) {
        return;
    }
    setPadName(frame.pad, name);
    const int32_t id = tn::nameToId(name);
    int32_t frequency = 0;
    if (!getPadFrequency(frame.pad, frequency) || frequency != id) {
        setPadLocationId(frame.pad, id); // e.g. a pre-1.0 pad that was never upgraded
    }
    markChangeableDirty(frame.pad);
    log(L"teleporter: loaded '" + name + L"'");
}

// "<ItemName> (<LocationName>)", as vanilla formats tags.
static void post_PadGetItemNameText(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (!g_config.nameableTeleporters) {
        return;
    }
    const std::wstring name = getActivePadName(Ctx.Context);
    UFunction* fn = Ctx.TheStack.Node();
    if (name.empty() || !fn) {
        return;
    }
    FProperty* retProp = nullptr;
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        if (prop->HasAnyPropertyFlags(CPF_ReturnParm)) {
            retProp = prop;
        }
    }
    if (!CastField<FTextProperty>(retProp)) {
        return;
    }
    void* result = Ctx.RESULT_DECL ? Ctx.RESULT_DECL : retProp->ContainerPtrToValuePtr<void>(Ctx.TheStack.Locals());
    std::wstring text = static_cast<FText*>(result)->ToString();
    text.erase(text.find_last_not_of(L' ') + 1); // vanilla leaves "{ItemName} " without a tag
    FText replaced((text + L" (" + name + L")").c_str());
    retProp->CopyCompleteValue(result, &replaced);
}

bool tryInit(const TweakConfig& config) {
    g_config = config;
    if (g_playerClass) {
        applyTweaksToObject(g_playerClass->GetClassDefaultObject()); // keep defaults fresh
        return true;
    }

    FSoftObjectPath(FString(kPlayerClassPath)).TryLoad();
    g_playerClass = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, kPlayerClassPath);
    if (!g_playerClass) {
        return false; // not loaded yet; retry on a later tick
    }

    applyTweaksToObject(g_playerClass->GetClassDefaultObject());
    registerHooks();
    log(L"Player class resolved; tweaks applied to defaults, hooks registered");
    return true;
}

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
        return true;
    } catch (const std::exception& e) {
        const char* what = e.what();
        std::wstring msg(what, what + std::strlen(what));
        RC::Output::send<RC::LogLevel::Error>(STR("[RandomTweaks] Hook FAILED: {} — {}\n"), fullName, msg.c_str());
        return false;
    }
}

int registerHooks() {
    int count = 0;
    count += registerHook(kCorpseDamagePath, pre_CorpseDamage, nullptr);
    count += registerHook(kProcessDamagePath, pre_ProcessDamage, post_ProcessDamage);
    count += registerHook(kBeginPlayPath, pre_ReceiveBeginPlay, nullptr);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:LocalUpdateWalkSpeed"),
        nullptr, post_LocalUpdateWalkSpeed);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:LocalUpdateJumpHeight"),
        nullptr, post_LocalUpdateJumpHeight);
    count += registerHook(kIsInventorySlotEmptyPath, nullptr, post_IsInventorySlotEmpty);
    count += registerHook(kUpdateGearVisualPath, pre_UpdateGearVisualStatus, post_UpdateGearVisualStatus);
    count += registerHook(kUpdateFPMeshVisibilityPath, pre_UpdateFPMeshVisibility, post_UpdateFPMeshVisibility);
    // These BPs aren't loaded at boot; load them so the hooks can resolve.
    FSoftObjectPath(FString(kTrinketClassPath)).TryLoad();
    count += registerHook(kStartAttachTrinketPath, pre_StartAttachTrinket, post_StartAttachTrinket);
    FSoftObjectPath(FString(kTransmogSlotClassPath)).TryLoad();
    count += registerHook(kTransmogSlotEmptyClickPath, pre_TransmogSlotEmptyClicked, nullptr);
    FSoftObjectPath(FString(kPadClassPath)).TryLoad();
    count += registerHook(kPadDeliverStringPath, pre_PadDeliverString, nullptr);
    count += registerHook(kPadOnLoadedFromSavePath, pre_PadOnLoadedFromSave, post_PadOnLoadedFromSave);
    count += registerHook(kPadGetItemNameTextPath, nullptr, post_PadGetItemNameText);
    count += registerHook(kPadInteractPath, pre_PadInteract, nullptr);
    count += registerHook(kPadYesClickedPath, pre_PadYesClicked, post_PadYesClicked);
    FSoftObjectPath(FString(kTeleporterPopupClassPath)).TryLoad();
    count += registerHook(kTeleporterPopupConstructPath, pre_TeleporterPopupConstruct, post_TeleporterPopupConstruct);
    count += registerHook(kTeleporterPopupUpdateTextPath, pre_TeleporterPopupUpdateText, post_TeleporterPopupUpdateText);
    FSoftObjectPath(FString(kTagOptionClassPath)).TryLoad();
    count += registerHook(kTagOptionConstructPath, nullptr, post_TagOptionConstruct);
    return count;
}
