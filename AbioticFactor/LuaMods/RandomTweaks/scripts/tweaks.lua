local tweaks = {}

local UEHelpers = require("UEHelpers")

local PLAYER_CLASS_PATH = "/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C"
local PLAYER_BEGINPLAY_PATH = "/Game/Blueprints/Characters/Abiotic_PlayerCharacter.Abiotic_PlayerCharacter_C:ReceiveBeginPlay"
local NIGHT_LIGHT_Z = 1000.2581

local config = nil

-- Sets tweak values on a target object (CDO or live player instance).
-- Numeric entries nil -> skipped. Values mirror Convenience Tweaks v1.6.2.
--
-- Movement is stat-driven: the game's LocalUpdateWalkSpeed recomputes MaxWalkSpeed
-- from the NATIVE BaseWalkSpeed/BaseSprintSpeed (not the BP CurrentWalkSpeed), and
-- LocalUpdateJumpHeight recomputes JumpZVelocity from CurrentJumpHeight. So we set
-- the native base props and force a recompute on live players via RefreshLivePlayer.
---@param obj AAbiotic_PlayerCharacter_C
---@param cfg table
function tweaks.ApplyToObject(obj, cfg)
  if not obj or not cfg then return false end

  local ok, err = pcall(function()
    if cfg.walkSpeed ~= nil then
      obj.BaseWalkSpeed = cfg.walkSpeed      -- native, what LocalUpdateWalkSpeed reads
      obj.CurrentWalkSpeed = cfg.walkSpeed   -- BP mirror
    end
    if cfg.sprintSpeed ~= nil then
      obj.BaseSprintSpeed = cfg.sprintSpeed  -- native
    end
    if cfg.staminaDrainRate ~= nil then obj.Stamina_DrainRate = cfg.staminaDrainRate end
    if cfg.staminaRegainRate ~= nil then obj.Stamina_RegainRate = cfg.staminaRegainRate end
    if cfg.interactionRange ~= nil then obj.InteractionRange = cfg.interactionRange end
    if cfg.ladderClimbSpeed ~= nil then obj.LadderClimbSpeed = cfg.ladderClimbSpeed end
    if cfg.jumpHeight ~= nil then obj.CurrentJumpHeight = cfg.jumpHeight end
    if cfg.lightIntensity ~= nil then obj.Light_DefaultIntensity = cfg.lightIntensity end
    if cfg.lanternIntensity ~= nil then obj.Lantern_DefaultIntensity = cfg.lanternIntensity end
    if cfg.disableNightLight then obj.WristwatchPointLightLocation.Z = NIGHT_LIGHT_Z end
  end)

  return ok
end

-- Forces the game to re-read tweaked values on a live player instance.
---@param player AAbiotic_PlayerCharacter_C
function tweaks.RefreshLivePlayer(player)
  if not player then return end
  pcall(function() player:LocalUpdateWalkSpeed() end)
  pcall(function() player:LocalUpdateJumpHeight() end)
end

-- Applies to the player class CDO (defaults for future spawns) and, when a
-- ReceiveBeginPlay hook is available, to each player as it spawns.
---@param cfg table
---@param done fun(success: boolean)
function tweaks.ApplyAll(cfg, done)
  if not cfg then return done and done(false) end
  config = cfg

  local attempts = 0
  local function tryApply()
    LoadAsset(PLAYER_CLASS_PATH)

    local playerClass = StaticFindObject(PLAYER_CLASS_PATH)
    if playerClass and playerClass:IsValid() then
      -- CDO defaults for any spawn that bypasses the hook.
      tweaks.ApplyToObject(playerClass:GetCDO(), cfg)

      -- Apply on every player spawn (covers initial spawn, respawns, map loads).
      local ok, err = pcall(RegisterHook, PLAYER_BEGINPLAY_PATH, function(self)
        local player = self:get()
        if player and player:IsValid() then
          tweaks.ApplyToObject(player, cfg)
          tweaks.RefreshLivePlayer(player)
        end
      end)

      -- The game periodically recomputes the stat-driven values and reverts them,
      -- so keep re-applying to the live player (same pattern as the Speedhack mod).
      LoopAsync(250, function()
        ExecuteInGameThread(function()
          local p = UEHelpers.GetPlayer()
          if p and p:IsValid() then
            tweaks.ApplyToObject(p, cfg)
          end
        end)
        return true
      end)

      print("[Random Tweaks] Applied convenience tweaks to player defaults\n")
      done(ok and true or false)
      return
    end

    attempts = attempts + 1
    if attempts < 10 then
      ExecuteWithDelay(1000, function()
        ExecuteInGameThread(tryApply)
      end)
    else
      print("[Random Tweaks] WARNING: could not resolve player class, tweaks not applied\n")
      done(false)
    end
  end

  ExecuteInGameThread(tryApply)
end

return tweaks
