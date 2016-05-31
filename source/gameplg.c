#include "global.h"

#define WRITEU8(addr, data) *(vu8*)(addr) = data
#define WRITEU16(addr, data) *(vu16*)(addr) = data
#define WRITEU32(addr, data) *(vu32*)(addr) = data
#define READU8(addr) *(vu8*)(addr)
#define READU16(addr) *(vu16*)(addr)
#define READU32(addr) *(vu32*)(addr)


u32 threadStack[0x1000];
Handle fsUserHandle;
FS_archive sdmcArchive;

#define IO_BASE_PAD		1
#define IO_BASE_LCD		2
#define IO_BASE_PDC		3
#define IO_BASE_GSPHEAP		4

u32 IoBasePad = 0xFFFD4000;

//My defined variables////////////////////////////////////
    void increaseIndvStat(int charU, int statU, u16 increment);
    void decreaseIndvStat(int charU, int statU, u16 increment);

    int charUsing = 0; // 0 = Mario | 1 = Luigi
    int statUsing = 0; // 0 = HP | 1 = BP | 2 = POW | 3 = DEF | 4 = SPEED | 5 = STACHE
    u16 incrementValue = 0x01; // Default = 1

                                  // 0 = HP     1 = BP     2 = POW      3 = DEF    4 = SPEED  5 = STACHE
    u32 unequippedMarioArray[] = {0x006E842E, 0x006E8432, 0x006E8436, 0x006E843A, 0x006E843E, 0x006E8442};
                               //         0-1 = HP               2-3 = BP        4 = POW     5 = DEF     6 = SPEED   7 = STACHE
    u32 equippedMarioArray[] = {0x006E841E, 0x006E8422, 0x006E8420, 0x006E8424, 0x006E8426, 0x006E8428, 0x006E842A, 0x006E842C};

    u32 unequippedLuigiArray[] = {0x006E8516, 0x006E851A, 0x006E851E, 0x006E8522, 0x006E8526, 0x006E852A};
    u32 equippedLuigiArray[] = {0x006E8506, 0x006E850A, 0x006E8508, 0x006E850C, 0x006E850E, 0x006E8510, 0x006E8512, 0x006E8514};

//////////////////////////////////////////////////////////

u32 getKey() {
	return (*(vu32*)(IoBasePad) ^ 0xFFF) & 0xFFF;
}

void waitKeyUp() {
	while (getKey() != 0) {
		svc_sleepThread(100000000);
	}
}

u8 cheatEnabled[64];
int isNewNtr = 0;


u32 plgGetIoBase(u32 IoType);
GAME_PLUGIN_MENU gamePluginMenu;

void initMenu() {
	memset(&gamePluginMenu, 0, sizeof(GAME_PLUGIN_MENU));
}

void addMenuEntry(u8* str) {
	if (gamePluginMenu.count > 64) {
		return;
	}
	u32 pos = gamePluginMenu.count;
	u32 len = strlen(str) + 1;
	gamePluginMenu.offsetInBuffer[pos] = gamePluginMenu.bufOffset;
	strcpy(&(gamePluginMenu.buf[gamePluginMenu.bufOffset]), str);

	gamePluginMenu.count += 1;
	gamePluginMenu.bufOffset += len;
}

u32 updateMenu() {
	PLGLOADER_INFO *plgLoaderInfo = (void*)0x07000000;
	plgLoaderInfo->gamePluginPid = getCurrentProcessId();
	plgLoaderInfo->gamePluginMenuAddr = (u32)&gamePluginMenu;

	u32 ret = 0;
	u32 hProcess;
	u32 homeMenuPid = plgGetIoBase(5);
	if (homeMenuPid == 0) {
		return 1;
	}
	ret = svc_openProcess(&hProcess, homeMenuPid);
	if (ret != 0) {
		return ret;
	}
	copyRemoteMemory( hProcess, &(plgLoaderInfo->gamePluginPid), CURRENT_PROCESS_HANDLE,  &(plgLoaderInfo->gamePluginPid), 8);
	final:
	svc_closeHandle(hProcess);
	return ret;
}

int scanMenu() {
	u32 i;
	for (i = 0; i < gamePluginMenu.count; i++) {
		if (gamePluginMenu.state[i]) {
			gamePluginMenu.state[i] = 0;
			return i;
		}
	}
	return -1;
}

// add one cheat menu entry
void addCheatMenuEntry(u8* str) {
	u8 buf[100];
	xsprintf(buf, "[ ] %s", str);
	addMenuEntry(buf);
}

// this function will be called when the state of cheat item changed
void onCheatItemChanged(int id, int enable) {
	// TODO: handle on cheat item is select or unselected
	if(id == 2 && enable == 1) charUsing = 0; // Sets the character stat to modify to mario
	if(id == 3 && enable == 1) charUsing = 1; // Sets the character stat to modify to luigi

	if(id == 4 && enable == 1) statUsing = 0; //HP
	if(id == 5 && enable == 1) statUsing = 1; //BP
	if(id == 6 && enable == 1) statUsing = 2; //POW
	if(id == 7 && enable == 1) statUsing = 3; //DEF
	if(id == 8 && enable == 1) statUsing = 4; //SPEED
	if(id == 9 && enable == 1) statUsing = 5; //STACHE

	if(id == 10 && enable == 1) incrementValue = 0x01; //Set the increment value to 1
	if(id == 11 && enable == 1) incrementValue = 0x05; //Set the increment value to 5
	if(id == 12 && enable == 1) incrementValue = 0x0A; //Set the increment value to 10
}

// Used to increase a stat value by the last parameter
void increaseStat(u32 eAddr, u32 ueAddr, u32 value){
	u32 equippedStat = READU32(eAddr);
	u32 unequippedStat = READU32(ueAddr);
	WRITEU32(eAddr, equippedStat + value);
	WRITEU32(ueAddr, unequippedStat + value);
}

// freeze the value
void freezeCheatValue() {
    //Set coins to 999,999
	if(cheatEnabled[0]) WRITEU32(0x006E85EC, 0x000F423F); // Simple coin cheat

}

// update the menu status
void updateCheatEnableDisplay(int id) {
	gamePluginMenu.buf[gamePluginMenu.offsetInBuffer[id] + 1] = cheatEnabled[id] ? 'X' : ' ';
}

// scan and handle events
void scanCheatMenu() {
	int ret = scanMenu();
	if (ret != -1) {
		cheatEnabled[ret] = !cheatEnabled[ret];
		updateCheatEnableDisplay(ret);
		onCheatItemChanged(ret, cheatEnabled[ret]);
	}
}

// init
void initCheatMenu() {
	initMenu();
	addCheatMenuEntry("999,999 Coins");
	addCheatMenuEntry("-----Stat Modifiers----");
	addCheatMenuEntry("R+UP/DN (Mario)");
	addCheatMenuEntry("R+UP/DN (Luigi)");
	addCheatMenuEntry("R+UP/DN - HP");
	addCheatMenuEntry("R+UP/DN - BP");
	addCheatMenuEntry("R+UP/DN - POW");
	addCheatMenuEntry("R+UP/DN - DEF");
	addCheatMenuEntry("R+UP/DN - SPEED");
	addCheatMenuEntry("R+UP/DN - STACHE");
	addCheatMenuEntry("R+UP/DN - Value 1");
	addCheatMenuEntry("R+UP/DN - Value 5");
	addCheatMenuEntry("R+UP/DN - Value 10");
	updateMenu();
}

void gamePluginEntry() {
	u32 ret, key;
	INIT_SHARED_FUNC(plgGetIoBase, 8);
	INIT_SHARED_FUNC(copyRemoteMemory, 9);
	// wait for game starts up (5 seconds)
	svc_sleepThread(5000000000);

	if (((NS_CONFIG*)(NS_CONFIGURE_ADDR))->sharedFunc[8]) {
		isNewNtr = 1;
	} else {
		isNewNtr = 0;
	}

	if (isNewNtr) {
		IoBasePad = plgGetIoBase(IO_BASE_PAD);
	}
	initCheatMenu();
	while (1) {
		svc_sleepThread(100000000);
		scanCheatMenu();
		freezeCheatValue();

		key = getKey();
		switch(key){
            case BUTTON_R + BUTTON_DU:
                increaseIndvStat(charUsing, statUsing, incrementValue);
                waitKeyUp();
            break;
            case BUTTON_R + BUTTON_DD:
                decreaseIndvStat(charUsing, statUsing, incrementValue);
            break;
		}

	}
}

void increaseIndvStat(int charU, int statU, u16 increment)
{

    if(charU == 0) // Mario
    {
        switch(statU)
        {
            case 0:
                WRITEU32(unequippedMarioArray[statU], READU32(unequippedMarioArray[statU]) + increment);
                WRITEU32(equippedMarioArray[statU], READU32(equippedMarioArray[statU]) + increment);
                WRITEU32(equippedMarioArray[statU + 1], READU32(equippedMarioArray[statU + 1]) + increment);
            break;
            case 1:
                WRITEU32(unequippedMarioArray[statU], READU32(unequippedMarioArray[statU]) + increment);
                WRITEU32(equippedMarioArray[statU + 1], READU32(equippedMarioArray[statU + 1]) + increment);
                WRITEU32(equippedMarioArray[statU + 2], READU32(equippedMarioArray[statU + 2]) + increment);
            break;
            default:
                WRITEU32(unequippedMarioArray[statU], READU32(unequippedMarioArray[statU]) + increment);
                WRITEU32(equippedMarioArray[statU + 2], READU32(equippedMarioArray[statU + 2]) + increment);
            break;
        }
    }
    else if(charU == 1) // Luigi
    {
        switch(statU)
        {
            case 0:
                WRITEU32(unequippedLuigiArray[statU], READU32(unequippedLuigiArray[statU]) + increment);
                WRITEU32(equippedLuigiArray[statU], READU32(equippedLuigiArray[statU]) + increment);
                WRITEU32(equippedLuigiArray[statU + 1], READU32(equippedLuigiArray[statU + 1]) + increment);
            break;
            case 1:
                WRITEU32(unequippedLuigiArray[statU], READU32(unequippedLuigiArray[statU]) + increment);
                WRITEU32(equippedLuigiArray[statU + 1], READU32(equippedLuigiArray[statU + 1]) + increment);
                WRITEU32(equippedLuigiArray[statU + 2], READU32(equippedLuigiArray[statU + 2]) + increment);
            break;
            default:
                WRITEU32(unequippedLuigiArray[statU], READU32(unequippedLuigiArray[statU]) + increment);
                WRITEU32(equippedLuigiArray[statU + 2], READU32(equippedLuigiArray[statU + 2]) + increment);
            break;
        }
    }
}

void decreaseIndvStat(int charU, int statU, u16 increment)
{

    if(charU == 0) // Mario
    {
        switch(statU)
        {
            case 0:
                WRITEU32(unequippedMarioArray[statU], READU32(unequippedMarioArray[statU]) - increment);
                WRITEU32(equippedMarioArray[statU], READU32(equippedMarioArray[statU]) - increment);
                WRITEU32(equippedMarioArray[statU + 1], READU32(equippedMarioArray[statU + 1]) - increment);
            break;
            case 1:
                WRITEU32(unequippedMarioArray[statU], READU32(unequippedMarioArray[statU]) - increment);
                WRITEU32(equippedMarioArray[statU + 1], READU32(equippedMarioArray[statU + 1]) - increment);
                WRITEU32(equippedMarioArray[statU + 2], READU32(equippedMarioArray[statU + 2]) - increment);
            break;
            default:
                WRITEU32(unequippedMarioArray[statU], READU32(unequippedMarioArray[statU]) - increment);
                WRITEU32(equippedMarioArray[statU + 2], READU32(equippedMarioArray[statU + 2]) - increment);
            break;
        }
    }
    else if(charU == 1) // Luigi
    {
        switch(statU)
        {
            case 0:
                WRITEU32(unequippedLuigiArray[statU], READU32(unequippedLuigiArray[statU]) - increment);
                WRITEU32(equippedLuigiArray[statU], READU32(equippedLuigiArray[statU]) - increment);
                WRITEU32(equippedLuigiArray[statU + 1], READU32(equippedLuigiArray[statU + 1]) - increment);
            break;
            case 1:
                WRITEU32(unequippedLuigiArray[statU], READU32(unequippedLuigiArray[statU]) - increment);
                WRITEU32(equippedLuigiArray[statU + 1], READU32(equippedLuigiArray[statU + 1]) - increment);
                WRITEU32(equippedLuigiArray[statU + 2], READU32(equippedLuigiArray[statU + 2]) - increment);
            break;
            default:
                WRITEU32(unequippedLuigiArray[statU], READU32(unequippedLuigiArray[statU]) - increment);
                WRITEU32(equippedLuigiArray[statU + 2], READU32(equippedLuigiArray[statU + 2]) - increment);
            break;
        }
    }
}
