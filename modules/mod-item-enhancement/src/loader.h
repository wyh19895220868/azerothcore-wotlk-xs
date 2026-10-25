

#ifndef ITEM_ENHANCEMENT_LOADER_H
#define ITEM_ENHANCEMENT_LOADER_H

// From SC
void AddSC_ItemEnhancement();
void AddSC_ItemEnhancementCommands();
void AddSC_ItemEnhancementScript();
void AddSC_ItemEnhancementLoader();
void AddSC_ItemEnhancementWorldScript();
void AddSC_ItemEnhancementPlayerScript();
void AddSC_ItemInspectGuidHandler();
void AddSC_ItemEnhancementUIScript();
void AddSC_ItemEnhancementAddonHandler();

// Add all
void AddItemEnhancementScripts()
{
    AddSC_ItemEnhancement();
    AddSC_ItemEnhancementCommands();
    AddSC_ItemEnhancementScript();
    AddSC_ItemEnhancementLoader();
    AddSC_ItemEnhancementWorldScript();
    AddSC_ItemEnhancementPlayerScript();
    AddSC_ItemInspectGuidHandler();
    AddSC_ItemEnhancementUIScript();
    AddSC_ItemEnhancementAddonHandler();
}

#endif // ITEM_ENHANCEMENT_LOADER_H
