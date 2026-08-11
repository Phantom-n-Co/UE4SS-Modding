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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

using namespace RC::Unreal;

static const wchar_t* kPlayerClassPath =
    L"/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C";
static const wchar_t* kBeginPlayPath =
    L"/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:ReceiveBeginPlay";
static const wchar_t* kProcessDamagePath =
    L"/Game/Blueprints/Characters/Abiotic_Character_ParentBP.Abiotic_Character_ParentBP_C:ProcessDamage";
static const wchar_t* kIsInventorySlotEmptyPath =
    L"/Game/Blueprints/Characters/Abiotic_InventoryComponent.Abiotic_InventoryComponent_C:IsInventorySlotEmpty";
static const wchar_t* kShelfClassPath =
    L"/Game/Blueprints/DeployedObjects/Furniture/Deployed_WishingShelf.Deployed_WishingShelf_C";
static const wchar_t* kWishingShelfItemsTablePath =
    L"/Game/Blueprints/DataTables/DT_WishingShelfItems.DT_WishingShelfItems";
static constexpr double kNightLightZ = 1000.2581;

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

// Mirror of the game's FAbiotic_InventoryItemSlotStruct (size 0x98), read
// directly off CurrentInventory. Layout matches StackAnything's in-game-proven
// view; sizes are compile-time-asserted so a game update breaks the build
// instead of silently reading garbage.
struct FAbioticSlotRowHandle {
    void* DataTable;   // 0x00
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

// Max stack of the item in `slot`, from its own DataTable row (StackSize_47 at
// 0x238, StackAnything-proven layout). 0 if the table/row can't be resolved.
static int32_t getMaxStack(const FAbioticSlotView& slot) {
    auto* dt = static_cast<UDataTable*>(slot.ItemDataTable.DataTable);
    if (!dt || slot.ItemDataTable.RowName.IsNone()) {
        return 0;
    }
    if (uint8_t* row = dt->FindRowUnchecked(slot.ItemDataTable.RowName)) {
        return *reinterpret_cast<int32_t*>(row + 0x238);
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
static UObject* getWorldFlagSubsystem() {
    UObject* found = nullptr;
    UObjectGlobals::ForEachUObject([&](UObject* obj, int32_t, int32_t) -> RC::LoopAction {
        if (obj->GetClassPrivate()->GetPathName().find(L"WorldFlagSubsystem") != std::wstring::npos &&
            !obj->HasAnyFlags(RF_ClassDefaultObject)) {
            found = obj;
            return RC::LoopAction::Break;
        }
        return RC::LoopAction::Continue;
    });
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
            std::memcpy(at, flagHandle, 0x20);
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
    std::mt19937 rng{std::random_device{}()};
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
    for (auto& entry : g_shelfCache) {
        if (entry.stocking.Get() != stocking) {
            continue;
        }
        if (UObject* shelf = entry.shelf.Get()) {
            if (readStocking(shelf) == stocking) {
                return shelf;
            }
        }
        entry = {};
    }
    UObject* shelf = scanShelfFor(stocking);
    g_shelfCache.push_back({FWeakObjectPtr(stocking), FWeakObjectPtr(shelf)});
    return shelf;
}

static void pre_ProcessDamage(UnrealScriptFunctionCallableContext& Ctx, void*) {
    if (!g_config.instantButcher) {
        return;
    }
    UObject* damageType = getObjectParam(Ctx, L"DamageType");
    if (!damageType) {
        return;
    }
    if (FProperty* prop = damageType->GetPropertyByNameInChain(STR("InstantGibType"))) {
        if (auto* b = CastField<FBoolProperty>(prop)) {
            b->SetPropertyValueInContainer(damageType, true);
        }
    }
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
    count += registerHook(kProcessDamagePath, pre_ProcessDamage, nullptr);
    count += registerHook(kBeginPlayPath, pre_ReceiveBeginPlay, nullptr);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:LocalUpdateWalkSpeed"),
        nullptr, post_LocalUpdateWalkSpeed);
    count += registerHook(
        STR("/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:LocalUpdateJumpHeight"),
        nullptr, post_LocalUpdateJumpHeight);
    count += registerHook(kIsInventorySlotEmptyPath, nullptr, post_IsInventorySlotEmpty);
    return count;
}
