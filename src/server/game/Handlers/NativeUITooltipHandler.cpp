/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License
 * for more details.
 */

#include "NativeUIProtocol.h"
#include "WorldSession.h"
#include "Item.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "WorldPacket.h"
#include "../../scripts/Custom/AddonActive/FrozenCoreModuleBridge.h"

// CMSG_NATIVE_UI_TOOLTIP_QUERY (0x524):
//   uint16 version (1), uint32 itemId, uint32 guidLow (0 = entry-only query)
//
// NativeUI clients ask for the aggregated tooltip payload instead of the
// "UITQ<TAB>QUERY:..." addon whisper; the answer is sent back on
// SMSG_NATIVE_UI_TOOLTIP_DATA (0x525).
void WorldSession::HandleNativeUITooltipQuery(WorldPacket& packet)
{
    uint16 version = 0;
    uint32 itemId = 0;
    uint32 guidLow = 0;
    packet >> version >> itemId >> guidLow;

    if (packet.rpos() != packet.size() || version != NativeUIProtocol::TooltipProtocolVersion ||
        itemId == 0 || !_nativeUIEnabled || !_player)
        return;

    if (guidLow == 0)
    {
        FrozenCoreSendItemTemplateDataNative(_player, itemId);
        return;
    }

    Item* item = _player->GetItemByGuid(ObjectGuid(HighGuid::Item, guidLow));
    if (!item || item->GetEntry() != itemId)
    {
        FrozenCoreSendItemTemplateDataNative(_player, itemId);
        return;
    }

    FrozenCoreSendItemModuleDataNative(_player, itemId, guidLow);
}

void WorldSession::SendNativeUITooltipData(uint32 itemId, uint32 guidLow, std::string const& payload)
{
    if (!_nativeUIEnabled || !_player || !itemId || payload.empty() ||
        payload.size() > NativeUIProtocol::MaxTooltipPayloadBytes)
        return;

    WorldPacket data(SMSG_NATIVE_UI_TOOLTIP_DATA, payload.size() + 12);
    data << NativeUIProtocol::TooltipProtocolVersion << itemId << guidLow << uint16(payload.size());
    data.append(payload.data(), payload.size());
    SendPacket(&data);
}
