#pragma once
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <cstddef>

using RC::Unreal::UObject;
using RC::Unreal::UDataTable;
using RC::Unreal::FName;
using RC::Unreal::FString;
using RC::Unreal::TArray;
using RC::Unreal::TEnumAsByte;
using RC::Unreal::int32;
using RC::Unreal::uint8;

namespace E_LiquidType {
    enum Type { None = 0, Water = 1, Energy = 7, LaserEnergy = 12 };
}
namespace E_InventorySlotType {
    enum Type {
        Hotbar = 0,
        InventoryBackpack = 1,
        EquipmentSlotsAll = 2,
        EquipmentSlotTorso = 3,
        EquipmentSlotHead = 4,
        EquipmentSlotLegs = 5,
        EquipmentSlotBackpack = 6,
        EquipmentSlotArms = 7,
        EquipmentSlotSuit = 8,
        EquipmentSlotHeadlamp = 9,
        EquipmentSlotTrinket = 10,
        EquipmentSlotWristwatch = 11,
        EquipmentSlotHacker = 12,
        EquipmentSlotShield = 13,
        EquipmentSlotTrinket2 = 14,
        EquipmentSlotCompanion = 15,
        Max = 16,
    };
}

class UAbiotic_InventoryComponent_C;
class AAbiotic_PlayerCharacter_C;
class ADeployed_LiquidContainer_ParentBP_C;
class AAbioticDeployed_CraftingBench_ParentBP_C;

struct FDataTableRowHandle {
    UDataTable* DataTable;
    FName RowName;
}; // 0x10

struct FAbiotic_InventoryChangeableDataStruct {
    char AssetID_25[0x10];                                            // 0x00
    double CurrentItemDurability_4;                                   // 0x10
    double MaxItemDurability_6;                                       // 0x18
    int32 CurrentStack_9;                                             // 0x20
    int32 CurrentAmmoInMagazine_12;                                   // 0x24
    int32 LiquidLevel_46;                                             // 0x28
    TEnumAsByte<E_LiquidType::Type> CurrentLiquid_19;                 // 0x2C
    FDataTableRowHandle TextureVariantRow_28;                         // 0x30
    bool DynamicState_39;                                             // 0x40
    FString PlayerMadeString_42;                                      // 0x48
    char GameplayTags_45[0x20];                                       // 0x58
    TArray<uint8> DynamicProperties_50;                               // 0x78
}; // 0x88

struct FAbiotic_InventoryItemSlotStruct {
    FDataTableRowHandle ItemDataTable_18;                             // 0x00
    FAbiotic_InventoryChangeableDataStruct ChangeableData_12;         // 0x10
}; // 0x98

struct FInventorySlotSelected_Struct {
#pragma pack(push, 4)
    UAbiotic_InventoryComponent_C* Inventory_2;                       // 0x00
    int32 Index_5;                                                    // 0x08
#pragma pack(pop)
}; // 0x0C

struct FSAItemRow {
    char pad0[0x238];
    int32 StackSize_47;                                               // 0x238
    char pad1[0x3F0 - 0x23C];
    struct { TEnumAsByte<E_InventorySlotType::Type> EquipSlot_5; } EquipmentData_100; // 0x3F0
};

struct FSortedItem {
    FName RowName;                                                    // 0x00
    int32 SourceIndex;                                                // 0x08
    char pad_0C[0x4];                                                 // 0x0C
    char ItemName[0x10];                                              // 0x10
    int32 StackCount;                                                 // 0x20
    int32 MaxStackCount;                                              // 0x24
    float TotalWeight;                                                // 0x28
    uint8 ItemType;                                                   // 0x2C
}; // 0x30

struct SInventoryView {
    char pad0[0x00B0];
    TArray<FAbiotic_InventoryItemSlotStruct> CurrentInventory;        // 0x00B0
};

struct SActorPlayerView {
    char pad0[0x0A00];
    UAbiotic_InventoryComponent_C* CharacterInventory;                // 0x0A00
    char pad1[0x1730 - 0x0A08];
    UAbiotic_InventoryComponent_C* CharacterEquipSlotInventory;       // 0x1730
    UAbiotic_InventoryComponent_C* CharacterHotbarInventory;          // 0x1738
    char pad2[0x1B08 - 0x1740];
    FInventorySlotSelected_Struct Server_LastSelectedHotbarItem;      // 0x1B08
};

struct SDeployedContainerView {
    char pad0[0x0888];
    int32 Liquid_FillLevel;                                           // 0x0888
    int32 Liquid_MaxFill;                                             // 0x088C
    char pad1[0x0930 - 0x0890];
    bool InfiniteSource;                                              // 0x0930
};

// BP script-function params buffers follow natural C++ alignment, except the
// game's FName is align-4 (RC::Unreal::FName is align-8), so this one struct
// needs pack(4) to place OptionalItemRow at 0x14. Offsets verified at runtime.
#pragma pack(push, 4)
struct FServer_TryChangeValueInLiquidContainer_Params {
    UAbiotic_InventoryComponent_C* Inventory;                         // 0x00
    int32 SlotIndex;                                                  // 0x08
    int32 NewLiquidValue;                                             // 0x0C
    TEnumAsByte<E_LiquidType::Type> LiquidType;                       // 0x10
    FName OptionalItemRow;                                            // 0x14
    bool Success;                                                     // 0x1C
};
#pragma pack(pop)

struct FTryFill_TO_PlayerContainer_Params {
    AAbiotic_PlayerCharacter_C* PlayerCharacter;                      // 0x00
    bool OnlyCheck;                                                   // 0x08
    bool Success;                                                     // 0x09
    void* WarningMessage;                                             // 0x10 (FText*, opaque)
};

struct FTryFill_FROM_PlayerContainer_Params {
    AAbiotic_PlayerCharacter_C* PlayerCharacter;                      // 0x00
    bool ChangeExistingLiquidType;                                    // 0x08
    TEnumAsByte<E_LiquidType::Type> NewLiquidType;                    // 0x09
    bool OnlyCheck;                                                   // 0x0A
    bool Success;                                                     // 0x0B
};

struct FServer_ModifyFillState_Params {
    TEnumAsByte<E_LiquidType::Type> Liquid;                           // 0x00
    int32 NewLiquidValue;                                             // 0x04
    bool SkipSave;                                                    // 0x08
    AAbiotic_PlayerCharacter_C* FillInstigator;                       // 0x10
};

// Native function (UAbioticFunctionLibrary::SortInventoryArray) — the detour
// keeps the natural layout: value array inline, out/ref params as pointers.
struct FSortInventoryArray_Params {
    TArray<FSortedItem> InventoryArray;                               // 0x00
    TArray<int32>* OutIndexesToClear;                                 // 0x10
};

struct FGetNewMaxStackSize_Params {
    int32 CurrentMaxStackSize;                                        // 0x00
    UObject* __WorldContext;                                          // 0x08
};

struct FIsInventoryFull_Params {
    bool IgnoreStackSize;                                               // 0x00
};

struct FAbioticItemCount_Struct {
    FDataTableRowHandle Item;                                          // 0x00
    int32 Count;                                                        // 0x10
    char pad_14[0x4];                                                   // native array stride 0x18
}; // 0x18

using FNativeItemCountView = FAbioticItemCount_Struct;

struct FAreItemsAvailable_Params {
    TArray<FAbioticItemCount_Struct> Items;                            // 0x00
    AAbiotic_PlayerCharacter_C* Player;                                // 0x10
};

struct FServer_TryCraftUpgrade_Params {
    AAbiotic_PlayerCharacter_C* Player;                                // 0x00
};

struct FGetItemInSlot_Params {
    UAbiotic_InventoryComponent_C* Inventory;                        // 0x00
    int32 Index;                                                     // 0x08
};

struct SCraftingBenchView {
    char pad0[0x0878];
    UAbiotic_InventoryComponent_C* UpgradeInventory;                 // 0x0878
};

// TryPlaceIteminInventorySlot: value params (the item being placed + target slot)
struct FTryPlaceIteminInventorySlot_Params {
    FDataTableRowHandle DataTableRowHandle;                          // 0x00
    FAbiotic_InventoryChangeableDataStruct ChangeableData;           // 0x10
    int32 TargetIndex;                                               // 0x98
    bool CheckOnly;                                                  // 0x9C
    bool IsEquippingGear_;                                           // 0x9D
    int32* Remaining;                                                // 0xA0 (out)
    bool* Success;                                                   // 0xA8 (out)
    FAbiotic_InventoryItemSlotStruct* CurrentItem;                   // 0xB0 (out)
};

static_assert(sizeof(FDataTableRowHandle) == 0x10, "FDataTableRowHandle layout");
static_assert(sizeof(FAbioticItemCount_Struct) == 0x18, "Abiotic item count layout");
static_assert(sizeof(FNativeItemCountView) == 0x18, "Native item count layout");
static_assert(sizeof(FAbiotic_InventoryChangeableDataStruct) == 0x88, "changeable data size");
static_assert(sizeof(FAbiotic_InventoryItemSlotStruct) == 0x98, "slot size");
static_assert(sizeof(FInventorySlotSelected_Struct) == 0xC, "selected struct size");
static_assert(sizeof(FSortedItem) == 0x30, "FSortedItem size");
static_assert(offsetof(FAbiotic_InventoryChangeableDataStruct, CurrentStack_9) == 0x20, "CurrentStack offset");
static_assert(offsetof(FAbiotic_InventoryChangeableDataStruct, LiquidLevel_46) == 0x28, "LiquidLevel offset");
static_assert(offsetof(FAbiotic_InventoryChangeableDataStruct, TextureVariantRow_28) == 0x30, "TextureVariant offset");
static_assert(offsetof(FSAItemRow, StackSize_47) == 0x238, "StackSize offset");
static_assert(offsetof(FSAItemRow, EquipmentData_100) == 0x3F0, "EquipmentData offset");
static_assert(offsetof(SInventoryView, CurrentInventory) == 0xB0, "CurrentInventory offset");
static_assert(offsetof(SDeployedContainerView, Liquid_FillLevel) == 0x888, "Liquid_FillLevel offset");
static_assert(offsetof(SDeployedContainerView, InfiniteSource) == 0x930, "InfiniteSource offset");
static_assert(offsetof(SActorPlayerView, CharacterHotbarInventory) == 0x1738, "CharacterHotbar offset");
static_assert(offsetof(SActorPlayerView, Server_LastSelectedHotbarItem) == 0x1B08, "Server_LastSelected offset");
