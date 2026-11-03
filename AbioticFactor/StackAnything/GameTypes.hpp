#pragma once
#include <Unreal/UObject.hpp>
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
using RC::Unreal::int32;
using RC::Unreal::uint8;

// Mirrors of the game's native (/Script/AbioticFactor) inventory structs as of
// v1.5.0 (Modding Update). validateLayout() (StackAnything.cpp) checks every
// size and offset used here against the game's reflection data at startup and
// disables the hooks on a mismatch, so a game update logs an error instead of
// corrupting memory. Actor/widget fields are not mirrored at all: they are
// resolved by property name at runtime.

namespace ELiquidType {
    enum Type : uint8 { Empty = 0, Water = 1, Battery = 7, Laser = 12 };
}
namespace EInventorySlotType {
    enum Type : uint8 { EquipmentSlot_Hacker = 12, EquipmentSlot_Companion = 15 };
}

class UAbiotic_InventoryComponent_C;
class AAbiotic_PlayerCharacter_C;

// FRowHandle and its subclasses (FGlobalItemRowHandle, FTextureVariantRowHandle)
// are polymorphic: +0x00 is a vtable pointer, NOT a UDataTable*. Never
// dereference it; resolve rows by RowName (findItemRow in StackAnything.cpp).
struct FRowHandle {
    void* VTable;                                                     // 0x00
    FName RowName;                                                    // 0x08
}; // 0x10

// FAnyItemRowHandle : FMultiRowHandle : FRowHandle, plus EAnyItemTable Table.
struct FAnyItemRowHandle {
    FRowHandle Handle;                                                // 0x00
    uint8 Table;                                                      // 0x10
}; // 0x18

struct FAbioticInventoryChangeableData {
    FString AssetID;                                                  // 0x00
    double CurrentItemDurability;                                     // 0x10
    double MaxItemDurability;                                         // 0x18
    int32 CurrentStack;                                               // 0x20
    int32 CurrentAmmoInMagazine;                                      // 0x24
    int32 LiquidLevel;                                                // 0x28
    ELiquidType::Type CurrentLiquid;                                  // 0x2C
    FRowHandle TextureVariantRow;                                     // 0x30
    bool DynamicState;                                                // 0x40
    FString PlayerMadeString;                                         // 0x48
    char GameplayTags[0x20];                                          // 0x58
    TArray<uint8> DynamicProperties;                                  // 0x78
}; // 0x88

struct FAbioticInventoryItemSlot {
    FRowHandle ItemDataTable;                                         // 0x00 (FGlobalItemRowHandle)
    FAbioticInventoryChangeableData ChangeableData;                   // 0x10
}; // 0x98

// Blueprint user-defined struct (/Game/Blueprints/Data/InventorySlotSelected_Struct).
#pragma pack(push, 4)
struct FInventorySlotSelected_Struct {
    UAbiotic_InventoryComponent_C* Inventory;                         // 0x00
    int32 Index;                                                      // 0x08
}; // 0x0C
#pragma pack(pop)

struct FSortedItem {
    FName RowName;                                                    // 0x00
    int32 SourceIndex;                                                // 0x08
    char pad_0C[0x4];                                                 // 0x0C
    char ItemName[0x10];                                              // 0x10 (FText)
    int32 StackCount;                                                 // 0x20
    int32 MaxStackCount;                                              // 0x24
    float TotalWeight;                                                // 0x28
    uint8 ItemType;                                                   // 0x2C
}; // 0x30

// Recipe / upgrade cost entry (FAbioticItemCount, stride 0x18, before v1.5.0).
struct FAbioticItemInput {
    FAnyItemRowHandle Item;                                           // 0x00
    int32 Count;                                                      // 0x18
}; // 0x20

// Native function (UAbioticFunctionLibrary::SortInventoryArray) — the detour
// keeps the natural layout: value array inline, out/ref params as pointers.
struct FSortInventoryArray_Params {
    TArray<FSortedItem> InventoryArray;                               // 0x00
    TArray<int32>* OutIndexesToClear;                                 // 0x10
};

static_assert(sizeof(FRowHandle) == 0x10, "FRowHandle size");
static_assert(sizeof(FAnyItemRowHandle) == 0x18, "FAnyItemRowHandle size");
static_assert(sizeof(FAbioticInventoryChangeableData) == 0x88, "changeable data size");
static_assert(sizeof(FAbioticInventoryItemSlot) == 0x98, "slot size");
static_assert(sizeof(FInventorySlotSelected_Struct) == 0xC, "selected struct size");
static_assert(sizeof(FSortedItem) == 0x30, "FSortedItem size");
static_assert(sizeof(FAbioticItemInput) == 0x20, "FAbioticItemInput size");
