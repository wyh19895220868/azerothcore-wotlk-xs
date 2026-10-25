
#include "NpcBeastmaster.h"
#include "Config.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "StringConvert.h"
#include <map>
#include <vector>
#pragma execution_character_set("utf-8")

PetCustomMap PetCustomStore;

namespace
{
    std::vector<uint32> HunterSpells = { 883, 982, 2641, 6991, 48990, 1002, 1462, 6197 };

    PetsStore pets = { {"蝙蝠", 10357}, {"熊", 12037}, {"野猪", 29996}, {"猫科", 2175}, {"秃鹰", 2931}, {"螃蟹", 18241}, {"鳄鱼", 1417}, {"龙鹰", 18155}, {"猩猩", 1559}, {"猎犬", 29452}, {"狗类", 5829}, {"飞蛾", 25498}, {"虚空鳐", 18285}, {"猫头鹰", 14343}, {"猛禽", 9684}, {"毁灭者", 22123}, {"蝎子", 14476}, {"赤蛇", 28011}, {"蜘蛛", 12433}, {"孢子蝠", 18280}, {"鸵鸟", 22807}, {"乌龟", 14223}, {"追猎者", 23163}, {"黄蜂", 18283}, {"风蛇", 5834}, {"土狼", 3825}, {"座狼", 24128} };
    PetsStore exoticPets = { {"奇美拉", 20932}, {"熔岩犬", 33502}, {"魔暴龙", 32485}, {"犀牛", 25487}, {"异种蝎", 6582}, {"异虫", 30422}, {"幽灵熊", 38453}, {"幽灵虎", 33776}, {"幽灵豹", 32517}, {"幽灵狼", 35189} };
    PetsStore rarePets = { {"特殊鸟", 3068}, {"奥托纳", 32481} };
    PetsStore rareExoticPets = { {"库肯", 17447} };

    enum PetGossip
    {
        PET_BEASTMASTER_HOWL = 9036,
        PET_PAGE_SIZE = 13,
        PET_PAGE_START_PETS = 501,
        PET_PAGE_START_EXOTIC_PETS = 601,
        PET_PAGE_START_RARE_PETS = 701,
        PET_PAGE_START_RARE_EXOTIC_PETS = 801,
        PET_PAGE_MAX = 901,
        PET_MAIN_MENU = 50,
        PET_REMOVE_SKILLS = 80,
    };

    constexpr auto PET_SPELL_CALL_PET = 883;
    constexpr auto PET_SPELL_TAME_BEAST = 13481;
    constexpr auto PET_SPELL_BEAST_MASTERY = 53270;
    constexpr auto PET_MAX_HAPPINESS = 1048000;
}

NpcBeastmaster* NpcBeastmaster::Instance()
{
    static NpcBeastmaster AtInstance;
    return &AtInstance;
}

void NpcBeastmaster::LoadSystem()
{
    PetCustomStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 配置值 FROM __全职业宠物配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            PetCustomStore.insert(std::make_pair(Fields[0].Get<uint32>(), Fields[1].Get<std::string>()));

        } while (Result->NextRow());
    }
}

std::string NpcBeastmaster::GetStrConfig(uint32 Index)
{
    auto Itr = PetCustomStore.find(Index);
    return Itr != PetCustomStore.end() ? Itr->second : "";
}

uint32 NpcBeastmaster::GetUInt(uint32 Index)
{
    auto Itr = PetCustomStore.find(Index);
    if (Itr != PetCustomStore.end())
        return uint32(atoi(Itr->second.c_str()));

    return 0;
}

float NpcBeastmaster::GetFloat(uint32 Index)
{
    auto Itr = PetCustomStore.find(Index);
    if (Itr != PetCustomStore.end())
        return float(atof(Itr->second.c_str()));

    return 0.0f;
}

bool NpcBeastmaster::GetBoolean(uint32 Index)
{
    auto Itr = PetCustomStore.find(Index);
    if (Itr != PetCustomStore.end())
    {
        std::transform(Itr->second.begin(), Itr->second.end(), Itr->second.begin(), ::toupper);
        return (Itr->second == "1" || Itr->second == "YES" || Itr->second == "TRUE" || Itr->second == "ON" || Itr->second == "是");
    }

    return false;
}

void NpcBeastmaster::ShowMainMenu(Player* player, Creature* creature)
{
    if (GetBoolean(101))
    {
        if (player->getClass() != CLASS_HUNTER)
        {
            creature->Whisper("[提示]: 目前只允许猎人使用", LANG_UNIVERSAL, player);
            return;
        }
    }

    uint32 NeedLevel = GetUInt(103);
    if (player->getLevel() < NeedLevel && NeedLevel != 0)
    {
        std::string MExperience = Acore::StringFormat("[提示]: 当前角色等级太低 至少需要 {} 级", NeedLevel);
        creature->Whisper(MExperience.c_str(), LANG_UNIVERSAL, player);
        return;
    }

    ClearGossipMenuFor(player);

    AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看普通宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS);
    AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看稀有宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS);
    AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看特殊普通宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS);
    AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看特殊稀有宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS);
    AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看兽栏管理", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_STABLEPET);
    AddGossipItemFor(player, GOSSIP_ICON_TALK, "购买宠物食品", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_VENDOR);
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());

    player->PlayDirectSound(PET_BEASTMASTER_HOWL);
}

void NpcBeastmaster::GossipSelect(Player* player, Creature* creature, uint32 action)
{
    ClearGossipMenuFor(player);

    if (action == PET_MAIN_MENU)
    {
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看普通宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看稀有宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看特殊普通宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看特殊稀有宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "查看兽栏管理", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_STABLEPET);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "购买宠物食品", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_VENDOR);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_PETS && action < PET_PAGE_START_EXOTIC_PETS)
    {
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_PETS + 1;
        int maxPage = pets.size() / PET_PAGE_SIZE + (pets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS + page);

        AddPetsToGossip(player, pets, page);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_EXOTIC_PETS && action < PET_PAGE_START_RARE_PETS)
    {
        if (!(player->HasSpell(PET_SPELL_BEAST_MASTERY) || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec())))
        {
            player->addSpell(PET_SPELL_BEAST_MASTERY, SPEC_MASK_ALL, false);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_EXOTIC_PETS + 1;
        int maxPage = exoticPets.size() / PET_PAGE_SIZE + (exoticPets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS + page);

        AddPetsToGossip(player, exoticPets, page);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_RARE_PETS && action < PET_PAGE_START_RARE_EXOTIC_PETS)
    {
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_RARE_PETS + 1;
        int maxPage = rarePets.size() / PET_PAGE_SIZE + (rarePets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS + page);

        AddPetsToGossip(player, rarePets, page);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_RARE_EXOTIC_PETS && action < PET_PAGE_MAX)
    {
        if (!(player->HasSpell(PET_SPELL_BEAST_MASTERY) || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec())))
        {
            player->addSpell(PET_SPELL_BEAST_MASTERY, SPEC_MASK_ALL, false);
            std::ostringstream messageLearn;
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_RARE_EXOTIC_PETS + 1;
        int maxPage = rareExoticPets.size() / PET_PAGE_SIZE + (rareExoticPets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS + page);

        AddPetsToGossip(player, rareExoticPets, page);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
    else if (action == PET_REMOVE_SKILLS)
    {
        for (std::size_t i = 0; i < HunterSpells.size(); ++i)
            player->removeSpell(HunterSpells[i], SPEC_MASK_ALL, false);

        player->removeSpell(PET_SPELL_BEAST_MASTERY, SPEC_MASK_ALL, false);
        CloseGossipMenuFor(player);
    }
    else if (action == GOSSIP_OPTION_STABLEPET)
    {
        player->GetSession()->SendStablePet(creature->GetGUID());
    }
    else if (action == GOSSIP_OPTION_VENDOR)
    {
        player->GetSession()->SendListInventory(creature->GetGUID());
    }

    if (action >= PET_PAGE_MAX)
        CreatePet(player, creature, action);
}

void NpcBeastmaster::CreatePet(Player* player, Creature* creature, uint32 action)
{
    if (player->IsExistPet())
    {
        creature->Whisper("[提示]: 你当前已经有宠物了 请先丢弃或置放兽栏", LANG_UNIVERSAL, player);
        CloseGossipMenuFor(player);
        return;
    }

    Pet* pet = player->CreatePet(action - PET_PAGE_MAX, player->getClass() == CLASS_HUNTER ? PET_SPELL_TAME_BEAST : PET_SPELL_CALL_PET);
    if (!pet)
    {
        creature->Whisper("[提示]: 你当前已经有宠物了 请先丢弃或置放兽栏", LANG_UNIVERSAL, player);
        return;
    }

    pet->SetPower(POWER_HAPPINESS, PET_MAX_HAPPINESS);

    if (player->getClass() != CLASS_HUNTER)
    {
        if (!player->HasSpell(PET_SPELL_CALL_PET))
        {
            for (auto const& _spell : HunterSpells)
                if (!player->HasSpell(_spell))
                    player->learnSpell(_spell);
        }
    }

    std::string MessageAdopt = Acore::StringFormat("[提示]: 宠物召唤成功");
    creature->Whisper(MessageAdopt.c_str(), LANG_UNIVERSAL, player);
    CloseGossipMenuFor(player);
}

void NpcBeastmaster::AddPetsToGossip(Player* player, PetsStore const& petsStore, uint32 page)
{
    uint32 count = 1;

    for (auto const& [petName, petEntry] : petsStore)
    {
        if (count > (page - 1) * PET_PAGE_SIZE && count <= page * PET_PAGE_SIZE)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, petName, GOSSIP_SENDER_MAIN, petEntry + PET_PAGE_MAX);

        count++;
    }
}

void NpcBeastmaster::PlayerUpdate(Player* player, uint32 p_time)
{
    if (GetBoolean(102))
    {
        player->KeepPetHappyTime++;
        if (player->KeepPetHappyTime >= 5 * IN_MILLISECONDS)
        {
            player->KeepPetHappyTime = 0;
            Pet* pet = player->GetPet();
            if (pet && pet->getPetType() == HUNTER_PET)
                pet->SetPower(POWER_HAPPINESS, PET_MAX_HAPPINESS);
        }
    }
}
