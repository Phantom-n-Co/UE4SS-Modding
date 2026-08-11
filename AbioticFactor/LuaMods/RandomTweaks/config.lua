-- RandomTweaks configuration
-- Edit values here to customize. nil = skip that tweak.
return {
  instantButcher = true,
  debugLogging = true,

  tweaks = {
    -- Movement. walkSpeed sets the native BaseWalkSpeed + CurrentWalkSpeed and is
    -- applied via ReceiveBeginPlay so the game recomputes from it on spawn.
    -- Add `sprintSpeed = <value>` to also override BaseSprintSpeed (vanilla 685).
    walkSpeed = 550,          -- vanilla 500
    sprintSpeed = 822,        -- vanilla 685
    staminaDrainRate = 0.25,  -- vanilla 0.2
    staminaRegainRate = 0.8,  -- vanilla 0.2
    interactionRange = 320,   -- vanilla 200
    ladderClimbSpeed = 357,   -- vanilla 275
    jumpHeight = 510,         -- vanilla 425
    lightIntensity = 8.4,     -- vanilla 8.0
    lanternIntensity = 5.45,  -- vanilla 4.75
    -- Moves the wristwatch point light far away (night lights off)
    disableNightLight = true,
  },
}
