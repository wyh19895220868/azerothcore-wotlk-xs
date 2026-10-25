
#ifndef _DOADDON_H
#define _DOADDON_H

#define FuncIndex 1400                                                              //功能配置起始
#define AddonMaxIndex 24                                                              //

struct ItemRunesCacheTemp;

// Builds the ClientAtRunesData payload, including server-resolved rune metadata.
std::string BuildRunesDataPayload(uint32 guidLow, ItemRunesCacheTemp const& data);

class DoAddon
{
public:
    static DoAddon* instance()
    {
        static DoAddon instance;
        return &instance;
    }

    bool IsPacket(std::string RevPacket, std::string Packet);
    void SendPacketToClient(Player* Sender, std::string HMsg, std::string SMsg = " ");
    void OnAddonMessage(Player* player, const char* HMsg, const char* RevMsg);
};
#define sDoAddon DoAddon::instance()

#endif
