require("AFUtils.AFBase")
require("AFUtils.AFUtils")
require("AFUtils.DefaultObjects")
require("AFUtils.Enums")
require("AFUtils.ObjectsGetter")
require("AFUtils.StaticClasses")

local tweaks = require("tweaks")

-- Set mod info for logging
ModName = "Random Tweaks"

-- Config is a dofile'd table (edit config.lua in the mod root). Root is derived
-- from this script's location so it works regardless of the UE4SS working dir.
local info = debug.getinfo(1, "S")
local src = info and info.source or ""
local modRoot = (src:gsub("\\", "/")):match("^@(.*/)[Ss]cripts/") or "Mods/RandomTweaks/"
local config = dofile(modRoot .. "config.lua")

local function log(message)
  if config.debugLogging then
    print("[Random Tweaks] " .. message .. "\n")
  end
end

ExecuteInGameThread(function()
  RegisterHook("/Game/Blueprints/Characters/Abiotic_Character_ParentBP.Abiotic_Character_ParentBP_C:ProcessDamage",
    ---@param self RemoteUnrealParam<AAbiotic_Character_ParentBP_C>
    ---@param Damage RemoteUnrealParam<double>
    ---@param DamageType RemoteUnrealParam<UAbiotic_DamageType_ParentBP_C>
    ---@param HitLocation RemoteUnrealParam<FVector>
    ---@param HitNormal RemoteUnrealParam<FVector>
    ---@param HitComponent RemoteUnrealParam<UPrimitiveComponent>
    ---@param BoneHitName RemoteUnrealParam<FName>
    ---@param DirectionOfSource RemoteUnrealParam<FVector>
    ---@param Instigator RemoteUnrealParam<AActor>
    ---@param DamageCauser RemoteUnrealParam<AActor>
    ---@param HitInfo RemoteUnrealParam<FHitResult>
    function(self, Damage, DamageType, HitLocation, HitNormal, HitComponent, BoneHitName, DirectionOfSource, Instigator,
             DamageCauser, HitInfo)
      if config.instantButcher then
        ---@type AActor
        local instigator = Instigator:get()
        if instigator == nil then
          log("instigator is nil, skipping instant butcher logic.")
          return
        end
        ---@type AActor
        local damageCauser = DamageCauser:get()
        if damageCauser == nil then
          log("DamageCauser is nil, skipping instant butcher logic.")
          return
        end

        ---@type UAbiotic_DamageType_ParentBP_C
        local damageType = DamageType:get()
        if damageType == nil then
          log("DamageType is nil, skipping instant butcher logic.")
          return
        end

        damageType.InstantGibType = true
      end
    end)
end)

-- Apply convenience tweaks to the player class defaults once at load.
tweaks.ApplyAll(config.tweaks, function(success)
  if success then
    log("Convenience tweaks applied.")
  else
    log("Convenience tweaks could not be applied.")
  end
end)
