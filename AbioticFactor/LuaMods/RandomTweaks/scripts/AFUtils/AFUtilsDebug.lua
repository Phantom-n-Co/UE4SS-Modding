
--[[
    Author: Igromanru
    Created Date: 19.08.2024
    Description: Utility functions for the game Abiotic Factor
]]

require("AFUtils.AFBase")
require("AFUtils.BaseUtils.LogDebug")

---Logs in debug scope all relevant properties of FTimerHandle to console 
---@param WorldContext UObject
---@param TimerHandle FTimerHandle
---@param Prefix string?
function AFUtils.LogTimerHandle(WorldContext, TimerHandle, Prefix)
    if not WorldContext or not TimerHandle or not TimerHandle.Handle then return end
    Prefix = Prefix or ""

    local timerHandle = { Handle = TimerHandle.Handle }
    local isTimerValid = GetKismetSystemLibrary():K2_IsValidTimerHandle(timerHandle)
    LogDebug(Prefix .. "IsValid: "..tostring(isTimerValid))
    if isTimerValid then
        LogDebug(Prefix .. "TimerExists: "..tostring(GetKismetSystemLibrary():K2_TimerExistsHandle(WorldContext, timerHandle)))
        LogDebug(Prefix .. "IsTimerActive: "..tostring(GetKismetSystemLibrary():K2_IsTimerActiveHandle(WorldContext, timerHandle)))
        LogDebug(Prefix .. "IsTimerPaused: "..tostring(GetKismetSystemLibrary():K2_IsTimerPausedHandle(WorldContext, timerHandle)))
        LogDebug(Prefix .. "Elapsed: "..GetKismetSystemLibrary():K2_GetTimerElapsedTimeHandle(WorldContext, timerHandle))
        LogDebug(Prefix .. "TimerRemaining: "..GetKismetSystemLibrary():K2_GetTimerRemainingTimeHandle(WorldContext, timerHandle))
    end
end

---@param Timeline FTimeline
---@param Prefix string?
function AFUtils.LogTimeline(Timeline, Prefix)
    if not Timeline then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "LengthMode (enum 0-1):", Timeline.LengthMode)
    LogDebug(Prefix .. "bLooping:", tostring(Timeline.bLooping))
    LogDebug(Prefix .. "bReversePlayback:", tostring(Timeline.bReversePlayback))
    LogDebug(Prefix .. "Length:", Timeline.Length)
    LogDebug(Prefix .. "Events.Num:", #Timeline.Events)
    LogDebug(Prefix .. "InterpVectors.Num:", #Timeline.InterpVectors)
    LogDebug(Prefix .. "InterpFloats.Num:", #Timeline.InterpFloats)
    LogDebug(Prefix .. "InterpLinearColors.Num:", #Timeline.InterpLinearColors)
    LogDebug(Prefix .. "DirectionPropertyName:", Timeline.DirectionPropertyName:ToString())
end

---@param TimelineComponent UTimelineComponent
---@param Prefix string?
function AFUtils.LogTimelineComponent(TimelineComponent, Prefix)
    if not TimelineComponent or not TimelineComponent:IsValid() then return end
    Prefix = Prefix or ""

    AFUtils.LogTimeline(TimelineComponent.TheTimeline, Prefix .. "TheTimeline.")
    LogDebug(Prefix .. "bIgnoreTimeDilation:", tostring(TimelineComponent.bIgnoreTimeDilation))
end

---@param CharacterBuffComponent UCharacterBuffComponent
---@param Prefix string?
function AFUtils.LogCharacterBuffComponent(CharacterBuffComponent, Prefix)
    if not CharacterBuffComponent or not CharacterBuffComponent:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "ImmuneToBuffs.Num:", #CharacterBuffComponent.ImmuneToBuffs)
    for i = 1, #CharacterBuffComponent.ImmuneToBuffs do
        local buffDebuffRowHandle = CharacterBuffComponent.ImmuneToBuffs[i] ---@type FBuffDebuffRowHandle
        LogDebug(Prefix .. string.format("ImmuneToBuffs[%d]: RowName: %s, DataTablePath: %s", i, buffDebuffRowHandle.RowName:ToString(), buffDebuffRowHandle.DataTablePath:ToString()))
    end
    LogDebug(Prefix .. "CurrentBuffs.Num:", #CharacterBuffComponent.CurrentBuffs)
    for i = 1, #CharacterBuffComponent.CurrentBuffs do
        local buffDebuffRowHandle = CharacterBuffComponent.CurrentBuffs[i] ---@type FBuffDebuffEntry
        LogDebug(Prefix .. string.format("CurrentBuffs[%d]: BuffRow.RowName: %s, BuffRow.DataTablePath: %s", i, buffDebuffRowHandle.BuffRow.RowName:ToString(), buffDebuffRowHandle.BuffRow.DataTablePath:ToString()))
        LogDebug(Prefix .. string.format("CurrentBuffs[%d]: ParentLimb: %s, BuffExpireTime: %f", i, AFUtils.BodyLimbsToString(buffDebuffRowHandle.ParentLimb), buffDebuffRowHandle.BuffExpireTime))
    end
    LogDebug(Prefix .. "BuffParticles.Num:", #CharacterBuffComponent.BuffParticles)
    LogDebug(Prefix .. "BuffMaterials.Num:", #CharacterBuffComponent.BuffMaterials)
    LogDebug(Prefix .. "BuffTints.Num:", #CharacterBuffComponent.BuffTints)
end

---Logs in debug scope all relevant properties of FAbiotic_InventoryChangeableDataStruct to console 
---@param ChangeableData FAbiotic_InventoryChangeableDataStruct
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogInventoryChangeableDataStruct(ChangeableData, Prefix)
    if not ChangeableData then return end
    Prefix = Prefix or ""

    if ChangeableData.AssetID_25_06DB7A12469849D19D5FC3BA6BEDEEAB then
        LogDebug(Prefix .. "AssetID:", ChangeableData.AssetID_25_06DB7A12469849D19D5FC3BA6BEDEEAB:ToString())
    end
    LogDebug(Prefix .. "CurrentItemDurability:", ChangeableData.CurrentItemDurability_4_24B4D0E64E496B43FB8D3CA2B9D161C8)
    LogDebug(Prefix .. "MaxItemDurability:", ChangeableData.MaxItemDurability_6_F5D5F0D64D4D6050CCCDE4869785012B)
    LogDebug(Prefix .. "CurrentStack:", ChangeableData.CurrentStack_9_D443B69044D640B0989FD8A629801A49)
    LogDebug(Prefix .. "CurrentAmmoInMagazine:", ChangeableData.CurrentAmmoInMagazine_12_D68C190F4B2FA78A4B1D57835B95C53D)
    LogDebug(Prefix .. "LiquidLevel:", ChangeableData.LiquidLevel_46_D6414A6E49082BC020AADC89CC29E35A)
    LogDebug(Prefix .. "CurrentLiquid (enum 0-13):", AFUtils.LiquidTypeToString(ChangeableData.CurrentLiquid_19_3E1652F448223AAE5F405FB510838109) .. " (" .. ChangeableData.CurrentLiquid_19_3E1652F448223AAE5F405FB510838109 .. ")")
end

---Logs in debug scope all relevant properties of FAbiotic_InventoryItemSlotStruct to console 
---@param InventoryItemSlotStruct FAbiotic_InventoryItemSlotStruct
---@param Prefix string?
function AFUtils.LogInventoryItemSlotStruct(InventoryItemSlotStruct, Prefix)
    if not InventoryItemSlotStruct then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "RowName:", InventoryItemSlotStruct.ItemDataTable_18_BF1052F141F66A976F4844AB2B13062B.RowName:ToString())
    if InventoryItemSlotStruct.ItemDataTable_18_BF1052F141F66A976F4844AB2B13062B.DataTable:IsValid() then
        LogDebug(Prefix .. "DataTable:", InventoryItemSlotStruct.ItemDataTable_18_BF1052F141F66A976F4844AB2B13062B.DataTable:GetFullName())
    end
    AFUtils.LogInventoryChangeableDataStruct(InventoryItemSlotStruct.ChangeableData_12_2B90E1F74F648135579D39A49F5A2313, Prefix .. "ChangeableData.")
end

---Logs in debug scope all relevant properties of FAbiotic_LiquidStruct to console 
---@param LiquidStruct FAbiotic_LiquidStruct
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogLiquidStruct(LiquidStruct, Prefix)
    if not LiquidStruct then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "MaxLiquid:", LiquidStruct.MaxLiquid_16_80D4968B4CACEDD3D4018E87DA67E8B4)
    LogDebug(Prefix .. "AllowedLiquids.Num:", #LiquidStruct.AllowedLiquids_7_1DF3EB8C43F49DA3A1E4A2AF908148D3)
    for i = 1, #LiquidStruct.AllowedLiquids_7_1DF3EB8C43F49DA3A1E4A2AF908148D3 do
        local liquiedType = LiquidStruct.AllowedLiquids_7_1DF3EB8C43F49DA3A1E4A2AF908148D3[i]
        LogDebug(Prefix .. string.format("AllowedLiquids[%d]: %s (%d)", i, AFUtils.LiquidTypeToString(liquiedType), liquiedType))
    end
    LogDebug(Prefix .. "PercentageLiquidToStartWith:", LiquidStruct.PercentageLiquidToStartWith_11_835A4C4F440C319874D3EFA75CAFA4C5)
    LogDebug(Prefix .. "LiquidToStartWith.Num:", #LiquidStruct.LiquidToStartWith_15_F7D753A24D2130B92AF312AB9192AD9C)
    for i = 1, #LiquidStruct.LiquidToStartWith_15_F7D753A24D2130B92AF312AB9192AD9C do
        local liquiedType = LiquidStruct.LiquidToStartWith_15_F7D753A24D2130B92AF312AB9192AD9C[i]
        LogDebug(Prefix .. string.format("LiquidToStartWith[%d]:", i), liquiedType)
    end
end

---Logs in debug scope all relevant properties of FAbiotic_WeaponStruct to console 
---@param WeaponStruct FAbiotic_WeaponStruct
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogWeaponStruct(WeaponStruct, Prefix)
    if not WeaponStruct then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "Melee:", tostring(WeaponStruct.Melee_1_AB17935A4F944DCEEB1AB3A5B598E702))
    LogDebug(Prefix .. "MeleeSwingData.RowName:", WeaponStruct.MeleeSwingData_80_EA5C63F44178A08106BE41B8F7D8DE36.RowName:ToString())
    LogDebug(Prefix .. "TimeBetweenShots:", tostring(WeaponStruct.TimeBetweenShots_8_71ACC9414B36314DEF34B3A54649941C))
    LogDebug(Prefix .. "MaximumHitscanRange:", tostring(WeaponStruct.MaximumHitscanRange_26_F36D29CA48831A6C3AD49EB94F5D2BE2))
    LogDebug(Prefix .. "DamagePerHit:", tostring(WeaponStruct.DamagePerHit_16_F95199D1425C37191C55CDA0DC07BDDC))
    LogDebug(Prefix .. "BulletSpread_Min:", tostring(WeaponStruct.BulletSpread_Min_38_08ADC0BA4BEA02135BE0438A60AE5725))
    LogDebug(Prefix .. "BulletSpread_Max:", tostring(WeaponStruct.BulletSpread_Max_39_4AE2E5744A934A3FFCEC2A9D7A1A6963))
    LogDebug(Prefix .. "RecoilAmount:", tostring(WeaponStruct.RecoilAmount_42_85AFA9834A1CABF8183C088D857840EE))
    LogDebug(Prefix .. "PelletCount:", tostring(WeaponStruct.PelletCount_77_4504318146345E7029C78790B317E074))
    LogDebug(Prefix .. "MagazineSize:", tostring(WeaponStruct.MagazineSize_57_E890A3944240BB8D07EF0B9251F1FBD4))
    LogDebug(Prefix .. "RequireAmmo:", tostring(WeaponStruct.RequireAmmo_85_8BB1C1954D2A83BB1994549DDEEBA306))
    LogDebug(Prefix .. "AmmoType.RowName:", WeaponStruct.AmmoType_54_D19EDD9E48E4252D492757BFAAC23A73.RowName:ToString())
    LogDebug(Prefix .. "AmmoTypes.Num:", #WeaponStruct.AmmoTypes_99_6514B50249092C020B24F9ABF9AF0E37)
    LogDebug(Prefix .. "SecondaryAttack (enum 0-5):", WeaponStruct.SecondaryAttack_82_0ADE2DC74388F34F125F0DB6D9AAC1CD)
    LogDebug(Prefix .. "LoudnessOnPrimaryUse:", WeaponStruct.LoudnessOnPrimaryUse_74_7829648A4C3F44A62DCA09B3817DF796)
    LogDebug(Prefix .. "LoudnessOnSecondaryUse:", WeaponStruct.LoudnessOnSecondaryUse_73_89AB59C84EBC77DEB5DD2C9C88E9C237)
    LogDebug(Prefix .. "UnderwaterState (enum 0-2):", WeaponStruct.UnderwaterState_95_972463794CBCCEA48AC987A7FA4C0118)
    LogDebug(Prefix .. "BurstFireCount:", WeaponStruct.BurstFireCount_102_880FB81B4CA5EC7F7B5D6FBDE68275B8)
end

---Logs in debug scope all relevant properties of FAbiotic_InventoryItemStruct to console 
---@param InventoryItemStruct FAbiotic_InventoryItemStruct
---@param Prefix string?
function AFUtils.LogInventoryItemStruct(InventoryItemStruct, Prefix)
    if not InventoryItemStruct then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "ItemName:", InventoryItemStruct.ItemName_51_B88648C048EE5BC2885E4E95F3E13F0A:ToString())
    LogDebug(Prefix .. "ItemDescription:", InventoryItemStruct.ItemDescription_38_E5F7B38A4F3C41EB9DA52CA14654D303:ToString())
    LogDebug(Prefix .. "ItemFlavorText:", InventoryItemStruct.ItemFlavorText_39_12D05DD74EA145A5E7D1159C7F326177:ToString())
    -- LogDebug(Prefix .. "ToInteractWith_Text:", InventoryItemStruct.ToInteractWith_Text_66_C2148289464D5AAA4D19BBA13F15FE41:ToString())
    -- LogDebug(Prefix .. "ToLongInteractWith_Text:", InventoryItemStruct.ToLongInteractWith_Text_68_4FBE88F341B6A020E3216CA026A1E4E8:ToString())
    -- LogDebug(Prefix .. "ToPackage_Text:", InventoryItemStruct.ToPackage_Text_71_5094104748FCB4BD2F90C99A2C4C49A8:ToString())
    -- LogDebug(Prefix .. "ToLongPackage_Text:", InventoryItemStruct.ToLongPackage_Text_72_CB77853E49960F43E6422C90DC967508:ToString())
    -- LogDebug(Prefix .. "Scale_WorldMesh:", InventoryItemStruct.Scale_WorldMesh_143_AF66D856410026FCC19E70AC421B3667)
    -- LogDebug(Prefix .. "Scale_FirstPersonMesh:", InventoryItemStruct.Scale_FirstPersonMesh_146_6AFCDB94484AE7625E73C6AFB835D21F)
    -- LogDebug(Prefix .. "Scale_TPHeldMesh:", InventoryItemStruct.Scale_TPHeldMesh_145_6826D00A4F30AEDBF62E02892E4261E6)
    LogDebug(Prefix .. "PlacementOrientationsAllowed enum:", InventoryItemStruct.PlacementOrientationsAllowed_122_75894D7C4B93F103C06AB18421167757)
    LogDebug(Prefix .. "CanLoseDurability:", tostring(InventoryItemStruct.CanLoseDurability_29_42EA515F4AC1EC69D8480DB36C01D5E1))
    LogDebug(Prefix .. "MaxItemDurability:", InventoryItemStruct.MaxItemDurability_31_6EBCEFC943F9E85DE9350BBC0E249447)
    LogDebug(Prefix .. "ChanceToLoseDurability:", InventoryItemStruct.ChanceToLoseDurability_97_DF967C464092E2E6AEEBFE84C536ACAB)
    -- ToDo log FAbiotic_ItemDropStruct, InventoryItemStruct.RepairItem_106_C2E89B0B4B7F39FC3B2B828BBD13A391
    LogDebug(Prefix .. "StackSize:", InventoryItemStruct.StackSize_47_D124F11B4B6D9766B2B33699795845A9)
    LogDebug(Prefix .. "Weight:", InventoryItemStruct.Weight_119_CE7DB430417207921D739CAF458D4D7C)
    LogDebug(Prefix .. "TryPlaceInHotbar:", tostring(InventoryItemStruct.TryPlaceInHotbar_128_F78F6AA34A238AEAB278F48066C080BF))
    LogDebug(Prefix .. "IsWeapon:", tostring(InventoryItemStruct.IsWeapon_63_57F6A703413EA260B1455CA81F2D4911))
    AFUtils.LogWeaponStruct(InventoryItemStruct.WeaponData_61_3C29CF6C4A7F9DD435F9318FEE4B033D, Prefix .. "WeaponData.")
    -- ToDo log FAbiotic_Equipment_Struct, InventoryItemStruct.EquipmentData_100_576D05464F36104AFE501B878255E318
    -- ToDo log FAbiotic_Consumable_Struct, InventoryItemStruct.ConsumableData_84_757B6B114FF23016981BEF888A31C670
    -- ToDo log FAbiotic_CookableStruct, InventoryItemStruct.CookableData_94_7EFD1F0A4A7EFB44D3D8B9B14581BF36
    AFUtils.LogLiquidStruct(InventoryItemStruct.LiquidData_110_4D07F09C483C1E65B39024ABC7032FA0, Prefix .. "LiquidData.")
    LogDebug(Prefix .. "StrippedFromBuild:", tostring(InventoryItemStruct.StrippedFromBuild_149_8F18A38A4BB983B801C379B47FC5D5A9))
end

---@param SaveDataDeployableStruct FSaveData_Deployable_Struct
---@param Prefix string?
function AFUtils.LogSaveDataDeployableStruct(SaveDataDeployableStruct, Prefix)
    if not SaveDataDeployableStruct then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "Class:", SaveDataDeployableStruct.Class_77_84FAE6234D772064CD9B659BA5046B1C)
    -- LogDebug(Prefix .. "ActorPath:", SaveDataDeployableStruct.ActorPath_164_90AA6DAB481E5DC2E125A3A94475F44D)
    AFUtils.LogInventoryChangeableDataStruct(SaveDataDeployableStruct.ChangableData_37_6153F4A94F01A776C108038B7F38E256, Prefix .. "ChangableData.")
    LogDebug(Prefix .. "DeployableDestroyed:", SaveDataDeployableStruct.DeployableDestroyed_56_80BF5DDE46C5F8C6E6CD9EBF6A695E5E)
    LogDebug(Prefix .. "BrokeWhenPackaged:", SaveDataDeployableStruct.BrokeWhenPackaged_63_852033BB4713434A14C0D5B5792BA116)
    LogDebug(Prefix .. "HasBeenPackaged:", SaveDataDeployableStruct.HasBeenPackaged_59_9C1C3E4D4D61B7BC4E7D13A1B993E1B0)
    -- LogDebug(Prefix .. "Transform:", TransformToString(SaveDataDeployableStruct.Transform_50_85E8B13D40141C9B1308F4BB943BD753))
    LogDebug(Prefix .. "DeployedByPlayer:", SaveDataDeployableStruct.DeployedByPlayer_71_EA4E6F5C4DBE9C472BC1D1B3ADEE0205)
    LogDebug(Prefix .. "ConstructionMode:", SaveDataDeployableStruct.ConstructionMode_82_B226CF9D4E57045A9835B39D8D7AF98D)
    LogDebug(Prefix .. "ConstructionLevel:", SaveDataDeployableStruct.ConstructionLevel_85_460528D64DD6D1712C19198BC316254B)
    LogDebug(Prefix .. "ContainerInventories Num:", #SaveDataDeployableStruct.ContainerInventories_110_3A680B7244ACB095D963B786D9BB6ECB)
    -- ToDo ContainerInventories_110_3A680B7244ACB095D963B786D9BB6ECB array
    LogDebug(Prefix .. "ActiveSeats Num:", #SaveDataDeployableStruct.ActiveSeats_135_E030A01B4CB15C1F95700EA3945F2A85)
    LogDebug(Prefix .. "ItemProxies Num:", #SaveDataDeployableStruct.ItemProxies_149_E2E145CE4015C4EDFA89E2B0CE3F579A)
    LogDebug(Prefix .. "CustomTextDisplay:", SaveDataDeployableStruct.CustomTextDisplay_152_B59A50C74001B5D2234D9E9B0D7CAB7F:ToString())
    LogDebug(Prefix .. "FoundByPlayer:", SaveDataDeployableStruct.FoundByPlayer_154_B3A0D3F6458C7DAD36E130B39DAEDBE3)
    LogDebug(Prefix .. "Supports Num:", #SaveDataDeployableStruct.Supports_158_FE0D33184131D1E1C73782B44057EB5C)
    LogDebug(Prefix .. "NoResetVignette:", SaveDataDeployableStruct.NoResetVignette_161_C76AFFC84B04AA28B73A65836D6BB265)
end

---@param CookingStruct FAbiotic_Cooking_Struct
---@param Prefix string?
function AFUtils.LogCookingStruct(CookingStruct, Prefix)
    if not CookingStruct then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "Ingredient.Num:", #CookingStruct.Ingredient_26_75A231F54D4ED926835029AE13B322A3)
    for i = 1, #CookingStruct.Ingredient_26_75A231F54D4ED926835029AE13B322A3 do
        local ingredient = CookingStruct.Ingredient_26_75A231F54D4ED926835029AE13B322A3[i]
        LogDebug(Prefix .. string.format("Ingredient[%d].Item:", i), ingredient.Item_5_5AD3D6B1470ED45BCB2D15BC84BB0F1A.RowName:ToString())
        LogDebug(Prefix .. string.format("Ingredient[%d].Count:", i), ingredient.Count_6_4C6C5BFB4956F9C29A5C2BB6F28B7690)
    end
    LogDebug(Prefix .. "CookingState enum (0-7):", CookingStruct.CookingState_32_568AB66645F40DCFF9789698F2B13EA0)
    LogDebug(Prefix .. "CookedProgress:", CookingStruct.CookedProgress_34_F73379824243A5DFBCABCEB136769B43)
    LogDebug(Prefix .. "CookDuration:", CookingStruct.CookDuration_37_228EDAD3449116421DDD5D9463A67B6A)
    LogDebug(Prefix .. "Portions:", CookingStruct.Portions_40_C56BA4F04C09F80070394080F5A7345D)
    LogDebug(Prefix .. "StovewareType:", CookingStruct.StovewareType_43_E5BC5B244CA4D90B0302288209F7D0C8)
end

---Logs in debug scope all relevant properties of URechargeableComponent_C to console 
---@param RechargeableComponent URechargeableComponent_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogRechargeableComponent(RechargeableComponent, Prefix)
    if IsNotValid(RechargeableComponent) then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "LightSourceType enum (0-3):", RechargeableComponent.LightSourceType)
    LogDebug(Prefix .. "RechargeableActive:",  RechargeableComponent.RechargeableActive)
    LogDebug(Prefix .. "LastBatteryLevel:",  RechargeableComponent.LastBatteryLevel)
    LogDebug(Prefix .. "DrainPerTick:",  RechargeableComponent.DrainPerTick)
    LogDebug(Prefix .. "ChargeMultiplier:",  RechargeableComponent.ChargeMultiplier)
    LogDebug(Prefix .. "Headlamp:",  RechargeableComponent.Headlamp)
    LogDebug(Prefix .. "HeadlampIndex:",  RechargeableComponent.HeadlampIndex)
    LogDebug(Prefix .. "LastItemChargePercentage:",  RechargeableComponent.LastItemChargePercentage)
    LogDebug(Prefix .. "BatteryNotRequiredToFireWeapon:",  RechargeableComponent.BatteryNotRequiredToFireWeapon)
    LogDebug(Prefix .. "OwnerSlotType:",  RechargeableComponent.OwnerSlotType)
    LogDebug(Prefix .. "PlayerOwned:",  RechargeableComponent.PlayerOwned)
end

---Logs in debug scope all relevant properties of a ADeployed_Battery_ParentBP_C to console 
---@param DeployedBattery ADeployed_Battery_ParentBP_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogDeployedBattery(DeployedBattery, Prefix)
    if not DeployedBattery then return end
    Prefix = Prefix or ""

    LogDebug("PlugStripPowerSockets Count:", #DeployedBattery.PlugStripPowerSockets)
    LogDebug("HasBatteryPower:", tostring(DeployedBattery.HasBatteryPower))
    LogDebug("TimerMode enum (0-8):", DeployedBattery.TimerMode)
    LogDebug("PowerTimerActive:", tostring(DeployedBattery.PowerTimerActive))
    LogDebug("BatteryPercentage:", DeployedBattery.BatteryPercentage)
    LogDebug("DevicesPullingPower:", DeployedBattery.DevicesPullingPower)
    LogDebug("MaxBattery:", DeployedBattery.MaxBattery)
    LogDebug("FreezeBatteryDrain:", tostring(DeployedBattery.FreezeBatteryDrain))
    LogDebug("BatteryChargeSpeed:", tostring(DeployedBattery.BatteryChargeSpeed))
    -- local outCount = {}
    -- DeployedBattery:GetPluggedInDeviceCount(outCount)
    -- if outCount and outCount.Count then
    --     LogDebug("PluggedInDeviceCount:", outCount.Count)
    -- end
    AFUtils.LogRechargeableComponent(DeployedBattery.RechargeableComponent, Prefix .. "Rechargeable.")
    AFUtils.LogFurnitureParentBP(DeployedBattery, Prefix .. "FurnitureParent.")
end

---Logs in debug scope all relevant properties of a AAbiotic_Item_ParentBP_C to console 
---@param Item AAbiotic_Item_ParentBP_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogItemParentBP(Item, Prefix)
    if not Item or not Item:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "Class Name:", Item:GetClass():GetFullName())
    LogDebug(Prefix .. "ItemDataRow.RowName:", Item.ItemDataRow.RowName:ToString())
    AFUtils.LogInventoryItemStruct(Item.ItemData, Prefix .. "ItemData.")
    AFUtils.LogInventoryChangeableDataStruct(Item.ChangeableData, Prefix .. "ChangeableData.")
    LogDebug(Prefix .. "HasBeenPickedUp:", tostring(Item.HasBeenPickedUp))
    LogDebug(Prefix .. "To Interact with Text:", Item["To Interact with Text"]:ToString())
    LogDebug(Prefix .. "NoPhysics:", tostring(Item.NoPhysics))
    LogDebug(Prefix .. "NoCollision:", tostring(Item.NoCollision))
    -- LogDebug(Prefix .. "PopupAfterSpawned:", tostring(Item.PopupAfterSpawned))
    -- LogDebug(Prefix .. "ShouldSimulate:", tostring(Item.ShouldSimulate))
    -- LogDebug(Prefix .. "ProjectilePredict_BaseSpeed:", Item.ProjectilePredict_BaseSpeed)
    -- LogDebug(Prefix .. "ProjectilePredict_SpeedMultiplier:", Item.ProjectilePredict_SpeedMultiplier)
    -- LogDebug(Prefix .. "Bounciness:", Item.Bounciness)
    -- LogDebug(Prefix .. "Collision Radius:", Item["Collision Radius"])
    -- LogDebug(Prefix .. string.format("ProjectilePredict_Velocity: X: %f Y: %f Z: %f", Item.ProjectilePredict_Velocity.X, Item.ProjectilePredict_Velocity.Y, Item.ProjectilePredict_Velocity.Z))
    LogDebug(Prefix .. "ItemDecayInterval:", Item.ItemDecayInterval)
    -- LogDebug(Prefix .. "Should Bounce:", tostring(Item["Should Bounce"]))
    -- LogDebug(Prefix .. "SpawnedFromProjectileImpact:", tostring(Item.SpawnedFromProjectileImpact))
    -- local rechargeableComponent = GetBlueprintCreatedComponentByClass(Item, AFUtils.GetClassRechargeableComponent_C())
    -- if rechargeableComponent then
    --     AFUtils.LogRechargeableComponent(rechargeableComponent, Prefix .. "RechargeableComponent.")
    -- end
end

---Logs in debug scope all relevant properties of a AAbiotic_Weapon_ParentBP_C to console 
---@param Weapon AAbiotic_Weapon_ParentBP_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogWeaponParentBP(Weapon, Prefix)
    if not Weapon or not Weapon:IsValid() then return end
    Prefix = Prefix or ""

    AFUtils.LogItemParentBP(Weapon, Prefix)
    LogDebug(Prefix .. "MaxMagazineSize:", Weapon.MaxMagazineSize)
    LogDebug(Prefix .. "CurrentRoundsInMagazine:", Weapon.CurrentRoundsInMagazine)
    LogDebug(Prefix .. "CurrentAmmoIndex:", Weapon.CurrentAmmoIndex)
    LogDebug(Prefix .. "FireDelayDuration:", Weapon.FireDelayDuration)
    LogDebug(Prefix .. "OnHitOnlyOncePerAttack:", Weapon.OnHitOnlyOncePerAttack)
    LogDebug(Prefix .. "OnHitBlocked:", Weapon.OnHitBlocked)
    LogDebug(Prefix .. "IsCharging:", Weapon.IsCharging)
    LogDebug(Prefix .. "UsesChargingLogic:", Weapon.UsesChargingLogic)
    LogDebug(Prefix .. "ChargeAmount:", Weapon.ChargeAmount)
    LogDebug(Prefix .. "ChargeSpeed:", Weapon.ChargeSpeed)
    LogDebug(Prefix .. "OnAmmoHit:", Weapon.OnAmmoHit)
    LogDebug(Prefix .. "HasAmmoVisualsLoaded:", Weapon.HasAmmoVisualsLoaded)
    LogDebug(Prefix .. "CompatibleAmmoTypes.Num:", #Weapon.CompatibleAmmoTypes)
    LogDebug(Prefix .. "ConsumeAmmoOnFire:", Weapon.ConsumeAmmoOnFire)
end

---Logs in debug scope all relevant properties of a UAbiotic_InventoryComponent_C to console 
---@param Inventory UAbiotic_InventoryComponent_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogInventoryComponent(Inventory, Prefix)
    if not Inventory or not Inventory:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "CurrentInventory.Num:", #Inventory.CurrentInventory)
    for i = 1, #Inventory.CurrentInventory, 1 do
        local inventoryItemSlotStruct = Inventory.CurrentInventory[i]
        AFUtils.LogInventoryItemSlotStruct(inventoryItemSlotStruct, Prefix .. "CurrentInventory["..i.."].")
    end
    LogDebug(Prefix .. "MaxSlots:", Inventory.MaxSlots)
    LogDebug(Prefix .. "InventorySlotType enum:", Inventory.InventorySlotType)
    LogDebug(Prefix .. "DisplayName:", Inventory.DisplayName:ToString())
    LogDebug(Prefix .. "ContainerType enum:", Inventory.ContainerType)
    LogDebug(Prefix .. "SlotAppearance enum:", Inventory.SlotAppearance)
    -- LogDebug(Prefix .. "DestroyParentWhenEmpty:", tostring(Inventory.DestroyParentWhenEmpty))
    LogDebug(Prefix .. "EmptyItemRow.RowName:", Inventory.EmptyItemRow.RowName:ToString())
    LogDebug(Prefix .. "InitialInventorySize:", Inventory.InitialInventorySize)
    LogDebug(Prefix .. "CurrentTotalInventoryWeight:", Inventory.CurrentTotalInventoryWeight)
    LogDebug(Prefix .. "CraftingCheck:", tostring(Inventory.CraftingCheck))
    LogDebug(Prefix .. "ItemDecayInterval:", Inventory.ItemDecayInterval)
    LogDebug(Prefix .. "InternalTemperature enum:", Inventory.InternalTemperature)
    LogDebug(Prefix .. "AutoStartItemDecay:", tostring(Inventory.AutoStartItemDecay))
    LogDebug(Prefix .. "CurrentRadiation:", Inventory.CurrentRadiation)
    LogDebug(Prefix .. "RadioactiveShielding:", Inventory.RadioactiveShielding)
    LogDebug(Prefix .. "InventoryUpdateLocked:", tostring(Inventory.InventoryUpdateLocked))
end

---Logs in debug scope all relevant properties of a UW_InventoryItemSlot_C to console 
---@param InventoryItemSlot UW_InventoryItemSlot_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogInventoryItemSlot(InventoryItemSlot, Prefix)
    if not InventoryItemSlot or not InventoryItemSlot:IsValid() then return end
    Prefix = Prefix or ""

    AFUtils.LogInventoryItemSlotStruct(InventoryItemSlot.ItemInSlot, Prefix .. "ItemInSlot.")
    LogDebug(Prefix .. "Empty:", InventoryItemSlot.Empty)
    LogDebug(Prefix .. "HasEmptySlotTooltip:", InventoryItemSlot.HasEmptySlotTooltip)
    LogDebug(Prefix .. "EmptySlotTooltipText:", InventoryItemSlot.EmptySlotTooltipText:ToString())
    LogDebug(Prefix .. "SlotType (enum 0-13):", InventoryItemSlot.SlotType)
    LogDebug(Prefix .. "SlotIndex:", InventoryItemSlot.SlotIndex)
    -- LogDebug(Prefix .. "RepresentationOnly:", InventoryItemSlot.RepresentationOnly)
    LogDebug(Prefix .. "UnavailableItem:", InventoryItemSlot.UnavailableItem)
    AFUtils.LogInventoryChangeableDataStruct(InventoryItemSlot.ItemChangeableStats, Prefix .. "ItemChangeableStats.")
    LogDebug(Prefix .. "MaxWeightShown:", InventoryItemSlot.MaxWeightShown)
    LogDebug(Prefix .. "MinWeightShown:", InventoryItemSlot.MinWeightShown)
    LogDebug(Prefix .. "LastItemCanLoseDurability:", InventoryItemSlot.LastItemCanLoseDurability)
    LogDebug(Prefix .. "Same Item:", InventoryItemSlot["Same Item"])
    LogDebug(Prefix .. "KeyPressed.KeyName:", InventoryItemSlot.KeyPressed.KeyName:ToString())
    -- LogDebug(Prefix .. "ShowTopLeftHotkeyNumber:", InventoryItemSlot.ShowTopLeftHotkeyNumber)
    -- LogDebug(Prefix .. "TopLeftHotkeyBinding:", InventoryItemSlot.TopLeftHotkeyBinding:ToString())
    LogDebug(Prefix .. "QuickMoveOn:", InventoryItemSlot.QuickMoveOn)
    LogDebug(Prefix .. "Leftovers:", InventoryItemSlot.Leftovers)
    -- LogDebug(Prefix .. "BriefTooltip:", InventoryItemSlot.BriefTooltip)
    LogDebug(Prefix .. "CanShowCraftableIcon:", InventoryItemSlot.CanShowCraftableIcon)
    LogDebug(Prefix .. "EdgeGlowWhenFilled:", InventoryItemSlot.EdgeGlowWhenFilled)
    LogDebug(Prefix .. "RemoveGlowWhenHovered:", InventoryItemSlot.RemoveGlowWhenHovered)
    -- LogDebug(Prefix .. "PinnedRecipeItemPresent:", InventoryItemSlot.PinnedRecipeItemPresent)
    -- LogDebug(Prefix .. "TooltipDisplayState (enum 0-4):", InventoryItemSlot.TooltipDisplayState)
    -- LogDebug(Prefix .. "SlotAppearance (enum 0-3):", InventoryItemSlot.SlotAppearance)
end

---Logs in debug scope all relevant properties of a ULeyakDirectorComponent_C to console 
---@param LeyakDirectorComponent ULeyakDirectorComponent_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogLeyakDirectorComponent(LeyakDirectorComponent, Prefix)
    if IsNotValid(LeyakDirectorComponent) then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "TimeLastLeyakSpawn:", LeyakDirectorComponent.TimeLastLeyakSpawn)
    LogDebug(Prefix .. "LeyakViewCounter:", LeyakDirectorComponent.LeyakViewCounter)
    LogDebug(Prefix .. "LeyakTauntCooldownMultiplier:", LeyakDirectorComponent.LeyakTauntCooldownMultiplier)
    LogDebug(Prefix .. "LeyakCooldown:", LeyakDirectorComponent.LeyakCooldown)
    LogDebug(Prefix .. "ActiveLeyakContainmentID:", LeyakDirectorComponent.ActiveLeyakContainmentID)
end

---Logs in debug scope all relevant properties of a AAbiotic_AIDirector_C to console 
---@param AIDirector AAbiotic_AIDirector_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogAIDirector(AIDirector, Prefix)
    if not AIDirector or not AIDirector:IsValid() then return end
    Prefix = Prefix or ""

    AFUtils.LogLeyakDirectorComponent(AIDirector.LeyakDirectorComponent)
    -- AFUtils.LogTimerHandle(AIDirector, AIDirector.Tick_PrioritySpawns, Prefix .. "Tick_PrioritySpawns.")
    -- AFUtils.LogTimerHandle(AIDirector, AIDirector.Tick_DormantSpawns, Prefix .. "Tick_DormantSpawns.")
    LogDebug(Prefix .. "CurrentFacilitySpawns:", AIDirector.CurrentFacilitySpawns)
    LogDebug(Prefix .. "DirectorDebugOn:", AIDirector.DirectorDebugOn)
    LogDebug(Prefix .. "DebugNoDayNightManager:", AIDirector.DebugNoDayNightManager)
    LogDebug(Prefix .. "NPCArray.Num:", #AIDirector.NPCArray)
    LogDebug(Prefix .. "NPCCap:", AIDirector.NPCCap)
    -- AFUtils.LogTimerHandle(AIDirector, AIDirector.Assault_Timer, Prefix .. "Assault_Timer.")
    LogDebug(Prefix .. "Assault_CurrentPhase (enum 1-6):", AIDirector.Assault_CurrentPhase)
    -- ToDo Log Assault_CurrentData
    -- AFUtils.LogTimerHandle(AIDirector, AIDirector.Assault_ActiveLogicTimer, Prefix .. "Assault_ActiveLogicTimer.")
    LogDebug(Prefix .. "Assault_TimeLastSpawnedNPC:", AIDirector.Assault_TimeLastSpawnedNPC)
    LogDebug(Prefix .. "Assault_BenchTargetLocation:", VectorToString(AIDirector.Assault_BenchTargetLocation))
    LogDebug(Prefix .. "Assault_PatrolTargetLocation:", VectorToString(AIDirector.Assault_PatrolTargetLocation))
    LogDebug(Prefix .. "Assault_CurrentRow:", AIDirector.Assault_CurrentRow:ToString())
    LogDebug(Prefix .. "Assault_CurrentSpawnIndex:", AIDirector.Assault_CurrentSpawnIndex)
    LogDebug(Prefix .. "Assault_TimeOfInitialWarning:", AIDirector.Assault_TimeOfInitialWarning)
    -- LogDebug(Prefix .. "Assault_PreviousSpawnIndex:", AIDirector.Assault_PreviousSpawnIndex)
    LogDebug(Prefix .. "TimeBeforeAssaults:", AIDirector.TimeBeforeAssaults)
    -- AFUtils.LogTimerHandle(AIDirector, AIDirector.LeyakTimer, Prefix .. "LeyakTimer.")
    LogDebug(Prefix .. "LeyakCooldown:", AIDirector.LeyakCooldown)
    LogDebug(Prefix .. "ActiveLeyak IsValid:", AIDirector.ActiveLeyak:IsValid())
    LogDebug(Prefix .. "CurrentLeyakTarget IsValid:", AIDirector.CurrentLeyakTarget:IsValid())
    LogDebug(Prefix .. "LeyakSpawnAttempts:", AIDirector.LeyakSpawnAttempts)
    LogDebug(Prefix .. "LeyakWorldFlag.RowName:", AIDirector.LeyakWorldFlag.RowName:ToString())
    LogDebug(Prefix .. "LeyakWorldFlag.DataTablePath:", AIDirector.LeyakWorldFlag.DataTablePath:ToString())
    LogDebug(Prefix .. "TimeLastLeyakSpawn:", AIDirector.TimeLastLeyakSpawn)
    -- AFUtils.LogTimerHandle(AIDirector, AIDirector.CoworkerTimer, Prefix .. "CoworkerTimer.")
    LogDebug(Prefix .. "MaxAssaultPortals:", AIDirector.MaxAssaultPortals)
    LogDebug(Prefix .. "LeyakViewCounter:", AIDirector.LeyakViewCounter)
    LogDebug(Prefix .. "CurrentAssaultPortals:", AIDirector.CurrentAssaultPortals)
    LogDebug(Prefix .. "LeyakTauntCooldownMultiplier:", AIDirector.LeyakTauntCooldownMultiplier)
    LogDebug(Prefix .. "DebugDisableNPCFreezing:", AIDirector.DebugDisableNPCFreezing)
    LogDebug(Prefix .. "PendingSpawns_Quick.Num:", #AIDirector.PendingSpawns_Quick)
    LogDebug(Prefix .. "PendingSpawns_Moderate.Num:", #AIDirector.PendingSpawns_Moderate)
    LogDebug(Prefix .. "PendingSpawns_Slow.Num:", #AIDirector.PendingSpawns_Slow)
    LogDebug(Prefix .. "LastIndex_Quick:", AIDirector.LastIndex_Quick)
    LogDebug(Prefix .. "LastIndex_Moderate:", AIDirector.LastIndex_Moderate)
    LogDebug(Prefix .. "LastIndex_Slow:", AIDirector.LastIndex_Slow)
end

---Logs in debug scope all relevant properties of a AAI_Controller_Leyak_C to console 
---@param AIControllerLeyak AAI_Controller_Leyak_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogAIControllerLeyak(AIControllerLeyak, Prefix)
    if not AIControllerLeyak then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "ValidTargets.Num:", #AIControllerLeyak.ValidTargets)
    LogDebug(Prefix .. "SkipLeyakLevelCheck:", tostring(AIControllerLeyak.SkipLeyakLevelCheck))
    LogDebug(Prefix .. "MaxLeyakChaseDistance:", AIControllerLeyak.MaxLeyakChaseDistance)
    LogDebug(Prefix .. "LeyakAggroRange:", AIControllerLeyak.LeyakAggroRange)
    LogDebug(Prefix .. "WantsToDespawn:", tostring(AIControllerLeyak.WantsToDespawn))
    LogDebug(Prefix .. "MaxLeyakChaseDistance_Aggrod:", AIControllerLeyak.MaxLeyakChaseDistance_Aggrod)
end

---Logs in debug scope all relevant properties of a ANPC_Leyak_C to console 
---@param Leyak ANPC_Leyak_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogNPCLeyak(Leyak, Prefix)
    if not Leyak then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "ActorLocation:", VectorToString(Leyak:K2_GetActorLocation()))
    LogDebug(Prefix .. "Timeline_Dissolve_NewTrack:", Leyak.Timeline_Dissolve_NewTrack_1_039C1F844D7D7D0E8CA9FEBA672ECF59)
    LogDebug(Prefix .. "Timeline_Dissolve__Direction (enum 1-2):", Leyak.Timeline_Dissolve__Direction_039C1F844D7D7D0E8CA9FEBA672ECF59)
    LogDebug(Prefix .. "RequiredMegalightDuration:", Leyak.RequiredMegalightDuration)
    LogDebug(Prefix .. "TargetPlayer IsValid:", tostring(Leyak.TargetPlayer:IsValid()))
    LogDebug(Prefix .. "HasBeenXrayed:", tostring(Leyak.HasBeenXrayed))
    LogDebug(Prefix .. "ViewedByTarget:", tostring(Leyak.ViewedByTarget))
    LogDebug(Prefix .. "CachedTimeSeen:", Leyak.CachedTimeSeen)
    LogDebug(Prefix .. "DistanceDifferenceToDespawn:", Leyak.DistanceDifferenceToDespawn)
    LogDebug(Prefix .. "SeenDespawnTime:", Leyak.SeenDespawnTime)
    LogDebug(Prefix .. "DealDamageInfront:", tostring(Leyak.DealDamageInfront))
    LogDebug(Prefix .. "StuckStartTime:", Leyak.StuckStartTime)
    LogDebug(Prefix .. "PotentiallyStuck:", tostring(Leyak.PotentiallyStuck))
    LogDebug(Prefix .. "AbsolutelyStuck:", tostring(Leyak.AbsolutelyStuck))
    LogDebug(Prefix .. "TimeAllowedToBeStuck:", Leyak.TimeAllowedToBeStuck)
end

---Logs in debug scope all relevant properties of a AWeapon_FishingRod_C to console 
---@param FishingRod AWeapon_FishingRod_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogFishingRod(FishingRod, Prefix)
    if not FishingRod then return end
    Prefix = Prefix or ""

    -- LogDebug(Prefix .. "FishingLocation:", VectorToString(FishingRod.FishingLocation))
    -- LogDebug(Prefix .. "ActiveFishingLocation:", VectorToString(FishingRod.ActiveFishingLocation))
    -- LogDebug(Prefix .. "ActiveCompletionZone:", VectorToString(FishingRod.ActiveCompletionZone))
    -- LogDebug(Prefix .. "ActiveFishLocation:", VectorToString(FishingRod.ActiveFishLocation))
    -- LogDebug(Prefix .. "ActivePlayerLocation:", VectorToString(FishingRod.ActivePlayerLocation))
    LogDebug(Prefix .. "ActiveRodTension:", FishingRod.ActiveRodTension)
    LogDebug(Prefix .. "NextDirectionChangeTime:", FishingRod.NextDirectionChangeTime)
    LogDebug(Prefix .. "NextCooldownTime:", FishingRod.NextCooldownTime)
    LogDebug(Prefix .. "FishCaptureProgress:", FishingRod.FishCaptureProgress)
    LogDebug(Prefix .. "FishCaptureDistance:", FishingRod.FishCaptureDistance)
    LogDebug(Prefix .. "FishCaptureStage:", FishingRod.FishCaptureStage)
    LogDebug(Prefix .. "ActiveFishSpeed:", FishingRod.ActiveFishSpeed)
    LogDebug(Prefix .. "TimeToStartMinigame:", FishingRod.TimeToStartMinigame)
    LogDebug(Prefix .. "HasActiveFish:", tostring(FishingRod.HasActiveFish))
    LogDebug(Prefix .. "PossibleFishingZone:", FishingRod.PossibleFishingZone.RowName:ToString())
    LogDebug(Prefix .. "ActiveFishingZone:", FishingRod.ActiveFishingZone.RowName:ToString())
    LogDebug(Prefix .. "ReelAnimTime:", FishingRod.ReelAnimTime)
    LogDebug(Prefix .. "Reeling:", tostring(FishingRod.Reeling))
    LogDebug(Prefix .. "HotspotActive:", tostring(FishingRod.HotspotActive))
    LogDebug(Prefix .. "LastTimeFishingEnded:", FishingRod.LastTimeFishingEnded)
    LogDebug(Prefix .. "CatchingJunk:", tostring(FishingRod.CatchingJunk))
    LogDebug(Prefix .. "JunkReward.RowName:", FishingRod.JunkReward.RowName:ToString())
    LogDebug(Prefix .. "FishReward.RowName:", FishingRod.FishReward.RowName:ToString())
    LogDebug(Prefix .. "FishReward.DataTablePath:", FishingRod.FishReward.DataTablePath:ToString())
    LogDebug(Prefix .. "RequiredCaptures:", FishingRod.RequiredCaptures)
    LogDebug(Prefix .. "TackleboxActive:", tostring(FishingRod.TackleboxActive))
    LogDebug(Prefix .. "LuckyHat:", tostring(FishingRod.LuckyHat))
    LogDebug(Prefix .. "OwnerLastKnownLevel:", FishingRod.OwnerLastKnownLevel)
    LogDebug(Prefix .. "ReelingSpeed:", FishingRod.ReelingSpeed)
end

---Logs in debug scope all relevant properties of a ABP_FishingHotspot_C to console 
---@param FishingHotspot ABP_FishingHotspot_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogFishingHotspot(FishingHotspot, Prefix)
    if not FishingHotspot then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "HotspotActive:", FishingHotspot.HotspotActive)
    LogDebug(Prefix .. "Registered:", FishingHotspot.Registered)
    LogDebug(Prefix .. "Cooldown:", FishingHotspot.Cooldown)
    LogDebug(Prefix .. "UseAmount:", FishingHotspot.UseAmount)
    LogDebug(Prefix .. "CooldownStartTime:", FishingHotspot.CooldownStartTime)
end

---Logs in debug scope all relevant properties of a ADayNightManager_C to console 
---@param DayNightManager ADayNightManager_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogDayNightManager(DayNightManager, Prefix)
    if not DayNightManager or not DayNightManager:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "DefaultStartingHour:", DayNightManager.DefaultStartingHour)
    LogDebug(Prefix .. "MorningStartHour:", DayNightManager.MorningStartHour)
    LogDebug(Prefix .. "NightfallStartHour:", DayNightManager.NightfallStartHour)
    LogDebug(Prefix .. "AssaultCheckHour:", DayNightManager.AssaultCheckHour)
    LogDebug(Prefix .. "WeatherCheckHour:", DayNightManager.WeatherCheckHour)
    LogDebug(Prefix .. "CurrentTimeInSeconds:", DayNightManager.CurrentTimeInSeconds)
    LogDebug(Prefix .. "24HoursInSeconds:", DayNightManager["24HoursInSeconds"])
    LogDebug(Prefix .. "SecondsPerRealSecond_Day:", DayNightManager.SecondsPerRealSecond_Day)
    LogDebug(Prefix .. "DoNotModify!DayNightTickRate:", DayNightManager["DoNotModify!DayNightTickRate"])
    LogDebug(Prefix .. "CurrentDay:", DayNightManager.CurrentDay)
    LogDebug(Prefix .. "IsNight:", tostring(DayNightManager.IsNight))
    LogDebug(Prefix .. "LastKnownHour:", DayNightManager.LastKnownHour)
    LogDebug(Prefix .. "SecondsPerRealSecond_Night:", DayNightManager.SecondsPerRealSecond_Night)
    LogDebug(Prefix .. "CurrentAnnouncementIndex:", DayNightManager.CurrentAnnouncementIndex)
    LogDebug(Prefix .. "AmbientTemperature_Day:", DayNightManager.AmbientTemperature_Day)
    LogDebug(Prefix .. "AmbientTemperature_Night:", DayNightManager.AmbientTemperature_Night)
    LogDebug(Prefix .. "CurrentAnnouncementJournals.Num:", #DayNightManager.CurrentAnnouncementJournals)
    LogDebug(Prefix .. "HavePlayersLeftStartingZone:", tostring(DayNightManager.HavePlayersLeftStartingZone))
    LogDebug(Prefix .. "LastAssaultDay:", DayNightManager.LastAssaultDay)
    LogDebug(Prefix .. "IsAssaultDay:", tostring(DayNightManager.IsAssaultDay))
    LogDebug(Prefix .. "SleepingPlayers:", DayNightManager.SleepingPlayers)
    LogDebug(Prefix .. "LastWeatherDay:", DayNightManager.LastWeatherDay)
    LogDebug(Prefix .. "CurrentWeatherEvent:", DayNightManager.CurrentWeatherEvent:ToString())
    LogDebug(Prefix .. "RequiredDaysBetweenWeather:", DayNightManager.RequiredDaysBetweenWeather)
    LogDebug(Prefix .. "TimeLastAutoSaved:", DayNightManager.TimeLastAutoSaved)
    if DayNightManager.ActiveWeatherDirector:IsValid() then
        LogDebug(Prefix .. "ActiveWeatherDirector.Class:", DayNightManager.ActiveWeatherDirector:GetClass():GetFullName())
    end
    LogDebug(Prefix .. "ActiveAmbientRadiation:", DayNightManager.ActiveAmbientRadiation)
    LogDebug(Prefix .. "CurrentAnnouncementCompendium.Num:", #DayNightManager.CurrentAnnouncementCompendium)
    LogDebug(Prefix .. "Weather_RequestByPlayer.RowName:", DayNightManager.Weather_RequestByPlayer.RowName:ToString())
    LogDebug(Prefix .. "Weather_RequestByPlayer.DataTablePath:", DayNightManager.Weather_RequestByPlayer.DataTablePath:ToString())
end

---Logs in debug scope all relevant properties of a AAbioticCharacter to console 
---@param Character AAbioticCharacter
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogAbioticCharacter(Character, Prefix)
    if not Character or not Character:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "ActorLocation:", VectorToString(Character:K2_GetActorLocation()))
    LogDebug(Prefix .. "ActorRotation:", RotatorToString(Character:K2_GetActorRotation()))
    if Character.TetheredBy:IsValid() then
        LogDebug(Prefix .. "TetheredBy:", Character.TetheredBy:GetFullName())
    end
    LogDebug(Prefix .. "GameplayTags.GameplayTags (", #Character.GameplayTags.GameplayTags .. "):")
    for i = 1, #Character.GameplayTags.GameplayTags, 1 do
        local gameplayTag = Character.GameplayTags.GameplayTags[i]
        LogDebug(Prefix .. "GameplayTag[" .. i .. "].TagName:", gameplayTag.TagName:ToString())
    end
    LogDebug(Prefix .. "GameplayTags.ParentTags (", #Character.GameplayTags.ParentTags .. "):")
    for i = 1, #Character.GameplayTags.ParentTags, 1 do
        local tag = Character.GameplayTags.ParentTags[i]
        LogDebug(Prefix .. "ParentTags[" .. i .. "].TagName:", tag.TagName:ToString())
    end
    -- ToDo StatModifiers
    LogDebug(Prefix .. "CurrentHealth_Head:", Character.CurrentHealth_Head)
    LogDebug(Prefix .. "CurrentHealth_Torso:", Character.CurrentHealth_Torso)
    LogDebug(Prefix .. "CurrentHealth_LeftArm:", Character.CurrentHealth_LeftArm)
    LogDebug(Prefix .. "CurrentHealth_RightArm:", Character.CurrentHealth_RightArm)
    LogDebug(Prefix .. "CurrentHealth_LeftLeg:", Character.CurrentHealth_LeftLeg)
    LogDebug(Prefix .. "CurrentHealth_RightLeg:", Character.CurrentHealth_RightLeg)
    LogDebug(Prefix .. "CurrentStamina:", Character.CurrentStamina)
    LogDebug(Prefix .. "MaxStamina:", Character.MaxStamina)
    LogDebug(Prefix .. "StaminaRequiredToSprint:", Character.StaminaRequiredToSprint)
    LogDebug(Prefix .. "BaseWalkSpeed:", Character.BaseWalkSpeed)
    LogDebug(Prefix .. "BaseSprintSpeed:", Character.BaseSprintSpeed)
    LogDebug(Prefix .. "MinimumMovementSpeed:", Character.MinimumMovementSpeed)
    LogDebug(Prefix .. "GlobalSpeedModifier:", Character.GlobalSpeedModifier)
    LogDebug(Prefix .. "GlobalSprintSpeedModifier:", Character.GlobalSprintSpeedModifier)
    LogDebug(Prefix .. "GlobalSwimSpeedModifier:", Character.GlobalSwimSpeedModifier)
    LogDebug(Prefix .. "GlobalCrouchSpeedModifier:", Character.GlobalCrouchSpeedModifier)
    LogDebug(Prefix .. "bIsSprinting:", Character.bIsSprinting)
    LogDebug(Prefix .. "bIsAiming:", Character.bIsAiming)
    LogDebug(Prefix .. "JetpackTimestamp:", Character.JetpackTimestamp)
    LogDebug(Prefix .. "bHasStaminaToSprint:", Character.bHasStaminaToSprint)
    LogDebug(Prefix .. "GetTotalHealth:", Character:GetTotalHealth())
    LogDebug(Prefix .. "GetMaxTotalHealth:", Character:GetMaxTotalHealth())
end

---Logs in debug scope all relevant properties of a AAbiotic_Character_ParentBP_C to console 
---@param Character AAbiotic_Character_ParentBP_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogCharacterParentBP(Character, Prefix)
    if not Character or not Character:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "ActorLocation:", VectorToString(Character:K2_GetActorLocation()))
    LogDebug(Prefix .. "ActorRotation:", RotatorToString(Character:K2_GetActorRotation()))
    -- AFUtils.LogOutlineComponent(Character.OutlineComponent, "OutlineComponent.")
    LogDebug(Prefix .. "TotalCombinedHealth:", Character.TotalCombinedHealth)
    LogDebug(Prefix .. "IsDead:", Character.IsDead)
    LogDebug(Prefix .. "IsDBNO:", Character.IsDBNO)
    LogDebug(Prefix .. "HasDBNOState:", Character.HasDBNOState)
    LogDebug(Prefix .. "IsDBNO:", Character.IsDBNO)
    LogDebug(Prefix .. "MeleeRange:", Character.MeleeRange)
    LogDebug(Prefix .. "InteractionRange:", Character.InteractionRange)
    LogDebug(Prefix .. "CurrentHunger:", Character.CurrentHunger)
    LogDebug(Prefix .. "MaxHunger:", Character.MaxHunger)
    LogDebug(Prefix .. "HungerDecayInterval:", Character.HungerDecayInterval)
    LogDebug(Prefix .. "HungerDecayAmount:", Character.HungerDecayAmount)
    -- LogDebug(Prefix .. "HeadBones.Num:", #Character.HeadBones)
    LogDebug(Prefix .. "CurrentFatigue:", Character.CurrentFatigue)
    LogDebug(Prefix .. "MaxFatigue:", Character.MaxFatigue)
    LogDebug(Prefix .. "FatigueIncreaseInterval:", Character.FatigueIncreaseInterval)
    LogDebug(Prefix .. "FatigueIncreaseAmount:", Character.FatigueIncreaseAmount)
    LogDebug(Prefix .. "CurrentThirst:", Character.CurrentThirst)
    LogDebug(Prefix .. "MaxThirst:", Character.MaxThirst)
    LogDebug(Prefix .. "ThirstDecayInterval:", Character.ThirstDecayInterval)
    LogDebug(Prefix .. "ThirstDecayAmount:", Character.ThirstDecayAmount)
    LogDebug(Prefix .. "HasThirst:", Character.HasThirst)
    LogDebug(Prefix .. "HasHunger:", Character.HasHunger)
    LogDebug(Prefix .. "HasFatigue:", Character.HasFatigue)
    -- LogDebug(Prefix .. "TorsoBones.Num:", #Character.TorsoBones)
    -- LogDebug(Prefix .. "LeftArmBones.Num:", #Character.LeftArmBones)
    -- LogDebug(Prefix .. "RightArmBones.Num:", #Character.RightArmBones)
    -- LogDebug(Prefix .. "LeftLegBones.Num:", #Character.LeftLegBones)
    -- LogDebug(Prefix .. "RightLegBones.Num:", #Character.RightLegBones)
    -- ToDo LastPointDamage
    LogDebug(Prefix .. "Faction (enum 0-14):", Character.Faction)
    LogDebug(Prefix .. "CurrentVoiceAudioImportance (enum 0-2):", Character.CurrentVoiceAudioImportance)
    LogDebug(Prefix .. "NPC_Targetable:", Character.NPC_Targetable)
    LogDebug(Prefix .. "Inventory:", Character.Inventory)
    LogDebug(Prefix .. "CurrentBodyTemperature:", Character.CurrentBodyTemperature)
    LogDebug(Prefix .. "LethalBodyTemperature_High:", Character.LethalBodyTemperature_High)
    LogDebug(Prefix .. "LethalBodyTemperature_Low:", Character.LethalBodyTemperature_Low)
    LogDebug(Prefix .. "HasBodyTemperature:", Character.HasBodyTemperature)
    LogDebug(Prefix .. "PreferredBodyTemperature:", Character.PreferredBodyTemperature)
    LogDebug(Prefix .. "LockedInPlaceMontage:", Character.LockedInPlaceMontage)
    LogDebug(Prefix .. "BleedoutStartTime:", Character.BleedoutStartTime)
    LogDebug(Prefix .. "CurrentBleedoutTime:", Character.CurrentBleedoutTime)
    LogDebug(Prefix .. "SelectedDeathAnimation:", Character.SelectedDeathAnimation)
    LogDebug(Prefix .. "DiedExplosively:", Character.DiedExplosively)
    LogDebug(Prefix .. "DamageSpilloverToHead:", Character.DamageSpilloverToHead)
    LogDebug(Prefix .. "IsSleeping:", Character.IsSleeping)
    LogDebug(Prefix .. "IsSittingInSeat:", Character.IsSittingInSeat)
    LogDebug(Prefix .. "IsDrivingVehicle:", Character.IsDrivingVehicle)
    LogDebug(Prefix .. "CurrentBed IsValid:", Character.CurrentBed:IsValid())
    LogDebug(Prefix .. "FatigueRequiredToSleep:", Character.FatigueRequiredToSleep)
    LogDebug(Prefix .. "HasMasterKey:", Character.HasMasterKey)
    LogDebug(Prefix .. "TimeLastRevived:", Character.TimeLastRevived)
    LogDebug(Prefix .. "BloodSplatterType (enum 0-8):", Character.BloodSplatterType)
    LogDebug(Prefix .. "IsReloading:", Character.IsReloading)
    LogDebug(Prefix .. "InvincibleToNPCs:", Character.InvincibleToNPCs)
    LogDebug(Prefix .. "DebuffsImmune.Num:", #Character.DebuffsImmune)
    LogDebug(Prefix .. "TimeOfLastPainYell:", Character.TimeOfLastPainYell)
    LogDebug(Prefix .. "TimeOfLastPainGrunt:", Character.TimeOfLastPainGrunt)
    LogDebug(Prefix .. "CurrentSeat IsValid:", Character.CurrentSeat:IsValid())
    LogDebug(Prefix .. "LastSeatIndex:", Character.LastSeatIndex)
    LogDebug(Prefix .. "ContinenceToReceive:", Character.ContinenceToReceive)
    LogDebug(Prefix .. "HasContinence:", Character.HasContinence)
    LogDebug(Prefix .. "CurrentContinence:", Character.CurrentContinence)
    LogDebug(Prefix .. "MaxContinence:", Character.MaxContinence)
    LogDebug(Prefix .. "ContinenceComputeInterval:", Character.ContinenceComputeInterval)
    LogDebug(Prefix .. "RegeneratesHealth:", Character.RegeneratesHealth)
    LogDebug(Prefix .. "HealthRegenInterval:", Character.HealthRegenInterval)
    LogDebug(Prefix .. "CurrentArmor_Chest:", Character.CurrentArmor_Chest)
    LogDebug(Prefix .. "CurrentArmor_Arms:", Character.CurrentArmor_Arms)
    LogDebug(Prefix .. "CurrentArmor_Legs:", Character.CurrentArmor_Legs)
    LogDebug(Prefix .. "CurrentArmor_Head:", Character.CurrentArmor_Head)
    LogDebug(Prefix .. "Invincible:", Character.Invincible)
    LogDebug(Prefix .. "CharacterSize (enum 0-3):", Character.CharacterSize)
    LogDebug(Prefix .. "ReducedHeadDamage:", Character.ReducedHeadDamage)
    LogDebug(Prefix .. "CurrentLightValue:", Character.CurrentLightValue)
    LogDebug(Prefix .. "DebugMeleeSwings:", Character.DebugMeleeSwings)
    LogDebug(Prefix .. "PerformingMeleeSwing:", Character.PerformingMeleeSwing)
    -- ToDo CurrentMeleeingData
    LogDebug(Prefix .. "ActorsHitThisMeleeSwing.Num:", #Character.ActorsHitThisMeleeSwing)
    LogDebug(Prefix .. "MeleeSwingProgress:", Character.MeleeSwingProgress)
    LogDebug(Prefix .. "CurrentReloadDuration:", Character.CurrentReloadDuration)
    LogDebug(Prefix .. "SpeakingAlpha:", Character.SpeakingAlpha)
    LogDebug(Prefix .. "HasLipsync:", Character.HasLipsync)
    LogDebug(Prefix .. "LipsyncRegistered:", Character.LipsyncRegistered)
    LogDebug(Prefix .. "CurrentSafetyValue:", Character.CurrentSafetyValue)
    LogDebug(Prefix .. "DoTick_GrabZoneCheck:", Character.DoTick_GrabZoneCheck)
    LogDebug(Prefix .. "GrabSocket:", Character.GrabSocket:ToString())
    LogDebug(Prefix .. "GrabbedByCharacter IsValid:", Character.GrabbedByCharacter:IsValid())
    LogDebug(Prefix .. "GrabCheckRadius:", Character.GrabCheckRadius)
    LogDebug(Prefix .. "HoldingCharacter IsValid:", Character.HoldingCharacter:IsValid())
    LogDebug(Prefix .. "InfiniteStamina:", Character.InfiniteStamina)
    LogDebug(Prefix .. "ClimbingLadder:", Character.ClimbingLadder)
    LogDebug(Prefix .. "CurrentLadder IsValid:", Character.CurrentLadder:IsValid())
    LogDebug(Prefix .. "LadderClimbSpeed:", Character.LadderClimbSpeed)
    LogDebug(Prefix .. "LadderStartZ:", Character.LadderStartZ)
    LogDebug(Prefix .. "RecentlyStaggered:", Character.RecentlyStaggered)
    LogDebug(Prefix .. "TemperatureInterval:", Character.TemperatureInterval)
    LogDebug(Prefix .. "TargetBodyTemperature:", Character.TargetBodyTemperature)
    LogDebug(Prefix .. "CurrentRadiation:", Character.CurrentRadiation)
    LogDebug(Prefix .. "CanReceiveRadiation:", Character.CanReceiveRadiation)
    LogDebug(Prefix .. "RadiationCheckInterval:", Character.RadiationCheckInterval)
    LogDebug(Prefix .. "CurrentRadiationBeingReceived:", Character.CurrentRadiationBeingReceived)
    LogDebug(Prefix .. "MaxRadiation:", Character.MaxRadiation)
    LogDebug(Prefix .. "WalkSpeed_Modifiers.Num:", #Character.WalkSpeed_Modifiers)
    for i = 1, #Character.WalkSpeed_Modifiers do
        local modifier = Character.WalkSpeed_Modifiers[i]
        if modifier then
            LogDebug(Prefix .. "WalkSpeed_Modifiers[", i, "]:", modifier)
        end
    end
    LogDebug(Prefix .. "RadiationResistance:", Character.RadiationResistance)
    LogDebug(Prefix .. "Pitch:", Character.Pitch)
    LogDebug(Prefix .. "Yaw:", Character.Yaw)
    LogDebug(Prefix .. "DamageReductionPerArmorPoint:", Character.DamageReductionPerArmorPoint)
    LogDebug(Prefix .. "LastLadderDirectionDown:", Character.LastLadderDirectionDown)
    LogDebug(Prefix .. "UsingZipline:", Character.UsingZipline)
    LogDebug(Prefix .. "CurrentZipline IsValid:", Character.CurrentZipline:IsValid())
    LogDebug(Prefix .. "DamageToNPCs:", Character.DamageToNPCs)
    LogDebug(Prefix .. "DefaultSpottability:", Character.DefaultSpottability)
    LogDebug(Prefix .. "ZenMode:", Character.ZenMode)
    LogDebug(Prefix .. "IsAI:", Character.IsAI)
    LogDebug(Prefix .. "ReviveCount:", Character.ReviveCount)
    LogDebug(Prefix .. "DeadFlailInAir:", Character.DeadFlailInAir)
    LogDebug(Prefix .. "IsShieldBlocking:", Character.IsShieldBlocking)
    LogDebug(Prefix .. "LastBlockedDamagePercentage:", Character.LastBlockedDamagePercentage)
    LogDebug(Prefix .. "WalkSoundVolume:", Character.WalkSoundVolume)
    LogDebug(Prefix .. "SprintSoundVolume:", Character.SprintSoundVolume)
    LogDebug(Prefix .. "TimeLastVomitted:", Character.TimeLastVomitted)
    LogDebug(Prefix .. "LastKilledByInstantDamageType:", Character.LastKilledByInstantDamageType)
    LogDebug(Prefix .. "IsSpeaking:", Character.IsSpeaking)
    LogDebug(Prefix .. "IsStateDrainDisabled:", Character.IsStateDrainDisabled)
    -- ToDo BloodStainsData
    LogDebug(Prefix .. "DefaultMaxFatigue:", Character.DefaultMaxFatigue)
    LogDebug(Prefix .. "SilenceDuringNarrative:", Character.SilenceDuringNarrative)
    LogDebug(Prefix .. "LadderStillOnFloor:", Character.LadderStillOnFloor)
    LogDebug(Prefix .. "LoadedVoiceAsset IsValid:", Character.LoadedVoiceAsset:IsValid())
    LogDebug(Prefix .. "LastStartBlockTime:", Character.LastStartBlockTime)
    LogDebug(Prefix .. "MaxReviveCount:", Character.MaxReviveCount)
    LogDebug(Prefix .. "IsDisabled:", Character.IsDisabled)
    LogDebug(Prefix .. "PerformingWeaponThrow:", Character.PerformingWeaponThrow)
    LogDebug(Prefix .. "WeaponHoldThrowProgress:", Character.WeaponHoldThrowProgress)
    -- ToDo TempThreshold_ properties
    LogDebug(Prefix .. "HasSwimmingStats:", Character.HasSwimmingStats)
    LogDebug(Prefix .. "SwimmingUpdateInterval:", Character.SwimmingUpdateInterval)
    LogDebug(Prefix .. "UnderwaterImmersionDepth:", Character.UnderwaterImmersionDepth)
    LogDebug(Prefix .. "WaddingImmersionDepth:", Character.WaddingImmersionDepth)
    LogDebug(Prefix .. "DrownTime:", Character.DrownTime)
    LogDebug(Prefix .. "RebreatherDrownTime:", Character.RebreatherDrownTime)
    LogDebug(Prefix .. "TotalArmorWeight:", Character.TotalArmorWeight)
    LogDebug(Prefix .. "BodyShieldAbsorbedLastHit:", Character.BodyShieldAbsorbedLastHit)
    LogDebug(Prefix .. "CrouchedWaddingImmersionDepth:", Character.CrouchedWaddingImmersionDepth)
    LogDebug(Prefix .. "MaxArmorDamageReduction:", Character.MaxArmorDamageReduction)
    LogDebug(Prefix .. "HasBeenGrabbed:", Character.HasBeenGrabbed)
    LogDebug(Prefix .. "NoDamageSpillover:", Character.NoDamageSpillover)
    LogDebug(Prefix .. "DefaultGravityScale:", Character.DefaultGravityScale)
    LogDebug(Prefix .. "NearbyFatigueMultiplier:", Character.NearbyFatigueMultiplier)
    LogDebug(Prefix .. "HitReactImpulse_Max:", Character.HitReactImpulse_Max)
    LogDebug(Prefix .. "CanRagdoll:", Character.CanRagdoll)
    LogDebug(Prefix .. "HadRVOEnabled:", Character.HadRVOEnabled)
    LogDebug(Prefix .. "PerformingMeleeAttack:", Character.PerformingMeleeAttack)
    LogDebug(Prefix .. "RagdollDelay:", Character.RagdollDelay)
    LogDebug(Prefix .. "RagdollBlendLength:", Character.RagdollBlendLength)
    LogDebug(Prefix .. "TorsoIsLethal:", Character.TorsoIsLethal)
    LogDebug(Prefix .. "TimeLastVehicleHit:", Character.TimeLastVehicleHit)
    LogDebug(Prefix .. "CurrentHeatResist:", Character.CurrentHeatResist)
    LogDebug(Prefix .. "CurrentColdResist:", Character.CurrentColdResist)
end

---Logs in debug scope all relevant properties of a AAbiotic_PlayerCharacter_C to console 
---@param Character AAbiotic_PlayerCharacter_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogPlayerCharacter(Character, Prefix)
    if not Character or not Character:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "CurrentWalkSpeed:", Character.CurrentWalkSpeed)
    LogDebug(Prefix .. "IsInventoryOpen:", Character.IsInventoryOpen)

    LogDebug(Prefix .. "MaxInventoryWeight:", Character.MaxInventoryWeight)

    LogDebug(Prefix .. "DefaultMaxInventoryWeight:", Character.DefaultMaxInventoryWeight)
end

---Logs in debug scope all relevant properties of a ANarrativeNPC_ParentBP_C to console 
---@param NarrativeNPC ANarrativeNPC_ParentBP_C
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogNarrativeNPC(NarrativeNPC, Prefix)
    if not NarrativeNPC or not NarrativeNPC:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "IsCorpse:", tostring(NarrativeNPC.IsCorpse))
    LogDebug(Prefix .. "NarrativeNPC_ConversationRow.RowName:", tostring(NarrativeNPC.NarrativeNPC_ConversationRow.RowName:ToString()))
    -- LogDebug(Prefix .. "HeadBoneLookAtTarget:", NarrativeNPC.HeadBoneLookAtTarget)
    LogDebug(Prefix .. "NarrativeState (enum 0-5):", NarrativeNPC.NarrativeState)
    LogDebug(Prefix .. "LastPlayedNarrativeState (enum 0-5):", NarrativeNPC.LastPlayedNarrativeState)
    LogDebug(Prefix .. "PlayerCharactersInDialogZone.Num:", #NarrativeNPC.PlayerCharactersInDialogZone)
    LogDebug(Prefix .. "IsSpeakingDialog:", NarrativeNPC.IsSpeakingDialog)
    LogDebug(Prefix .. "PlayerCharactersInBeckon:", #NarrativeNPC.PlayerCharactersInBeckon)
    LogDebug(Prefix .. "CurrentMessageIndex:", NarrativeNPC.CurrentMessageIndex)
    LogDebug(Prefix .. "NoHeadLook:", NarrativeNPC.NoHeadLook)
    LogDebug(Prefix .. "NoBeckonZone:", NarrativeNPC.NoBeckonZone)
    LogDebug(Prefix .. "NoDialogZone:", NarrativeNPC.NoDialogZone)
    LogDebug(Prefix .. "HeadLookAlpha:", NarrativeNPC.HeadLookAlpha)
    LogDebug(Prefix .. "BeckonInterval:", NarrativeNPC.BeckonInterval)
    LogDebug(Prefix .. "LastDialogWasBeckon:", NarrativeNPC.LastDialogWasBeckon)
    LogDebug(Prefix .. "ToInteractwithText:", NarrativeNPC["To Interact with Text"]:ToString())
    LogDebug(Prefix .. "LastVoiceLineIndex:", NarrativeNPC.LastVoiceLineIndex)
    LogDebug(Prefix .. "AutoSpeakNextDialog:", NarrativeNPC.AutoSpeakNextDialog)
    LogDebug(Prefix .. "LongInteractBText:", NarrativeNPC.LongInteractBText:ToString())
    LogDebug(Prefix .. "DistanceForBeckonSubtitles:", NarrativeNPC.DistanceForBeckonSubtitles)
    LogDebug(Prefix .. "NonInteractable:", NarrativeNPC.NonInteractable)
end

---Logs in debug scope all relevant properties of a UAbioticCharacterMovementComponent to console 
---@param MovementComponent UAbioticCharacterMovementComponent
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogAbioticCharacterMovementComponent(MovementComponent, Prefix)
    if not MovementComponent or not MovementComponent:IsValid() or not MovementComponent.MaxTetherDistance then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "bCanEverSprint:", MovementComponent.bCanEverSprint)
    LogDebug(Prefix .. "bCanEverAim:", MovementComponent.bCanEverAim)
    LogCharacterMovementComponent(MovementComponent, Prefix)
end

---Logs in debug scope all relevant AAbioticWorldSettings to console 
---@param WorldSettings AAbioticWorldSettings
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogWorldSettings(WorldSettings, Prefix)
    if not WorldSettings or not WorldSettings:IsValid() or not WorldSettings.SandboxValues then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "SandboxValues count:", #WorldSettings.SandboxValues)
    for i = 1, #WorldSettings.SandboxValues, 1 do
        ---@type FSandboxData
        local data = WorldSettings.SandboxValues[i]
        LogDebug(Prefix .. " #" .. i .. ": Key:", data.Key:ToString() .. ", Value " .. data.Value:ToString())
    end
    LogDebug(Prefix .. "ModifiedRuleset (enum 0-3):", WorldSettings.ModifiedRuleset)
end

---Logs in debug scope all relevant properties of a UCraftingEntryItem to console 
---@param CraftingEntryItem UCraftingEntryItem
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogCraftingEntryItem(CraftingEntryItem, Prefix)
    if not CraftingEntryItem or not CraftingEntryItem:IsValid() or not CraftingEntryItem.RecipeRow then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "RecipeRow.RowName:", CraftingEntryItem.RecipeRow.RowName:ToString())
    LogDebug(Prefix .. "CountToCreate:", CraftingEntryItem.CountToCreate)
    LogDebug(Prefix .. "RecipeItemName:", CraftingEntryItem.RecipeItemName:ToString())
    LogDebug(Prefix .. "ItemToCreate.RowName:", CraftingEntryItem.ItemToCreate.RowName:ToString())
    LogDebug(Prefix .. "BenchesRequired.Num:", #CraftingEntryItem.BenchesRequired)
    LogDebug(Prefix .. "Category:", CraftingEntryItem.Category)
    LogDebug(Prefix .. "RecipeItems.Num:", #CraftingEntryItem.RecipeItems)
    for i = 1, #CraftingEntryItem.RecipeItems, 1 do
        local recipeItem = CraftingEntryItem.RecipeItems[i]
        LogDebug(Prefix .. "RecipeItems[".. i .. "].Item.RowName:", recipeItem.Item.RowName:ToString())
        LogDebug(Prefix .. "RecipeItems[".. i .. "].Count:", recipeItem.Count)
    end
    LogDebug(Prefix .. "SubstituteItems.Num:", #CraftingEntryItem.SubstituteItems)
    for i = 1, #CraftingEntryItem.SubstituteItems, 1 do
        local recipeItem = CraftingEntryItem.SubstituteItems[i]
        LogDebug(Prefix .. "SubstituteItems[".. i .. "].Item.RowName:", recipeItem.Item.RowName:ToString())
        LogDebug(Prefix .. "SubstituteItems[".. i .. "].Count:", recipeItem.Count)
    end
    LogDebug(Prefix .. "bIsSoupRecipe:", CraftingEntryItem.bIsSoupRecipe)
    LogDebug(Prefix .. "bUnlocked:", CraftingEntryItem.bUnlocked)
    LogDebug(Prefix .. "bResearched:", CraftingEntryItem.bResearched)
    LogDebug(Prefix .. "bBenchAvailable:", CraftingEntryItem.bBenchAvailable)
    LogDebug(Prefix .. "bNewRecipeHighlight:", CraftingEntryItem.bNewRecipeHighlight)
    LogDebug(Prefix .. "bNeverCrafted:", CraftingEntryItem.bNeverCrafted)
    LogDebug(Prefix .. "bFavorited:", CraftingEntryItem.bFavorited)
end

---Logs in debug scope all relevant properties of a FAbioticRecipe_Struct to console 
---@param RecipeStruct FAbioticRecipe_Struct
---@param Prefix string? Prefix that should be added in front of each line
function AFUtils.LogRecipeStruct(RecipeStruct, Prefix)
    if not RecipeStruct or not RecipeStruct.ItemToCreate_4_842F5059497E898D938220BCCC148B08 then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "ItemToCreate.RowName:", RecipeStruct.ItemToCreate_4_842F5059497E898D938220BCCC148B08.RowName:ToString())
    LogDebug(Prefix .. "CountToCreate:", RecipeStruct.CountToCreate_17_9ACBB85C48DCB6769A2331AB7B56E2C8)
    LogDebug(Prefix .. "Category:", RecipeStruct.Category_22_940DB5D6483687DCE5FF63A7711F71C3)
    LogDebug(Prefix .. "RecipeItems Num:", #RecipeStruct.RecipeItems_7_0F13BA7A407C72065EE926B9D41B8B9E)
    for i = 1, #RecipeStruct.RecipeItems_7_0F13BA7A407C72065EE926B9D41B8B9E, 1 do
        local recipeItems = RecipeStruct.RecipeItems_7_0F13BA7A407C72065EE926B9D41B8B9E[i]
        LogDebug(Prefix .. "RecipeItems["..i.."].Item.RowName:", recipeItems.Item_5_5AD3D6B1470ED45BCB2D15BC84BB0F1A.RowName:ToString())
        LogDebug(Prefix .. "RecipeItems["..i.."].Count:", recipeItems.Count_6_4C6C5BFB4956F9C29A5C2BB6F28B7690)
    end
    LogDebug(Prefix .. "BenchesRequired Num:", #RecipeStruct.BenchesRequired_10_493C635841D8143BB87BDCA941CD28A6)
    LogDebug(Prefix .. "CraftDuration:", RecipeStruct.CraftDuration_13_BFC1ED4A429775D36D12E683816868D6)
    LogDebug(Prefix .. "LinkedRecipesToUnlock Num:", #RecipeStruct.LinkedRecipesToUnlock_28_EAECA1EA4C69C00231A206961B10737D)
    LogDebug(Prefix .. "NotUnlockableByPickup:", RecipeStruct.NotUnlockableByPickup_24_B20B4A1149D919E221126BA38DB0D6C2)
    LogDebug(Prefix .. "StatModifier.RowName:" .. RecipeStruct.StatModifier_41_48EF866B4719B527AA6212AD8AC21DFE.RowName:ToString())
    LogDebug(Prefix .. "StrippedFromBuild:", RecipeStruct.StrippedFromBuild_46_61BC23684470C1F8417C2CB501AE385D)
end

---@param DeployedParentBP AAbioticDeployed_ParentBP_C
---@param Prefix string?
function AFUtils.LogDeployedParentBP(DeployedParentBP, Prefix)
    if IsNotValid(DeployedParentBP) then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "JiggleDeployableTimeline_NewTrack:", DeployedParentBP.JiggleDeployableTimeline_NewTrack_0_7928E14D489C1875D42917A64D1EA085)
    LogDebug(Prefix .. "JiggleDeployableTimeline__Direction enum (0-1):", DeployedParentBP.JiggleDeployableTimeline__Direction_7928E14D489C1875D42917A64D1EA085)
    LogDebug(Prefix .. "TextureVariantsDataRow.RowName:", DeployedParentBP.TextureVariantsDataRow.RowName:ToString())
    LogDebug(Prefix .. "DeconstructedItemData.RowName:", DeployedParentBP.DeconstructedItemData.RowName:ToString())
    AFUtils.LogInventoryChangeableDataStruct(DeployedParentBP.ChangeableData, Prefix .. "ChangeableData.")
    LogDebug(Prefix .. "CurrentDurability:", DeployedParentBP.CurrentDurability)
    LogDebug(Prefix .. "MaxDurability:", DeployedParentBP.MaxDurability)
    LogDebug(Prefix .. "DeployableDestroyed:", DeployedParentBP.DeployableDestroyed)
    LogDebug(Prefix .. "LastDamageMultiplier:", DeployedParentBP.LastDamageMultiplier)
    LogDebug(Prefix .. "JigglesWhenDamaged:", DeployedParentBP.JigglesWhenDamaged)
    LogDebug(Prefix .. "JiggleRoot_RestingLocation:", VectorToTable(DeployedParentBP.JiggleRoot_RestingLocation))
    LogDebug(Prefix .. "DeployedByPlayer:", DeployedParentBP.DeployedByPlayer)
    LogDebug(Prefix .. "RequiresConstruction:", DeployedParentBP.RequiresConstruction)
    LogDebug(Prefix .. "ConstructionModeActive:", DeployedParentBP.ConstructionModeActive)
    LogDebug(Prefix .. "ConstructionLevel_Current:", DeployedParentBP.ConstructionLevel_Current)
    LogDebug(Prefix .. "ConstructionLevel_Max:", DeployedParentBP.ConstructionLevel_Max)
    -- LogDebug(Prefix .. "Parent Power Source:", DeployedParentBP['Parent Power Source'])
    LogDebug(Prefix .. "AlternativeObjectName:", DeployedParentBP.AlternativeObjectName:ToString())
    LogDebug(Prefix .. "RestoresSanityWhenPowered:", DeployedParentBP.RestoresSanityWhenPowered)
    LogDebug(Prefix .. "LastDamagedByPlayer:", DeployedParentBP.LastDamagedByPlayer)
    LogDebug(Prefix .. "RequiresPower:", DeployedParentBP.RequiresPower)
    LogDebug(Prefix .. "SpawnedAssetID:", DeployedParentBP.SpawnedAssetID:ToString())
    LogDebug(Prefix .. "RequireToolToPackage:", DeployedParentBP.RequireToolToPackage)
    LogDebug(Prefix .. "CulledByWorldLoad:", DeployedParentBP.CulledByWorldLoad)
    LogDebug(Prefix .. "RemainsWhenBroken:", DeployedParentBP.RemainsWhenBroken)
    LogDebug(Prefix .. "IsBrokenState:", DeployedParentBP.IsBrokenState)
    LogDebug(Prefix .. "MeshComponents Num:", #DeployedParentBP.MeshComponents)
    LogDebug(Prefix .. "FoundByPlayer:", DeployedParentBP.FoundByPlayer)
    LogDebug(Prefix .. "MaterialsBeforeHologram Num:", #DeployedParentBP.MaterialsBeforeHologram)
    LogDebug(Prefix .. "Supports Num:", #DeployedParentBP.Supports)
    LogDebug(Prefix .. "DeployedInVignette:", DeployedParentBP.DeployedInVignette)
    LogDebug(Prefix .. "NoResetVignette:", DeployedParentBP.NoResetVignette)
    LogDebug(Prefix .. "DropOverflowBag:", DeployedParentBP.DropOverflowBag)
    LogDebug(Prefix .. "IsCurrentlyLoadingFromSave:", DeployedParentBP.IsCurrentlyLoadingFromSave)
    LogDebug(Prefix .. "PaintedDeployableRow.RowName:", DeployedParentBP.PaintedDeployableRow.RowName:ToString())
    LogDebug(Prefix .. "PaintedColor enum (0-12):", DeployedParentBP.PaintedColor)
    LogDebug(Prefix .. "MaterialOverrideCount:", DeployedParentBP.MaterialOverrideCount)
    LogDebug(Prefix .. "PowerCordAttached Num:", #DeployedParentBP.PowerCordAttached)
    LogDebug(Prefix .. "nosave:", DeployedParentBP.nosave)
    LogDebug(Prefix .. "CanBeLaserPowered:", DeployedParentBP.CanBeLaserPowered)
    LogDebug(Prefix .. "HasLaserPower:", DeployedParentBP.HasLaserPower)
    LogDebug(Prefix .. "HitLasers Num:", #DeployedParentBP.HitLasers)
end

---@param FurnitureParentBP AAbioticDeployed_Furniture_ParentBP_C
---@param Prefix string?
function AFUtils.LogFurnitureParentBP(FurnitureParentBP, Prefix)
    if IsNotValid(FurnitureParentBP) then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "CanBePackaged:", FurnitureParentBP.CanBePackaged)
    LogDebug(Prefix .. "HasBeenPackagedUp:", FurnitureParentBP.HasBeenPackagedUp)
    LogDebug(Prefix .. "SeatsOccupied Num:", #FurnitureParentBP.SeatsOccupied)
    LogDebug(Prefix .. "Seats Num:", #FurnitureParentBP.Seats)
    LogDebug(Prefix .. "SeatOccupiers Num:", #FurnitureParentBP.SeatOccupiers)
    LogDebug(Prefix .. "BrokeWhenPackaged:", FurnitureParentBP.BrokeWhenPackaged)
    LogDebug(Prefix .. "DefaultBreakChanceOnPackage:", FurnitureParentBP.DefaultBreakChanceOnPackage)
    LogDebug(Prefix .. "WorldSpawnChance:", FurnitureParentBP.WorldSpawnChance)
    LogDebug(Prefix .. "BreakChanceReductionPerLevel:", FurnitureParentBP.BreakChanceReductionPerLevel)
    AFUtils.LogSaveDataDeployableStruct(FurnitureParentBP.SaveData, Prefix .. "SaveData.")
    LogDebug(Prefix .. "To Interact with Text:", FurnitureParentBP["To Interact with Text"]:ToString())
    LogDebug(Prefix .. "PlayerMadeString:", FurnitureParentBP.PlayerMadeString:ToString())
    LogDebug(Prefix .. "RepGroundPosition:", VectorToString(FurnitureParentBP.RepGroundPosition))
    LogDebug(Prefix .. "PendingStructuralSetup:", FurnitureParentBP.PendingStructuralSetup)
    LogDebug(Prefix .. "PendingFloatCheck:", FurnitureParentBP.PendingFloatCheck)
    LogDebug(Prefix .. "IgnoreSupports:", FurnitureParentBP.IgnoreSupports)
    LogDebug(Prefix .. "Targetable:", FurnitureParentBP.Targetable)
    LogDebug(Prefix .. "CanBeNPCTarget:", FurnitureParentBP.CanBeNPCTarget)
    LogDebug(Prefix .. "Faction enum (0-11):", FurnitureParentBP.Faction)
    LogDebug(Prefix .. "SeatExitOffset:", FurnitureParentBP.SeatExitOffset)
    LogDebug(Prefix .. "NPCTargetPriority enum (0-4):", FurnitureParentBP.NPCTargetPriority)
    LogDebug(Prefix .. "SupportOffsetUp:", FurnitureParentBP.SupportOffsetUp)
    LogDebug(Prefix .. "CurrentActorRotation:", RotatorToString(FurnitureParentBP.CurrentActorRotation))
    AFUtils.LogDeployedParentBP(FurnitureParentBP, Prefix .. "DeployedParentBP.")
end

---@param LeyakContainment ADeployed_LeyakContainment_C
---@param Prefix string?
function AFUtils.LogDeployedLeyakContainment(LeyakContainment, Prefix)
    if not LeyakContainment or not LeyakContainment:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "SpawnedAssetID:", LeyakContainment.SpawnedAssetID:ToString())
    LogDebug(Prefix .. "FrostGlassTimeline:", LeyakContainment.FrostGlassTimeline_NewTrack_0_08756EC44A134A401814C5B93A4A86D5)
    LogDebug(Prefix .. "FrostGlassTimeline Direction:", LeyakContainment.FrostGlassTimeline__Direction_08756EC44A134A401814C5B93A4A86D5)
    local comparisonIndex = LeyakContainment.ContainsLeyak:GetComparisonIndex()
    LogDebug(Prefix .. "ContainsLeyak ComparisonIndex:", comparisonIndex)
    if comparisonIndex > 0 then
        LogDebug(Prefix .. "ContainsLeyak:", LeyakContainment.ContainsLeyak:ToString())
    end
    LogDebug(Prefix .. "Stability Level:", LeyakContainment["Stability Level"])
    LogDebug(Prefix .. "StabilityDecreasePerNight:", LeyakContainment.StabilityDecreasePerNight)
    LogDebug(Prefix .. "MaxStability:", LeyakContainment.MaxStability)
    LogDebug(Prefix .. "FeedRequiredToFill:", LeyakContainment.FeedRequiredToFill)
    LogDebug(Prefix .. "FoggedGlass:", LeyakContainment.FoggedGlass)
end

---@param OutlineComponent UOutlineComponent_C
---@param Prefix string?
function AFUtils.LogOutlineComponent(OutlineComponent, Prefix)
    if not OutlineComponent or not OutlineComponent:IsValid() then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "OutlineMask:", OutlineComponent.OutlineMask)
    LogDebug(Prefix .. "RegisteredComponents count:", #OutlineComponent.RegisteredComponents)
    LogDebug(Prefix .. "ComponentEnabled:", OutlineComponent.ComponentEnabled)
end

---@param FarmingPlot AFarmingPlot_BP_C
---@param Prefix string?
function AFUtils.LogFarmingPlot(FarmingPlot, Prefix)
    if IsNotValid(FarmingPlot) then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "PlotIndex:", FarmingPlot.PlotIndex)
    -- ToDo log FarmingPlot.PlantProxy
    LogDebug(Prefix .. "GrowthTickRate:", FarmingPlot.GrowthTickRate)
    LogDebug(Prefix .. "HasWater:", FarmingPlot.HasWater)
    LogDebug(Prefix .. "FertilizerSaveConversionValue:", FarmingPlot.FertilizerSaveConversionValue)
    LogDebug(Prefix .. "PlantGrowthStageMax:", FarmingPlot.PlantGrowthStageMax)
    LogDebug(Prefix .. "VisualFertilizeQuality:", FarmingPlot.VisualFertilizeQuality)
end


---@param LiquidContainer ADeployed_LiquidContainer_ParentBP_C
---@param Prefix string?
function AFUtils.LogDeployedLiquidContainer(LiquidContainer, Prefix)
    if IsNotValid(LiquidContainer) then return end
    Prefix = Prefix or ""

    -- AFUtils.LogFurnitureParentBP(LiquidContainer, Prefix)
    LogDebug(Prefix .. "Liquid_FillLevel:", LiquidContainer.Liquid_FillLevel)
    LogDebug(Prefix .. "Liquid_MaxFill:", LiquidContainer.Liquid_MaxFill)
    LogDebug(Prefix .. "CurrentLiquid_Type enum (0-13):", AFUtils.LiquidTypeToString(LiquidContainer.CurrentLiquid_Type) .. " (" .. LiquidContainer.CurrentLiquid_Type .. ")")
    LogDebug(Prefix .. "LocalPlayer_HasLiquidInContainer:", LiquidContainer.LocalPlayer_HasLiquidInContainer)
    LogDebug(Prefix .. "LocalPlayer_CanTakeDirectSwig:", LiquidContainer.LocalPlayer_CanTakeDirectSwig)
    LogDebug(Prefix .. "AllowedLiquids.Num:", #LiquidContainer.AllowedLiquids)
    for i = 1, #LiquidContainer.AllowedLiquids do
        local liquiedType = LiquidContainer.AllowedLiquids[i]
        LogDebug(Prefix .. string.format("AllowedLiquids[%d]: %s (%d)", i, AFUtils.LiquidTypeToString(liquiedType), liquiedType))
    end
    LogDebug(Prefix .. "ObjectName_Empty:", LiquidContainer.ObjectName_Empty:ToString())
    LogDebug(Prefix .. "ObjectName_WithLiquid:", LiquidContainer.ObjectName_WithLiquid:ToString())
    LogDebug(Prefix .. "GiveRandomFill:", LiquidContainer.GiveRandomFill)
    LogDebug(Prefix .. "LiquidToItemText:", LiquidContainer.LiquidToItemText:ToString())
    LogDebug(Prefix .. "LiquidToContainerText:", LiquidContainer.LiquidToContainerText:ToString())
    LogDebug(Prefix .. "LiquidDrinkDirectlyText:", LiquidContainer.LiquidDrinkDirectlyText:ToString())
    LogDebug(Prefix .. "InfiniteSource:", LiquidContainer.InfiniteSource)
    LogDebug(Prefix .. "DrinkCounter:", LiquidContainer.DrinkCounter)
    -- LogDebug(Prefix .. "LastDrinker:", LiquidContainer.LastDrinker)
end

---@param GardenPlot AGardenPlot_ParentBP_C
---@param Prefix string?
function AFUtils.LogGardenPlot(GardenPlot, Prefix)
    if IsNotValid(GardenPlot) then return end
    Prefix = Prefix or ""

    AFUtils.LogDeployedLiquidContainer(GardenPlot, Prefix)
    LogDebug(Prefix .. "FarmingPlots.Num:", #GardenPlot.FarmingPlots)
    for i = 1, #GardenPlot.FarmingPlots do
        AFUtils.LogFarmingPlot(GardenPlot.FarmingPlots[i], Prefix .. "FarmingPlots[" .. i .. "].")
    end
    LogDebug(Prefix .. "WaterTickRate:", GardenPlot.WaterTickRate)
    LogDebug(Prefix .. "WaterLossPerTick:", GardenPlot.WaterLossPerTick)
    LogDebug(Prefix .. "WaterRequiredLight:", GardenPlot.WaterRequiredLight)
end

---@param ItemProxy AItemProxyParent_BP_C
---@param Prefix string?
function AFUtils.LogItemProxy(ItemProxy, Prefix)
    if IsNotValid(ItemProxy) then return end
    Prefix = Prefix or ""

    AFUtils.LogInventoryChangeableDataStruct(ItemProxy.ChangeableData, Prefix .. "ChangeableData.")
    LogDebug(Prefix .. "ItemRow.RowName:", ItemProxy.ItemRow.RowName:ToString())
    LogDebug(Prefix .. "SpotIndex:", ItemProxy.SpotIndex)
end

---@param CookingProxy ACookingProxy_BP_C
---@param Prefix string?
function AFUtils.LogCookingProxy(CookingProxy, Prefix)
    if IsNotValid(CookingProxy) then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "State enum (0-7):", CookingProxy.State)
    LogDebug(Prefix .. "OriginalItem.RowName:", CookingProxy.OriginalItem.RowName:ToString())
    LogDebug(Prefix .. "CookwareItem.RowName:", CookingProxy.CookwareItem.RowName:ToString())
    LogDebug(Prefix .. "TimeToCook:", CookingProxy.TimeToCook)
    LogDebug(Prefix .. "ChefSkill enum (0-7):", CookingProxy.ChefSkill)
    LogDebug(Prefix .. "TimeToBurn:", CookingProxy.TimeToBurn)
    LogDebug(Prefix .. "TimeToIgnite:", CookingProxy.TimeToIgnite)
    LogDebug(Prefix .. "Cookware:", CookingProxy.Cookware)
    -- LogDebug(Prefix .. "Cooking Delay GUID:", CookingProxy["Cooking Delay GUID"])
    LogDebug(Prefix .. "CurrentCookType enum (0-2):", CookingProxy.CurrentCookType)
    LogDebug(Prefix .. "IsCookingSoup:", CookingProxy.IsCookingSoup)
    AFUtils.LogCookingStruct(CookingProxy["Soup Data"], Prefix .. "Soup Data.")
    LogDebug(Prefix .. "CurrentCookedDisplay:", CookingProxy.CurrentCookedDisplay:ToString())
    LogDebug(Prefix .. "New Ingredient Added:", CookingProxy["New Ingredient Added"])
    LogDebug(Prefix .. "OriginalChefPlayerID:", CookingProxy.OriginalChefPlayerID:ToString())
    LogDebug(Prefix .. "MaxLiquid:", CookingProxy.MaxLiquid)
    LogDebug(Prefix .. "CurrentFoodRowName:", CookingProxy.CurrentFoodRowName:ToString())
    LogDebug(Prefix .. "Current FXState enum (0-11):", CookingProxy["Current FXState"])
    LogDebug(Prefix .. "BoilingTaintedWater:", CookingProxy.BoilingTaintedWater)
    LogDebug(Prefix .. "TaintedWaterBoilDelay:", CookingProxy.TaintedWaterBoilDelay)
    LogDebug(Prefix .. "LastModifiedTime:", CookingProxy.LastModifiedTime)
    LogDebug(Prefix .. "StovewareMeshType:", CookingProxy.StovewareMeshType)
    LogDebug(Prefix .. "FoodRow:", CookingProxy.FoodRow:ToString())
    AFUtils.LogItemProxy(CookingProxy, Prefix)
end

---@param StoveParent AStove_ParentBP_C
---@param Prefix string?
function AFUtils.LogStoveParent(StoveParent, Prefix)
    if IsNotValid(StoveParent) then return end
    Prefix = Prefix or ""

    LogDebug(Prefix .. "IsBakingOven:", StoveParent.IsBakingOven)
    LogDebug(Prefix .. "Local_LookingAtEmptyStoveSlot:", StoveParent.Local_LookingAtEmptyStoveSlot)
    LogDebug(Prefix .. "Local_CookableItemInHand:", StoveParent.Local_CookableItemInHand)
    LogDebug(Prefix .. "CookSpots.Num:", #StoveParent.CookSpots)
    LogDebug(Prefix .. "CookableProxies.Num:", #StoveParent.CookableProxies)
    LogDebug(Prefix .. "StovesOn.Num:", #StoveParent.StovesOn)
    LogDebug(Prefix .. "StoveTopVerticalOffset:", StoveParent.StoveTopVerticalOffset)
    LogDebug(Prefix .. "Local_LookingAtEmptyCookware:", StoveParent.Local_LookingAtEmptyCookware)
    LogDebug(Prefix .. "LocalHoldingCookware:", StoveParent.LocalHoldingCookware)
    LogDebug(Prefix .. "Local Cookware Is A Pot:", StoveParent["Local Cookware Is A Pot"])
    LogDebug(Prefix .. "Local Holding A Container:", StoveParent["Local Holding A Container"])
    LogDebug(Prefix .. "Local Start Cooking Soup:", StoveParent["Local Start Cooking Soup"])
    LogDebug(Prefix .. "TimeStartedCooking.Num:", #StoveParent.TimeStartedCooking)
    for i = 1, #StoveParent.TimeStartedCooking do
        local time = StoveParent.TimeStartedCooking[i]
        LogDebug(Prefix .. string.format("TimeStartedCooking[%d]: %f", i, time))
    end
    LogDebug(Prefix .. "LocalHoldingBakeable:", StoveParent.LocalHoldingBakeable)
    LogDebug(Prefix .. "Current Stove Top State:", StoveParent["Current Stove Top State"])
    LogDebug(Prefix .. "Current Held Item State:", StoveParent["Current Held Item State"])
    LogDebug(Prefix .. "SoupWidgetOffset:", StoveParent.SoupWidgetOffset)
    LogDebug(Prefix .. "RequiredItemName:", StoveParent.RequiredItemName:ToString())
    LogDebug(Prefix .. "RequiredFixItItem.RowName:", StoveParent.RequiredFixItItem.RowName:ToString())
    LogDebug(Prefix .. "HasBeenFixed:", StoveParent.HasBeenFixed)
    LogDebug(Prefix .. "PanRotations.Num:", #StoveParent.PanRotations)
    for i = 1, #StoveParent.PanRotations do
        local panRotation = StoveParent.PanRotations[i]
        LogDebug(Prefix .. string.format("PanRotations[%d]: %f", i, panRotation))
    end
    LogDebug(Prefix .. "IsFixingInventorySave:", StoveParent.IsFixingInventorySave)
end

return AFUtils