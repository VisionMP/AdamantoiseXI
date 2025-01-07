/*
===========================================================================

  Copyright (c) 2010-2015 Darkstar Dev Teams

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses/

  This file is part of DarkStar-server source code.

===========================================================================
*/

#include "../../common/showmsg.h"

#include <string.h> 

#include "../universal_container.h"
#include "../item_container.h"

#include "../lua/luautils.h"

#include "../packets/char_update.h"
#include "../packets/char_sync.h"
#include "../packets/fishing.h"
#include "../packets/inventory_finish.h"
#include "../packets/message_name.h"
#include "../packets/message_text.h"
#include "../packets/release.h"
#include "../packets/message_system.h"

#include "charutils.h"
#include "fishingutils.h"
#include "itemutils.h"
#include "../map.h"
#include "zoneutils.h"


namespace fishingutils
{
    int8 CatchLevel;
    uint16 MessageOffset[MAX_ZONEID];

    void LoadFishingMessages()
    {
        zoneutils::ForEachZone([](CZone* PZone) {
            MessageOffset[PZone->GetID()] = luautils::GetTextIDVariable(PZone->GetID(), "FISHING_MESSAGE_OFFSET");
            });
    }

    /************************************************************************
    *																		*
    *  Получение смещения для сообщений рыбалки								*
    *																		*
    ************************************************************************/

    uint16 GetMessageOffset(uint16 ZoneID)
    {
        return MessageOffset[ZoneID];
    }

    /************************************************************************
    *																		*
    *  VISION CODE				*
    *																		*
    ************************************************************************/
    uint8 GetFishingSkill(CCharEntity* PChar)
    {
        return static_cast<uint8>(std::floor(PChar->RealSkills.skill[SKILL_FISHING] / 10));
    }

    uint8 GetFishingRank(CCharEntity* PChar)
    {
        return static_cast<uint8>(std::floor(PChar->RealSkills.rank[SKILL_FISHING]));
    }

    void DoSkillUp(CCharEntity* PChar)
    {
        int16 skillUpAmount = 3;

        int32 FishingSkill = GetFishingSkill(PChar);
        uint8 skillRank = GetFishingRank(PChar); // Check character rank
        uint16 maxSkill = (skillRank + 1) * 100;
           
        int32 skillUpChance = dsprand::GetRandomNumber(1000 + (FishingSkill * 2));
        int32 noSkillUpChance = dsprand::GetRandomNumber(850);       

        //if (skillUpChance > noSkillUpChance)
        //{
            PChar->RealSkills.skill[SKILL_FISHING] += skillUpAmount;
            PChar->pushPacket(new CMessageBasicPacket(PChar, PChar, SKILL_FISHING, skillUpAmount, 38));           
            charutils::SetCharVar(PChar, "FishingSkill", PChar->RealSkills.skill[SKILL_FISHING]);
            //PChar->pushPacket(new CMessageBasicPacket(PChar, PChar, SKILL_FISHING, 0.1, 48));

            if (PChar->RealSkills.skill[SKILL_FISHING] / 10 >= 1)
            {
                charutils::SaveCharSkills(PChar, SKILL_FISHING);
                PChar->pushPacket(new CMessageBasicPacket(PChar, PChar, SKILL_FISHING, (FishingSkill + skillUpAmount), 53));                
                charutils::SetCharVar(PChar, "FishingSkill", 0);
            }
            else
            {
                //charutils::SaveCharSkills(PChar, SKILL_FISHING + skillUpAmount);

            }

            //charutils::SaveCharSkills(PChar, FishingSkill);
        //}
    }

    void FishingSkillup(CCharEntity* PChar, uint8 catchLevel)
    {       

        uint8        skillRank = PChar->RealSkills.rank[SKILL_FISHING];
        uint16       maxSkill = (skillRank + 1) * 100;
        int32        charSkill = PChar->RealSkills.skill[SKILL_FISHING];
        int32        charSkillLevel = (uint32)std::floor(PChar->RealSkills.skill[SKILL_FISHING] / 10);
        uint8        levelDifference = 0;
        int          maxSkillAmount = 1;
        CItemWeapon* Rod = dynamic_cast<CItemWeapon*>(PChar->getEquip(SLOT_RANGED));

        if (catchLevel > charSkillLevel)
        {
            levelDifference = catchLevel - charSkillLevel;
        }

        // No skillup if fish level not between char level and 50 levels higher
        if (catchLevel <= charSkillLevel || (levelDifference > 50))
        {
            return;
        }

        int skillRoll = 90;
        int maxChance = 0;
        int bonusChanceRoll = 8;

        // Lu shang rod under level 50 penalty
        //if (Rod != nullptr && charSkillLevel < 50 && Rod->getID() == LU_SHANG)
       // {
       //     skillRoll += 20;
       // }

        // Generate a normal distribution favoring fish 10 levels higher in skill
        // with 5 levels of deviation on either side
        double normDist = exp(-0.5 * log(2 * M_PI) - log(5) - pow(levelDifference - 11, 2) / 50);
        int    distMod = (int)std::floor(normDist * 200);
        int    lowerLevelBonus = (int)std::floor((100 - charSkillLevel) / 10);
        int    skillLevelPenalty = (int)std::floor(charSkillLevel / 10);

        // Minimum 4% chance
        maxChance = std::max(4, distMod + lowerLevelBonus - skillLevelPenalty);

        // Configuration multiplier.
       // maxChance = maxChance * settings::get<float>("map.FISHING_SKILL_MULTIPLIER");

        // Moon phase skillup modifiers
        uint8 phase = CVanaTime::getInstance()->getMoonPhase();
        uint8 moonDirection = CVanaTime::getInstance()->getMoonDirection();
        switch (moonDirection)
        {
        case 0: // None
            if (phase == 0)
            {
                skillRoll -= 20;
                bonusChanceRoll -= 3;
            }
            else if (phase == 100)
            {
                skillRoll += 10;
                bonusChanceRoll += 3;
            }
            break;

        case 1: // Waning (decending)
            if (phase <= 10)
            {
                skillRoll -= 15;
                bonusChanceRoll -= 2;
            }
            else if (phase >= 95 && phase <= 100)
            {
                skillRoll += 5;
                bonusChanceRoll += 2;
            }
            break;

        case 2: // Waxing (increasing)
            if (phase <= 5)
            {
                skillRoll -= 10;
                bonusChanceRoll -= 1;
            }
            else if (phase >= 90 && phase <= 100)
            {
                bonusChanceRoll += 1;
            }
            break;
        }

        // Not in City bonus
        //CZone* PZone = zoneutils::GetZone(PChar->getZone());
        //if (!(PZone && PZone->Get & ZONE_TYPE::CITY))
        //{
       //     skillRoll -= 10;
       // }

        if (charSkillLevel < 50)
        {
            skillRoll -= (20 - (uint8)std::floor(charSkillLevel / 3));
        }

        // Max skill amount increases as level difference gets higher
        const int skillAmountAdd = 1 + (int)std::floor(levelDifference / 5);
        maxSkillAmount = std::min(skillAmountAdd, 3);

        if (dsprand::GetRandomNumber(skillRoll) < maxChance)
        {
            int32 skillAmount = 1;

            // Bonus points
            if (dsprand::GetRandomNumber(bonusChanceRoll) == 1)
            {
                skillAmount = dsprand::GetRandomNumber(1, maxSkillAmount);
            }

            if ((skillAmount + charSkill) > maxSkill)
            {
                skillAmount = maxSkill - charSkill;
            }

            if (skillAmount > 0)
            {
                PChar->RealSkills.skill[SKILL_FISHING] += skillAmount;
               // PChar->pushPacket<CMessageBasicPacket>(PChar, PChar, SKILL_FISHING, skillAmount, 38);
                PChar->pushPacket(new CMessageBasicPacket(PChar, PChar, SKILL_FISHING, skillAmount, 38));

                if ((charSkill / 10) < (charSkill + skillAmount) / 10)
                {
                    PChar->WorkingSkills.skill[SKILL_FISHING] += 0x20;

                    if (PChar->RealSkills.skill[SKILL_FISHING] >= maxSkill)
                    {
                        PChar->WorkingSkills.skill[SKILL_FISHING] |= 0x8000; // blue capped text
                    }

                    //PChar->pushPacket<CCharSkillsPacket>(PChar);
                    //PChar->pushPacket<CMessageBasicPacket>(PChar, PChar, SKILL_FISHING, (charSkill + skillAmount) / 10, 53);
                    PChar->pushPacket(new CMessageBasicPacket(PChar, PChar, SKILL_FISHING, (charSkill + skillAmount) / 10, 53));

                }

                charutils::SaveCharSkills(PChar, SKILL_FISHING);
            }
        }
    }


    void StartFishing(CCharEntity* PChar)
    {
        if (PChar->animation != ANIMATION_NONE)
        {
            PChar->pushPacket(new CMessageSystemPacket(0, 0, 142));
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_FISHING));
            return;
        }

        uint16 MessageOffset = fishingutils::GetMessageOffset(PChar->getZone());

        if (MessageOffset == 0)
        {
            ShowWarning(CL_YELLOW"Player wants to fish in %s\n" CL_RESET, PChar->loc.zone->GetName());
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_FISHING));
            return;
        }

        CItemWeapon* WeaponItem = nullptr;

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_RANGED);

        if ((WeaponItem == nullptr) ||
            !(WeaponItem->isType(ITEM_WEAPON)) ||
            (WeaponItem->getSkillType() != SKILL_FISHING))
        {
            // сообщение: "You can't fish without a rod in your hands"

            PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x01));
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_FISHING));
            return;
        }

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_AMMO);

        if ((WeaponItem == nullptr) ||
            !(WeaponItem->isType(ITEM_WEAPON)) ||
            (WeaponItem->getSkillType() != SKILL_FISHING))
        {
            // сообщение: "You can't fish without bait on the hook"	

            PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x02));
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_FISHING));
            return;
        }

        PChar->animation = ANIMATION_FISHING_START;
        PChar->updatemask |= UPDATE_HP;

        PChar->pushPacket(new CCharUpdatePacket(PChar));
        PChar->pushPacket(new CCharSyncPacket(PChar));
    }

    /************************************************************************
    *																		*
    *  Персонаж ломает удочку												*
    *																		*
    ************************************************************************/

    bool CheckFisherLuck(CCharEntity* PChar)
    {
        if (PChar->UContainer->GetType() != UCONTAINER_EMPTY)
        {
            ShowDebug(CL_CYAN"Player cannot fish! UContainer is not empty\n" CL_RESET);
            return false;
        }

        CItemFish* PFish = nullptr;
        CItemWeapon* WeaponItem = nullptr;

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_RANGED);

        DSP_DEBUG_BREAK_IF(WeaponItem == nullptr);
        DSP_DEBUG_BREAK_IF(WeaponItem->isType(ITEM_WEAPON) == false);
        DSP_DEBUG_BREAK_IF(WeaponItem->getSkillType() != SKILL_FISHING);

        uint16 RodID = WeaponItem->getID();

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_AMMO);

        DSP_DEBUG_BREAK_IF(WeaponItem == nullptr);
        DSP_DEBUG_BREAK_IF(WeaponItem->isType(ITEM_WEAPON) == false);
        DSP_DEBUG_BREAK_IF(WeaponItem->getSkillType() != SKILL_FISHING);

        uint16 LureID = WeaponItem->getID();

        int32 FishingFail = dsprand::GetRandomNumber(750);
        int32 FishingChance = dsprand::GetRandomNumber(1000);
        int32 FishingSkill = GetFishingSkill(PChar);

        if (FishingChance + FishingSkill >= FishingFail)
        {
            const char* Query =
                "SELECT "
                "fish.fishid,"      // 0
                "fish.min,"         // 1
                "fish.max,"         // 2
                "fish.size,"        // 3
                "fish.stamina,"     // 4
                "fish.watertype,"   // 5
                "rod.flag, "         // 6
                "lure.luck "        // 7
                "FROM fishing_zone AS zone "
                "INNER JOIN fishing_rod  AS rod  USING (fishid) "
                "INNER JOIN fishing_lure AS lure USING (fishid) "
                "INNER JOIN fishing_fish AS fish USING (fishid) "
                "WHERE zone.zoneid = %u AND rod.rodid = %u AND lure.lureid = %u AND lure.luck != 0 "
                "ORDER BY luck";

            int32 ret = Sql_Query(SqlHandle, Query, PChar->getZone(), RodID, LureID);

            if (ret != SQL_ERROR && Sql_NumRows(SqlHandle) != 0)
            {
                //int32 FishingFail = dsprand::GetRandomNumber(750);
                //int32 skillUpChance = dsprand::GetRandomNumber(1000 + (FishingSkill * 2));
                //int32 noSkillUpChance = dsprand::GetRandomNumber(850);
                //int32 FishingSkill = GetFishingSkill(PChar);
                int32 FishingSkillb = PChar->RealSkills.skill[48] / 10;

                while (Sql_NextRow(SqlHandle) == SQL_SUCCESS)
                {
                    int32 FishMin = Sql_GetIntData(SqlHandle, 1);
                    int32 FishMax = Sql_GetIntData(SqlHandle, 2);
                    CatchLevel = Sql_GetIntData(SqlHandle, 2);

                    if (FishingSkill >= FishMin)
                    {
                        //ADD CODE FOR SPECIAL LURES - LureID == 321782 ex

                        PFish = new CItemFish(*itemutils::GetItemPointer(Sql_GetIntData(SqlHandle, 0)));

                        PChar->UContainer->SetType(UCONTAINER_FISHING);
                        PChar->UContainer->SetItem(0, PFish); // Two for double lure


                        //PChar->RealSkills.skill[SKILL_FISHING] += 0.1;
                        //PChar->pushPacket(new CMessageBasicPacket(PChar, PChar, SKILL_FISHING, 0.1, 38));
                        //PChar->WorkingSkills.skill[SKILL_FISHING] += 0x20;
                        //PChar->pushPacket(new CMessageBasicPacket(PChar, PChar, SKILL_FISHING, PChar->WorkingSkills.skill[SKILL_FISHING] += 0x20, 53));


                        //DoSkillUp(PChar);

                        break;
                    }
                    else
                    {
                        delete PFish;

                    }
                }
            }
        }

        return (PFish != nullptr);
    }

    /************************************************************************
    *																		*
    *  Персонаж теряет наживку (теряет блесну лишь при условии RemoveFly)	*
    *																		*
    ************************************************************************/

    bool LureLoss(CCharEntity* PChar, bool RemoveFly)
    {
        CItemWeapon* PLure = (CItemWeapon*)PChar->getEquip(SLOT_AMMO);

        DSP_DEBUG_BREAK_IF(PLure == nullptr);
        DSP_DEBUG_BREAK_IF(PLure->isType(ITEM_WEAPON) == false);
        DSP_DEBUG_BREAK_IF(PLure->getSkillType() != SKILL_FISHING);

        if (!RemoveFly &&
            (PLure->getStackSize() == 1))
        {
            return false;
        }
        if (PLure->getQuantity() == 1)
        {
            charutils::EquipItem(PChar, 0, SLOT_AMMO, LOC_INVENTORY);
        }

        charutils::UpdateItem(PChar, PLure->getLocationID(), PLure->getSlotID(), -1);
        PChar->pushPacket(new CInventoryFinishPacket());
        return true;
    }

    /************************************************************************
    *																		*
    *  Персонаж ломает удочку												*
    *																		*
    ************************************************************************/

    void RodBreaks(CCharEntity* PChar)
    {
        uint8  SlotID = PChar->equip[SLOT_RANGED];
        CItem* PRod = PChar->getStorage(LOC_INVENTORY)->GetItem(SlotID);

        DSP_DEBUG_BREAK_IF(PRod == nullptr);

        uint16 BrokenRodID = 0;

        switch (PRod->getID())
        {
        case 0x4276:  BrokenRodID = 0x0728; break;
        case 0x4277:  BrokenRodID = 0x0729; break;
        case 0x43E4:  BrokenRodID = 0x01E3; break;
        case 0x43E5:  BrokenRodID = 0x01D9; break;
        case 0x43E6:  BrokenRodID = 0x01D8; break;
        case 0x43E7:  BrokenRodID = 0x01E2; break;
        case 0x43E8:  BrokenRodID = 0x01EA; break;
        case 0x43E9:  BrokenRodID = 0x01EB; break;
        case 0x43EA:  BrokenRodID = 0x01E9; break;
        case 0x43EB:  BrokenRodID = 0x01E4; break;
        case 0x43EC:  BrokenRodID = 0x01E8; break;
        case 0x43ED:  BrokenRodID = 0x01E7; break;
        case 0x43EE:  BrokenRodID = 0x01E6; break;
        case 0x43EF:  BrokenRodID = 0x01E5; break;
        }

        DSP_DEBUG_BREAK_IF(BrokenRodID == 0);

        charutils::EquipItem(PChar, 0, SLOT_RANGED, LOC_INVENTORY);
        charutils::UpdateItem(PChar, LOC_INVENTORY, SlotID, -1);
        charutils::AddItem(PChar, LOC_INVENTORY, BrokenRodID, 1);
    }

    /************************************************************************
    *																		*
    *																		*
    *																		*
    ************************************************************************/

    void FishingAction(CCharEntity* PChar, FISHACTION action, uint16 stamina, uint32 special)
    {
        uint16 MessageOffset = GetMessageOffset(PChar->getZone());

        switch (action)
        {
        case FISHACTION_CHECK:
        {
            if (CheckFisherLuck(PChar))
            {
                // сообщение: "Something caught the hook!"

                //PChar->animation = ANIMATION_FISHING_FISH;
                //PChar->updatemask |= UPDATE_HP;
                PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x08));
                PChar->pushPacket(new CFishingPacket(10128, 128, 20, 500, 13, 140, 60, 0, 0));
            }
            else
            {
                // сообщение: "You didn't catch anything."

                PChar->animation = ANIMATION_FISHING_STOP;
                PChar->updatemask |= UPDATE_HP;
                PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x04));
            }
        }
        break;
        case FISHACTION_FINISH:
        {
            if (stamina == 0)
            {
                // сообщение: "You caught fish!"

                DSP_DEBUG_BREAK_IF(PChar->UContainer->GetType() != UCONTAINER_FISHING);
                DSP_DEBUG_BREAK_IF(PChar->UContainer->GetItem(0) == nullptr);

                PChar->animation = ANIMATION_FISHING_CAUGHT;
                PChar->updatemask |= UPDATE_HP;

                CItem* PFish = PChar->UContainer->GetItem(0);
                
                //DoSkillUp(PChar);
                



                // TODO: анализируем RodFlag

                charutils::AddItem(PChar, LOC_INVENTORY, PFish->getID(), 1);
                PChar->loc.zone->PushPacket(PChar, CHAR_INRANGE_SELF, new CMessageNamePacket(PChar, MessageOffset + 0x27, PChar, PFish->getID()));

                if (PFish->isType(ITEM_USABLE))
                {
                    LureLoss(PChar, false);
                }
                FishingSkillup(PChar, CatchLevel);
                delete PFish;
            }
            else if (stamina <= 0x64)
            {
                // сообщение: "Your line breaks!"

                PChar->animation = ANIMATION_FISHING_LINE_BREAK;
                PChar->updatemask |= UPDATE_HP;
                LureLoss(PChar, true);
                PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x06));
            }
            else if (stamina <= 0x100)
            {
                // сообщение: "You give up!"

                PChar->animation = ANIMATION_FISHING_STOP;
                PChar->updatemask |= UPDATE_HP;

                if (PChar->UContainer->GetType() == UCONTAINER_FISHING &&
                    LureLoss(PChar, false))
                {
                    PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x24));
                }
                else {
                    PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x25));
                }
            }
            else
            {
                // сообщение: "You lost your catch!"

                PChar->animation = ANIMATION_FISHING_STOP;
                PChar->updatemask |= UPDATE_HP;
                LureLoss(PChar, false);
                PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x09));
            }
            PChar->UContainer->Clean();
        }
        break;
        case FISHACTION_WARNING:
        {
            // сообщение: "You don't know how much longer you can keep this one on the line..."

            PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x28));
            return;
        }
        break;
        case FISHACTION_END:
        {
            // skillup

            PChar->animation = ANIMATION_NONE;
            PChar->updatemask |= UPDATE_HP;
        }
        break;
        }

        PChar->pushPacket(new CCharUpdatePacket(PChar));
        PChar->pushPacket(new CCharSyncPacket(PChar));
    }

  
} // namespace fishingutils
