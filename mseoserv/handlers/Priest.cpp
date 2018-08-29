#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../world.hpp"
#include "../util/secure_string.hpp"

namespace Handlers
{
    void Priest_Accept(Character *character, PacketReader &reader)
    {
        (void)character;
        (void)reader;

        if (character->npc_type == ENF::Priest)
        {
            character->map->Msg(character->npc,"Very well, the ceremony will start in " + util::to_string(static_cast<int>(character->world->config["WeddingStartDelay"])) + " seconds.");
            character->npc->marriage_state = 1;
            character->npc->marriage_timer = static_cast<int>(Timer::GetTime()) + static_cast<int>(character->world->config["WeddingStartDelay"]);
            character->npc->Wedding();

            PacketBuilder reply;
            reply.SetID(PACKET_MUSIC, PACKET_PLAYER);
            reply.AddShort(util::to_int(character->world->config["WeddingMusic"]));

            for(Character * ch : character->map->characters){
                ch->Send(reply);
            }
        }
    }

    void Priest_Use(Character *character, PacketReader &reader)
    {
        (void)character;
        (void)reader;

        if (character->npc_type == ENF::Priest)
        {
            if (character->world->GetCharacter(character->npc->marriage_partner[0]) == character){
                character->npc->marriage_state = 7; // the Wedding function will bump this to 8 upon call
                character->map->Msg(character, "Yes, I do.", true);
            }
            else if (character->world->GetCharacter(character->npc->marriage_partner[1]) == character){
                character->npc->marriage_state = 11;

                character->map->Msg(character, "Yes, I do.", true);
            }else{
                character->npc->marriage_state = 20;
            }
        }
    }

    void Priest_Request(Character *character, PacketReader &reader)
    {

        if (character->npc_type == ENF::Priest)
        {
            reader.GetInt();
            reader.GetByte();

            Character *partner = character->world->GetCharacter(reader.GetEndString());

            if (!partner || partner == character || partner->map != character->map)
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_PARTNER_MAP);
                character->Send(reply);
            }
            else if (!partner->partner.empty())
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_PARTNER_ALREADY_MARRIED);
                character->Send(reply);
            }
            else if (partner->paperdoll[Character::Armor] != util::to_int(character->world->config[(partner->gender == GENDER_FEMALE ? "WeddingArmorFemale" : "WeddingArmorMale")]))
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_PARTNER_CLOTHES);
                partner->Send(reply);
                character->Send(reply);
            }
            else if (character->fiance != util::ucfirst(util::lowercase(partner->SourceName())) || partner->fiance != util::ucfirst(util::lowercase(character->SourceName())))
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_NO_PREMISSION);
                character->Send(reply);
            }
            else
            {
                partner->npc = character->npc;
                partner->npc_type = ENF::Priest;

                character->npc->marriage_partner[0] = character->SourceName();
                character->npc->marriage_partner[1] = partner->SourceName();
                character->npc->marriage_state = 1;

                PacketBuilder builder(PACKET_PRIEST, PACKET_REQUEST);
                builder.AddShort(1);
                builder.AddString(character->SourceName());
                partner->Send(builder);
            }
        }
    }

    void Priest_Open(Character *character, PacketReader &reader)
    {
        short id = reader.GetShort();

        UTIL_FOREACH(character->map->npcs, npc)
        {
            if (npc->index == id && npc->ENF().type == ENF::Priest)
            {
                character->npc = npc;
                character->npc_type = ENF::Priest;

                if (npc->marriage_state > 0)
                    return;


                if (character->fiance.empty() || !character->partner.empty())
                {
                    character->map->Msg(npc, "You have no partner! Please buy a wedding approval from law bob or if you're already married, a divorce.");
                    return;
                }

                if (character->paperdoll[Character::Armor] != util::to_int(character->world->config[(character->gender == GENDER_FEMALE ? "WeddingArmorFemale" : "WeddingArmorMale")])
                || util::to_int(character->world->config[(character->gender == GENDER_FEMALE ? "WeddingArmorFemale" : "WeddingArmorMale")]) == 0)
                {
                    PacketBuilder reply;
                    reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                    reply.AddChar(PRIEST_CLOTHES);
                    character->Send(reply);
                }
                else if (character->level < util::to_int(character->world->config["WeddingLevelNeeded"]))
                {
                    PacketBuilder reply;
                    reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                    reply.AddChar(PRIEST_UNEXPERIENCED);
                    character->Send(reply);
                }
                else
                {
                    PacketBuilder reply;
                    reply.SetID(PACKET_PRIEST, PACKET_OPEN);
                    reply.AddInt(0);
                    character->Send(reply);
                }

                break;
            }
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_PRIEST)
        Register(PACKET_ACCEPT, Priest_Accept, Playing);
        Register(PACKET_USE, Priest_Use, Playing);
        Register(PACKET_REQUEST, Priest_Request, Playing);
        Register(PACKET_OPEN, Priest_Open, Playing);
    PACKET_HANDLER_REGISTER_END(PACKET_PRIEST)
}
