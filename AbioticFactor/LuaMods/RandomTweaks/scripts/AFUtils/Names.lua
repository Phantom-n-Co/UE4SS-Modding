--[[
    Author: Igromanru
    Created Date: 19.08.2024
    Description: Utility functions for the game Abiotic Factor
]]

require("AFUtils.AFBase")

local UEHelpers = require("UEHelpers")

---- Buffs ----
AFUtils.DebuffLegSprain = UEHelpers.FindFName("Debuff_LegSprain")
AFUtils.DebuffRecentLegInjury = UEHelpers.FindFName("Debuff_RecentLegInjury")
AFUtils.DebuffLegFracture = UEHelpers.FindFName("Debuff_LegFracture")
AFUtils.DebuffLegBroken = UEHelpers.FindFName("Debuff_LegBroken")
AFUtils.DebuffDamaged = UEHelpers.FindFName("Debuff_Damaged")
AFUtils.DebuffInCombat = UEHelpers.FindFName("Debuff_InCombat")
AFUtils.DebuffBleeding = UEHelpers.FindFName("Debuff_Bleeding")
AFUtils.DebuffUnderwater = UEHelpers.FindFName("Debuff_Underwater")
AFUtils.DebuffWading = UEHelpers.FindFName("Debuff_Wading")
AFUtils.DebuffDrowning = UEHelpers.FindFName("Debuff_Drowning")

---- Food ----
AFUtils.IceCreamName = UEHelpers.FindFName("icecream")
AFUtils.FoodGreyebName = UEHelpers.FindFName("food_greyeb")

---- NPCs ----
AFUtils.LeyakRowName = UEHelpers.FindFName("Leyak")
AFUtils.KrasueRowName = UEHelpers.FindFName("Krasue")

return AFUtils