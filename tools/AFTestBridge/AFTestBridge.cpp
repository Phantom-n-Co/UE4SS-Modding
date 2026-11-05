#include "AFTestBridge.hpp"

#include <DynamicOutput/DynamicOutput.hpp>

// Unreal's wrapper: raw <Windows.h> macros (InterlockedCompareExchangePointer, MemoryBarrier) break later Unreal headers under clang-cl
#include <Unreal/Core/Windows/WindowsHWrapper.hpp>
// WindowsHWrapper sets WIN32_LEAN_AND_MEAN, so winsock.h was not pulled in and winsock2.h is safe here
#include <winsock2.h>

#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Constructs/Loop.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Core/Containers/FString.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace RC::Unreal;

namespace AFTestBridge {
namespace {

// ---- Text helpers (no Win32 NLS: WindowsHWrapper defines NOKERNEL et al.) ----

std::wstring fromUtf8(const std::string& s) {
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        auto c = static_cast<unsigned char>(s[i]);
        uint32_t cp;
        size_t len;
        if (c < 0x80) {
            cp = c, len = 1;
        } else if ((c >> 5) == 0x6) {
            cp = c & 0x1F, len = 2;
        } else if ((c >> 4) == 0xE) {
            cp = c & 0x0F, len = 3;
        } else if ((c >> 3) == 0x1E) {
            cp = c & 0x07, len = 4;
        } else {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        if (i + len > s.size()) {
            out.push_back(0xFFFD);
            break;
        }
        for (size_t k = 1; k < len; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        }
        i += len;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<wchar_t>(cp));
        }
    }
    return out;
}

std::string toUtf8(const std::wstring& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
            ++i;
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// JSON string literal (quoted, escaped) from UTF-16 text.
std::string js(const std::wstring& s) {
    std::string u = toUtf8(s);
    std::string out = "\"";
    for (unsigned char c : u) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                static const char* hex = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[c >> 4]);
                out.push_back(hex[c & 0xF]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out += "\"";
    return out;
}

std::string okReply(const std::string& resultJson) { return "{\"ok\":true,\"result\":" + resultJson + "}"; }
std::string errReply(const std::wstring& msg) { return "{\"ok\":false,\"error\":" + js(msg) + "}"; }

std::wstring fstr(const FString& s) { return std::wstring(*s); }

// ---- Game-thread command handlers ----

uint64_t g_ticks = 0;

// True for CDOs, archetypes and templates owned by a class (e.g. widget-tree
// templates inside a WidgetBlueprintGeneratedClass, which carry no archetype flag).
bool isTemplate(UObject* obj) {
    if (obj->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) {
        return true;
    }
    for (UObject* outer = obj->GetOuterPrivate(); outer; outer = outer->GetOuterPrivate()) {
        if (outer->IsA<UClass>() || outer->HasAnyFlags(RF_ClassDefaultObject)) {
            return true;
        }
    }
    return false;
}

std::vector<UObject*> liveInstances(const std::wstring& className) {
    std::vector<UObject*> all;
    UObjectGlobals::FindAllOf(className, all);
    std::erase_if(all, [](UObject* obj) { return !obj || isTemplate(obj); });
    return all;
}

UObject* firstLive(const std::wstring& className) {
    std::vector<UObject*> live = liveInstances(className);
    return live.empty() ? nullptr : live.front();
}

// "player" | "pc" | "first:<ClassName>" | "named:<ClassName>:<ObjectName>" | full object path ("/Game/....Name"),
// optionally followed by "->Prop" steps that follow object properties
// (e.g. "first:W_HostGameSetup_C->TextInput_WorldName").
UObject* resolveBase(const std::wstring& target) {
    if (target == L"player") {
        return firstLive(STR("Abiotic_PlayerCharacter_C"));
    }
    if (target == L"pc") {
        return firstLive(STR("PlayerController"));
    }
    if (target.starts_with(L"first:")) {
        return firstLive(target.substr(6));
    }
    if (target.starts_with(L"named:")) {
        // named:<ClassName>:<ObjectName>, for objects whose paths can't be looked up (widgets)
        const size_t colon = target.find(L':', 6);
        if (colon == std::wstring::npos) {
            return nullptr;
        }
        const std::wstring objName = target.substr(colon + 1);
        for (UObject* obj : liveInstances(target.substr(6, colon - 6))) {
            if (obj->GetName() == objName) {
                return obj;
            }
        }
        return nullptr;
    }
    return UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, target.c_str());
}

UObject* resolveTarget(const std::wstring& target) {
    size_t arrow = target.find(L"->");
    UObject* obj = resolveBase(target.substr(0, arrow));
    while (obj && arrow != std::wstring::npos) {
        const size_t start = arrow + 2;
        arrow = target.find(L"->", start);
        const std::wstring prop = target.substr(start, arrow == std::wstring::npos ? std::wstring::npos : arrow - start);
        auto* objProp = CastField<FObjectPropertyBase>(obj->GetPropertyByNameInChain(prop.c_str()));
        obj = objProp ? objProp->GetObjectPropertyValue(objProp->ContainerPtrToValuePtr<void>(obj)) : nullptr;
    }
    return obj;
}

std::string cmdPing() {
    return okReply("{\"pong\":true,\"ticks\":" + std::to_string(g_ticks) + "}");
}

// find <ClassName>: full names of live instances (templates excluded), max 100.
std::string cmdFind(const std::wstring& className) {
    std::vector<UObject*> live = liveInstances(className);
    std::string list = "[";
    for (size_t i = 0; i < live.size() && i < 100; ++i) {
        list += (i ? "," : "") + js(live[i]->GetFullName());
    }
    list += "]";
    return okReply("{\"count\":" + std::to_string(live.size()) + ",\"objects\":" + list + "}");
}

// KismetSystemLibrary::ExecuteConsoleCommand(WorldContextObject, Command, SpecificPlayer)
// via ProcessEvent on the library CDO. Output of the command is not captured.
std::string cmdConsole(const std::wstring& command) {
    UObject* pc = firstLive(STR("PlayerController"));
    if (!pc) {
        return errReply(L"no PlayerController");
    }
    auto* lib = UObjectGlobals::StaticFindObject<UObject*>(
        nullptr, nullptr, STR("/Script/Engine.Default__KismetSystemLibrary"));
    UFunction* fn = lib ? lib->GetFunctionByNameInChain(STR("ExecuteConsoleCommand")) : nullptr;
    if (!fn) {
        return errReply(L"KismetSystemLibrary:ExecuteConsoleCommand not found");
    }
    std::vector<uint8_t> params(fn->GetStructureSize());
    FString* cmdParam = nullptr;
    for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
        void* at = params.data() + prop->GetOffset_ForInternal();
        std::wstring name = prop->GetName();
        if (name == L"WorldContextObject" || name == L"SpecificPlayer") {
            if (auto* objProp = CastField<FObjectPropertyBase>(prop)) {
                objProp->SetObjectPropertyValue(at, pc);
            }
        } else if (name == L"Command" && CastField<FStrProperty>(prop)) {
            cmdParam = new (at) FString(command.c_str());
        }
    }
    if (!cmdParam) {
        return errReply(L"ExecuteConsoleCommand has no FString Command param");
    }
    lib->ProcessEvent(fn, params.data());
    cmdParam->~FString();
    return okReply("{\"command\":" + js(command) + ",\"playerController\":" + js(pc->GetFullName()) + "}");
}

std::string cmdGet(const std::wstring& target, const std::wstring& propName) {
    UObject* obj = resolveTarget(target);
    if (!obj) {
        return errReply(L"target not found: " + target);
    }
    FProperty* prop = obj->GetPropertyByNameInChain(propName.c_str());
    if (!prop) {
        return errReply(L"property not found: " + propName + L" on " + obj->GetFullName());
    }
    FString text;
    prop->ExportTextItem(text, prop->ContainerPtrToValuePtr<void>(obj), nullptr, obj, 0);
    return okReply("{\"object\":" + js(obj->GetFullName()) +
                   ",\"property\":" + js(propName) +
                   ",\"type\":" + js(fstr(prop->GetCPPType())) +
                   ",\"value\":" + js(fstr(text)) + "}");
}

// Property names + C++ types of the target's class chain (discovery for `get`).
std::string cmdProps(const std::wstring& target) {
    UObject* obj = resolveTarget(target);
    if (!obj) {
        return errReply(L"target not found: " + target);
    }
    std::string list = "[";
    bool first = true;
    for (FProperty* prop : TFieldRange<FProperty>(obj->GetClassPrivate(), EFieldIterationFlags::IncludeSuper)) {
        if (!first) {
            list += ",";
        }
        first = false;
        list += "{\"name\":" + js(prop->GetName()) + ",\"type\":" + js(fstr(prop->GetCPPType())) + "}";
    }
    list += "]";
    return okReply("{\"object\":" + js(obj->GetFullName()) + ",\"properties\":" + list + "}");
}

// ---- Reflection helpers ----

FProperty* findField(UStruct* owner, std::wstring_view name) {
    if (!owner) {
        return nullptr;
    }
    for (FProperty* prop : TFieldRange<FProperty>(owner, EFieldIterationFlags::IncludeSuper)) {
        if (prop->GetName() == name) {
            return prop;
        }
    }
    return nullptr;
}

// Offset of a field nested in struct-typed fields, e.g. {"ChangeableData", "CurrentStack"};
// -1 if any step is missing. `leaf` receives the last property.
int32_t nestedOffset(UStruct* owner, std::initializer_list<const wchar_t*> path, FProperty** leaf = nullptr) {
    int32_t total = 0;
    size_t i = 0;
    for (const wchar_t* name : path) {
        FProperty* prop = findField(owner, name);
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
        } else if (leaf) {
            *leaf = prop;
        }
    }
    return total;
}

std::wstring exportValue(FProperty* prop, const void* valuePtr, UObject* owner) {
    FString text;
    prop->ExportTextItem(text, valuePtr, nullptr, owner, 0);
    return fstr(text);
}

// ---- call: UFunction via ProcessEvent, args as UE text (ImportText format) ----

// Initializes every param in a ProcessEvent buffer and destroys them again on
// scope exit, so FString/TArray params (including engine-filled out arrays) are freed.
struct ParamBuffer {
    UFunction* fn;
    std::vector<uint8_t> data;
    std::vector<FProperty*> params;

    explicit ParamBuffer(UFunction* f) : fn(f), data(static_cast<size_t>(f->GetStructureSize())) {
        for (FProperty* prop : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated)) {
            if (prop->HasAnyPropertyFlags(CPF_Parm)) {
                prop->InitializeValue_InContainer(data.data());
                params.push_back(prop);
            }
        }
    }
    ~ParamBuffer() {
        for (FProperty* prop : params) {
            prop->DestroyValue_InContainer(data.data());
        }
    }
    ParamBuffer(const ParamBuffer&) = delete;
    ParamBuffer& operator=(const ParamBuffer&) = delete;

    FProperty* find(const std::wstring& name) const {
        for (FProperty* prop : params) {
            if (prop->GetName() == name) {
                return prop;
            }
        }
        return nullptr;
    }
};

// Bools and numbers must be fully valid text; other types are left to ImportText.
bool plainValueValid(FProperty* prop, const std::wstring& value) {
    if (CastField<FBoolProperty>(prop)) {
        std::wstring v;
        for (wchar_t c : value) {
            v.push_back(static_cast<wchar_t>(std::towlower(c)));
        }
        return v == L"true" || v == L"false" || v == L"1" || v == L"0";
    }
    static const std::wstring_view kNumeric[] = {L"int8", L"int16", L"int32", L"int64", L"uint8",
                                                 L"uint16", L"uint32", L"uint64", L"float", L"double"};
    const std::wstring type = fstr(prop->GetCPPType());
    if (std::ranges::find(kNumeric, std::wstring_view(type)) == std::end(kNumeric)) {
        return true; // enums (uint8 with an enum), structs, names, objects: ImportText decides
    }
    if (CastField<FByteProperty>(prop) || value.empty()) {
        // FByteProperty may carry an enum and accept its names; only reject empty
        return !value.empty();
    }
    wchar_t* end = nullptr;
    std::wcstod(value.c_str(), &end);
    return end && *end == L'\0';
}

// Parses UE text into a property value. FString takes the raw text (its import
// without PPF_Delimited keeps quotes literally); bools/numbers are pre-validated.
bool importValue(FProperty* prop, void* at, const std::wstring& value, UObject* owner) {
    if (CastField<FStrProperty>(prop)) {
        *static_cast<FString*>(at) = FString(value.c_str());
        return true;
    }
    if (!plainValueValid(prop, value)) {
        return false;
    }
    // Object params: ImportText finds no actors/widgets by path here (and resolves
    // short names like "player" to unrelated classes), so the bridge's own target
    // syntax (player, pc, first:<Class>, paths, ->Prop) goes first.
    if (auto* objProp = CastField<FObjectProperty>(prop); objProp && !CastField<FClassProperty>(prop)) {
        if (UObject* target = value == L"None" ? nullptr : resolveTarget(value)) {
            objProp->SetObjectPropertyValue(at, target);
            return true;
        }
    }
    const wchar_t* end = prop->ImportText_Direct(value.c_str(), at, owner, 0, nullptr);
    return end && (value.empty() || end != value.c_str());
}

// set <target> <prop> <value>: writes an object property (no setters/OnRep run).
std::string cmdSet(const std::wstring& target, const std::wstring& propName, const std::wstring& value) {
    UObject* obj = resolveTarget(target);
    if (!obj) {
        return errReply(L"target not found: " + target);
    }
    FProperty* prop = obj->GetPropertyByNameInChain(propName.c_str());
    if (!prop) {
        return errReply(L"property not found: " + propName + L" on " + obj->GetFullName());
    }
    void* at = prop->ContainerPtrToValuePtr<void>(obj);
    if (!importValue(prop, at, value, obj)) {
        return errReply(L"cannot parse " + propName + L" (" + fstr(prop->GetCPPType()) + L") from: " + value);
    }
    return okReply("{\"object\":" + js(obj->GetFullName()) + ",\"property\":" + js(propName) +
                   ",\"value\":" + js(exportValue(prop, at, obj)) + "}");
}

// call <target> <Function> [Param=Value ...]; unset params stay default-initialized.
// Reply: every param after the call (inputs, outs and ReturnValue), exported as text.
std::string cmdCall(const std::vector<std::wstring>& args) {
    UObject* obj = resolveTarget(args[1]);
    if (!obj) {
        return errReply(L"target not found: " + args[1]);
    }
    UFunction* fn = obj->GetFunctionByNameInChain(args[2].c_str());
    if (!fn) {
        return errReply(L"function not found: " + args[2] + L" on " + obj->GetFullName());
    }
    ParamBuffer buf(fn);
    for (size_t i = 3; i < args.size(); ++i) {
        const size_t eq = args[i].find(L'=');
        if (eq == std::wstring::npos) {
            return errReply(L"argument is not Name=Value: " + args[i]);
        }
        const std::wstring name = args[i].substr(0, eq);
        const std::wstring value = args[i].substr(eq + 1);
        FProperty* prop = buf.find(name);
        if (!prop) {
            std::wstring known;
            for (FProperty* p : buf.params) {
                known += (known.empty() ? L"" : L", ") + p->GetName();
            }
            return errReply(L"no param " + name + L" (params: " + known + L")");
        }
        if (!importValue(prop, prop->ContainerPtrToValuePtr<void>(buf.data.data()), value, obj)) {
            return errReply(L"cannot parse " + name + L" (" + fstr(prop->GetCPPType()) + L") from: " + value);
        }
    }
    obj->ProcessEvent(fn, buf.data.data());
    std::string params = "{";
    for (size_t i = 0; i < buf.params.size(); ++i) {
        FProperty* prop = buf.params[i];
        params += (i ? "," : "") + js(prop->GetName()) + ":" +
                  js(exportValue(prop, prop->ContainerPtrToValuePtr<void>(buf.data.data()), obj));
    }
    params += "}";
    return okReply("{\"object\":" + js(obj->GetFullName()) + ",\"function\":" + js(fn->GetName()) +
                   ",\"params\":" + params + "}");
}

// ---- Item tables (rows with struct /Script/AbioticFactor.AbioticInventoryItem) ----

struct ItemTables {
    std::vector<UDataTable*> tables;  // every loaded item table, ItemTable_Global first if loaded
    int32_t stackSize = -1;           // FAbioticInventoryItem::StackSize
    int32_t equipSlot = -1;           // FAbioticInventoryItem::EquipmentData.EquipSlot (uint8 enum)
};

ItemTables findItemTables() {
    ItemTables out;
    auto* rowStruct = UObjectGlobals::StaticFindObject<UScriptStruct*>(
        nullptr, nullptr, STR("/Script/AbioticFactor.AbioticInventoryItem"));
    if (!rowStruct) {
        return out;
    }
    out.stackSize = nestedOffset(rowStruct, {STR("StackSize")});
    FProperty* equipLeaf = nullptr;
    out.equipSlot = nestedOffset(rowStruct, {STR("EquipmentData"), STR("EquipSlot")}, &equipLeaf);
    if (equipLeaf && equipLeaf->GetElementSize() != 1) {
        out.equipSlot = -1;
    }
    UObjectGlobals::ForEachUObject([&](UObject* obj, int32_t, int32_t) -> RC::LoopAction {
        if (obj->IsA<UDataTable>() && !obj->HasAnyFlags(RF_ClassDefaultObject)) {
            auto* dt = static_cast<UDataTable*>(obj);
            if (dt->GetRowStruct().Get() == rowStruct) {
                out.tables.push_back(dt);
            }
        }
        return RC::LoopAction::Continue;
    });
    std::ranges::stable_partition(out.tables, [](UDataTable* dt) {
        return dt->GetPathName().ends_with(L".ItemTable_Global");
    });
    return out;
}

// Max stack of a row as the game sees it: ItemTable_Global first, else the first
// item table containing the row. -1 if not found.
int32_t maxStackOf(const ItemTables& t, FName row) {
    if (t.stackSize < 0 || row.IsNone()) {
        return -1;
    }
    for (UDataTable* dt : t.tables) {
        if (uint8_t* data = dt->FindRowUnchecked(row)) {
            return *reinterpret_cast<const int32_t*>(data + t.stackSize);
        }
    }
    return -1;
}

// items: every row of every loaded item table as [row, StackSize, EquipSlot].
std::string cmdItems() {
    ItemTables t = findItemTables();
    if (t.stackSize < 0 || t.equipSlot < 0) {
        return errReply(L"AbioticInventoryItem layout not resolved (StackSize / EquipmentData.EquipSlot)");
    }
    std::string tables = "[";
    for (size_t i = 0; i < t.tables.size(); ++i) {
        UDataTable* dt = t.tables[i];
        std::string rows = "[";
        TArray<FName> names = dt->GetRowNames();
        bool first = true;
        for (int32_t r = 0; r < names.Num(); ++r) {
            uint8_t* row = dt->FindRowUnchecked(names[r]);
            if (!row) {
                continue;
            }
            rows += (first ? "[" : ",[") + js(names[r].ToString()) + "," +
                    std::to_string(*reinterpret_cast<const int32_t*>(row + t.stackSize)) + "," +
                    std::to_string(row[t.equipSlot]) + "]";
            first = false;
        }
        rows += "]";
        tables += (i ? "," : "") + std::string("{\"path\":") + js(dt->GetPathName()) + ",\"rows\":" + rows + "}";
    }
    tables += "]";
    return okReply("{\"tables\":" + tables + "}");
}

// ---- inventory: CurrentInventory slots of an inventory component ----

// JSON for one component, or nullopt if `inv` has no CurrentInventory array of structs.
std::optional<std::string> inventoryJson(UObject* inv, const std::wstring& label, const ItemTables& t) {
    auto* arr = CastField<FArrayProperty>(inv->GetPropertyByNameInChain(STR("CurrentInventory")));
    auto* inner = arr ? CastField<FStructProperty>(arr->GetInner()) : nullptr;
    if (!inner) {
        return std::nullopt;
    }
    UScriptStruct* slotStruct = inner->GetStruct().Get();
    const int32_t rowOff = nestedOffset(slotStruct, {STR("ItemDataTable"), STR("RowName")});
    const int32_t stackOff = nestedOffset(slotStruct, {STR("ChangeableData"), STR("CurrentStack")});
    if (rowOff < 0 || stackOff < 0) {
        return std::nullopt;
    }
    const auto* slots = reinterpret_cast<const TArray<uint8_t>*>(arr->ContainerPtrToValuePtr<void>(inv));
    const auto stride = static_cast<size_t>(inner->GetElementSize());
    std::string list = "[";
    bool first = true;
    for (int32_t i = 0; i < slots->Num(); ++i) {
        const uint8_t* slot = slots->GetData() + stride * static_cast<size_t>(i);
        const FName row = *reinterpret_cast<const FName*>(slot + rowOff);
        if (row.IsNone()) {
            continue;
        }
        list += (first ? "" : ",") + std::string("{\"index\":") + std::to_string(i) +
                ",\"row\":" + js(row.ToString()) +
                ",\"stack\":" + std::to_string(*reinterpret_cast<const int32_t*>(slot + stackOff)) +
                ",\"maxStack\":" + std::to_string(maxStackOf(t, row)) + "}";
        first = false;
    }
    list += "]";
    return "{\"name\":" + js(label) + ",\"object\":" + js(inv->GetFullName()) +
           ",\"size\":" + std::to_string(slots->Num()) + ",\"slots\":" + list + "}";
}

// inventory <target>: the target itself if it is an inventory component, else
// every inventory component referenced by the target's object properties.
std::string cmdInventory(const std::wstring& target) {
    UObject* obj = resolveTarget(target);
    if (!obj) {
        return errReply(L"target not found: " + target);
    }
    const ItemTables t = findItemTables();
    if (auto self = inventoryJson(obj, obj->GetName(), t)) {
        return okReply("{\"inventories\":[" + *self + "]}");
    }
    std::string list = "[";
    std::vector<UObject*> seen;
    for (FProperty* prop : TFieldRange<FProperty>(obj->GetClassPrivate(), EFieldIterationFlags::IncludeSuper)) {
        auto* objProp = CastField<FObjectPropertyBase>(prop);
        if (!objProp || prop->GetArrayDim() != 1) {
            continue;
        }
        UObject* value = objProp->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<void>(obj));
        if (!value || std::ranges::find(seen, value) != seen.end()) {
            continue;
        }
        seen.push_back(value);
        if (auto inv = inventoryJson(value, prop->GetName(), t)) {
            list += (list.size() > 1 ? "," : "") + *inv;
        }
    }
    list += "]";
    return okReply("{\"object\":" + js(obj->GetFullName()) + ",\"inventories\":" + list + "}");
}

std::string dispatch(const std::vector<std::wstring>& args) {
    if (args.empty() || args[0].empty()) {
        return errReply(L"empty command");
    }
    const std::wstring& cmd = args[0];
    if (cmd == L"ping") {
        return cmdPing();
    }
    if (cmd == L"console" && args.size() == 2) {
        return cmdConsole(args[1]);
    }
    if (cmd == L"get" && args.size() == 3) {
        return cmdGet(args[1], args[2]);
    }
    if (cmd == L"props" && args.size() == 2) {
        return cmdProps(args[1]);
    }
    if (cmd == L"call" && args.size() >= 3) {
        return cmdCall(args);
    }
    if (cmd == L"inventory" && args.size() == 2) {
        return cmdInventory(args[1]);
    }
    if (cmd == L"items" && args.size() == 1) {
        return cmdItems();
    }
    if (cmd == L"find" && args.size() == 2) {
        return cmdFind(args[1]);
    }
    if (cmd == L"set" && args.size() == 4) {
        return cmdSet(args[1], args[2], args[3]);
    }
    return errReply(L"unknown command or wrong argument count: " + cmd +
                    L" (ping | console cmd | get target prop | set target prop value | props target"
                    L" | call target fn [Name=Value...] | inventory target | items | find ClassName; args tab-separated)");
}

// ---- Socket thread -> game thread queue ----

struct Job {
    std::vector<std::wstring> args;
    std::promise<std::string> reply;
};

std::mutex g_jobsMutex;
std::deque<std::shared_ptr<Job>> g_jobs;
std::atomic<bool> g_stop{false};
std::thread g_server;
std::optional<Hook::GlobalCallbackId> g_tickId;

constexpr auto kJobTimeout = std::chrono::seconds(10);

// Engine tick (game thread): run every queued command. UObject access is only
// safe here, never on the socket thread.
void drainJobs() {
    ++g_ticks;
    std::deque<std::shared_ptr<Job>> jobs;
    {
        std::lock_guard lock(g_jobsMutex);
        jobs.swap(g_jobs);
    }
    for (auto& job : jobs) {
        try {
            job->reply.set_value(dispatch(job->args));
        } catch (const std::exception& e) {
            const char* what = e.what();
            job->reply.set_value(errReply(L"exception: " + std::wstring(what, what + std::strlen(what))));
        }
    }
}

std::vector<std::wstring> splitTabs(const std::wstring& line) {
    std::vector<std::wstring> out;
    size_t start = 0;
    while (true) {
        size_t tab = line.find(L'\t', start);
        out.push_back(line.substr(start, tab == std::wstring::npos ? std::wstring::npos : tab - start));
        if (tab == std::wstring::npos) {
            break;
        }
        start = tab + 1;
    }
    return out;
}

std::string runOnGameThread(const std::string& line) {
    auto job = std::make_shared<Job>();
    job->args = splitTabs(fromUtf8(line));
    auto future = job->reply.get_future();
    {
        std::lock_guard lock(g_jobsMutex);
        g_jobs.push_back(job);
    }
    if (future.wait_for(kJobTimeout) != std::future_status::ready) {
        return errReply(L"timeout: game thread did not run the command (loading screen or hang?)");
    }
    return future.get();
}

// Waits up to 200 ms for `s` to become readable; false on timeout or error.
bool readable(SOCKET s) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv{0, 200 * 1000};
    return select(0, &set, nullptr, nullptr, &tv) > 0;
}

void sendAll(SOCKET s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) {
            return;
        }
        sent += static_cast<size_t>(n);
    }
}

// One client at a time; requests are newline-terminated, tab-separated UTF-8;
// each reply is one JSON line.
void serveClient(SOCKET client) {
    std::string buf;
    char chunk[4096];
    while (!g_stop) {
        if (!readable(client)) {
            continue;
        }
        int n = recv(client, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            return; // closed or error
        }
        buf.append(chunk, static_cast<size_t>(n));
        size_t nl;
        while ((nl = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, nl);
            buf.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            sendAll(client, runOnGameThread(line) + "\n");
        }
    }
}

void serverMain() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        RC::Output::send<RC::LogLevel::Error>(STR("[AFTestBridge] WSAStartup failed\n"));
        return;
    }
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // loopback only: the bridge runs arbitrary game calls
    if (listener == INVALID_SOCKET ||
        bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listener, 1) != 0) {
        RC::Output::send<RC::LogLevel::Error>(
            STR("[AFTestBridge] Cannot listen on 127.0.0.1:{} (WSA error {})\n"), kPort, WSAGetLastError());
        if (listener != INVALID_SOCKET) {
            closesocket(listener);
        }
        WSACleanup();
        return;
    }
    RC::Output::send<RC::LogLevel::Verbose>(STR("[AFTestBridge] Listening on 127.0.0.1:{}\n"), kPort);
    while (!g_stop) {
        if (!readable(listener)) {
            continue;
        }
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            continue;
        }
        serveClient(client);
        closesocket(client);
    }
    closesocket(listener);
    WSACleanup();
}

} // namespace

void start() {
    if (g_server.joinable()) {
        return;
    }
    g_stop = false;
    g_tickId = Hook::RegisterEngineTickPreCallback(
        [](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) { drainJobs(); },
        {false, false, STR("AFTestBridge"), STR("Dispatch")});
    g_server = std::thread(serverMain);
}

void stop() {
    g_stop = true;
    if (g_server.joinable()) {
        g_server.join();
    }
    if (g_tickId) {
        Hook::UnregisterCallback(*g_tickId);
        g_tickId.reset();
    }
}

} // namespace AFTestBridge
