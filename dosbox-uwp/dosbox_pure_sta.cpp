#include <string>
#include "libretro.h"
#include "dosbox_pure_sta.h"
#ifdef MOUSE_SUPPORT
#include "Content/RetroCore.h"
using namespace dosbox_uwp;
#endif

int DBPS_SaveSlotIndex = 0;
std::string DBPS_BrowsePath;

void DBPS_OnContentLoad(const char*, const char*, size_t) {}
void DBPS_SubmitOSDFrame(const void*, unsigned, unsigned) {}
void DBPS_GetMouse(short& mx, short& my, bool)
{
#ifdef MOUSE_SUPPORT
    RetroCore::GetPointer(mx, my);
#else
    mx = 0; my = 0;
#endif
}
void DBPS_StartCaptureJoyBind(unsigned, unsigned, unsigned, unsigned, bool) {}
bool DBPS_HaveJoy() { return false; }
bool DBPS_GetJoyBind(unsigned, unsigned, unsigned, unsigned, bool, std::string&, std::string&, const char*) { return false; }
void DBPS_RequestSaveLoad(bool, bool) {}
bool DBPS_HaveSaveSlot() { return false; }
bool DBPS_ApplyConfigOverrides(const std::string&) { return false; }
bool DBPS_IsConfigOverride(const char*) { return false; }
void DBPS_ToggleConfigOverride(const char*, const char*) {}
std::string DBPS_GetConfigOverrideJSON() { return std::string(); }
