// chants.cpp
#include <windows.h>
#include <stdio.h>
#include "chants.h"
#include "kload_exp.h"
#include "afsreplace.h"
#include <map>
#include <unordered_map>
#include <random>
#include <vector>


KMOD k_chants={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;

class SoundPack 
{
	public:
		SoundPack() : _chantsFolder(), _anthemFolder() {}
		string _chantsFolder;
		string _anthemFolder;
};

static unordered_map<WORD, SoundPack> g_chantsMap;

bool homeHasChants = false;
bool awayHasChants = false;
string homeChant = "";
string awayChant = "";

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitChants();
bool chantsGetNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages);
bool chantsAfterReadFile(HANDLE hFile, LPVOID lpBuffer, 
        DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,  
        LPOVERLAPPED lpOverlapped);

void chantsBeginUniSelect();
void ReadMap();
WORD GetTeamId(int which);
void relinkChants();
string getRandomChant(string& currDir);
DWORD GetGameMode();

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{	
		Log(&k_chants,"Attaching dll...");
		
		switch (GetPESInfo()->GameVersion) 
		{
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //... and WE9 PC
            case gvWE9LEPC: //... and WE9:LE PC
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_chants,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:
		
		RegisterKModule(&k_chants);
		
		memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));
		
		
		
		HookFunction(hk_D3D_CreateDevice,(DWORD)InitChants);		

	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_chants,"Detaching dll...");
		UnhookFunction(hk_BeginUniSelect, (DWORD)chantsBeginUniSelect);

		UnhookFunction(hk_AfterReadFile,(DWORD)chantsAfterReadFile);
		
		g_chantsMap.clear();
		
		Log(&k_chants,"Detaching done.");
	};

	return true;
};

void InitChants()
{
    
	UnhookFunction(hk_D3D_CreateDevice,(DWORD)InitChants);

	ReadMap();
	if (g_chantsMap.empty()) 
	{
		Log(&k_chants, "Chants map empty, not need to attach anything else");
		return;

	}

	HookFunction(hk_BeginUniSelect, (DWORD)chantsBeginUniSelect);
	HookFunction(hk_GetNumPages, (DWORD)chantsGetNumPages);
	HookFunction(hk_AfterReadFile, (DWORD)chantsAfterReadFile);

}

bool FileExists(const char* filename)
{
	TRACE4(&k_chants, "FileExists: Checking file: %s", (char*)filename);
	HANDLE hFile;
	hFile = CreateFile(TEXT(filename),        // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);                 // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	CloseHandle(hFile);
	return TRUE;
};

bool chantsGetNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages)
{
    DWORD fileSize = 0;
    char filename[512] = {0};
	WORD homeTeamId = GetTeamId(HOME);
	WORD awayTeamId = GetTeamId(AWAY);
	unordered_map<WORD, SoundPack>::iterator itHome = g_chantsMap.find(homeTeamId);
	unordered_map<WORD, SoundPack>::iterator itAway = g_chantsMap.find(awayTeamId);

	bool result = false;

    if (afsId == AFS_FILE)  // 0_sound.afs
	{
		if (
			fileId >= dta[HOME_CHANT_ID] && 
			fileId < dta[HOME_CHANT_ID] + TOTAL_CHANTS && 
			homeHasChants &&
			itHome != g_chantsMap.end()
			)
		{
			homeChant = getRandomChant(itHome->second._chantsFolder);
			if (homeChant.empty()) return result;
			LOG(&k_chants, "home chant now is (%s)", homeChant.c_str());
			sprintf(filename, "%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, itHome->second._chantsFolder.c_str(), homeChant.c_str());
		}
		else if (
			fileId >= dta[AWAY_CHANT_ID] && 
			fileId < dta[AWAY_CHANT_ID] + TOTAL_CHANTS && 
			awayHasChants &&
			itAway != g_chantsMap.end()
			) 
		{
			awayChant = getRandomChant(itAway->second._chantsFolder);
			if (awayChant.empty()) return result;
			sprintf(filename, "%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, itAway->second._chantsFolder.c_str(), awayChant.c_str());
		}
		else if (
			(fileId == ENTRANCE_0 || fileId == ENTRANCE_1) &&
			itHome != g_chantsMap.end()
			)
		{
			if (itHome->second._anthemFolder.empty()) return result;

			sprintf(filename, "%sGDB\\chants\\%s\\%s%s", GetPESInfo()->gdbDir, itHome->second._anthemFolder.c_str(), fileNames[GetGameMode()], ".adx");
			if (!(FileExists(filename)))
			{
				ZeroMemory(filename, sizeof(filename));
				sprintf(filename, "%sGDB\\chants\\%s\\%s%s", GetPESInfo()->gdbDir, itHome->second._anthemFolder.c_str(), fileNames[sizeof(fileNames) - 1], ".adx");
			}
		}
		else
		{
			return result;
		}

		HANDLE TempHandle=CreateFile(filename,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (TempHandle!=INVALID_HANDLE_VALUE) 
		{
			fileSize=GetFileSize(TempHandle,NULL);
			CloseHandle(TempHandle);
		}
		else 
		{
			fileSize=0;
		};

		if (fileSize > 0) 
		{
			/*LogWithString(&k_chants, "chantsReadNumPages: found file: %s", filename);
			LogWithTwoNumbers(&k_chants,"chantsReadNumPages: had size: %08x pages (%08x bytes)", 
					orgNumPages, orgNumPages*0x800);*/

			// adjust buffer size to fit file
			*numPages = ((fileSize-1)>>0x0b)+1;
			/*LOG(&k_chants,
					"chantsReadNumPages: new size: %08x pages (%08x bytes)", 
					*numPages, (*numPages)*0x800);*/
			result = true;
		}
    }
    return result;
}


bool chantsAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	DWORD afsId = GetAfsIdByOpenHandle(hFile);
    DWORD filesize = 0;
    char filename[512] = {0};
	bool result = false;

	// ensure it is 0_sound.afs
	if (afsId == AFS_FILE) 
	{
		DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - (*lpNumberOfBytesRead);
		DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);
		WORD homeTeamId = GetTeamId(HOME);
		WORD awayTeamId = GetTeamId(AWAY);
		unordered_map<WORD, SoundPack>::iterator itHome = g_chantsMap.find(homeTeamId);
		unordered_map<WORD, SoundPack>::iterator itAway = g_chantsMap.find(awayTeamId);

		if (
			fileId >= dta[HOME_CHANT_ID] && 
			fileId < dta[HOME_CHANT_ID] + TOTAL_CHANTS && 
			homeHasChants && 
			itHome != g_chantsMap.end() &&
			!(homeChant.empty())
			)
		{
			sprintf(filename, "%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, itHome->second._chantsFolder.c_str(), homeChant.c_str());
			LOG(&k_chants, "loading home chant (%s)", filename);
		}
		else if (
			fileId >= dta[AWAY_CHANT_ID] && 
			fileId < dta[AWAY_CHANT_ID] + TOTAL_CHANTS && 
			awayHasChants &&
			itAway != g_chantsMap.end() &&
			!(awayChant.empty())
			)
		{
			sprintf(filename, "%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, itAway->second._chantsFolder.c_str(), awayChant.c_str());
			LOG(&k_chants, "loading away chant (%s)", filename);

		}
		else if(
			(fileId == ENTRANCE_0 || fileId == ENTRANCE_1) && 
			itHome != g_chantsMap.end() &&
			!(itHome->second._anthemFolder.empty())
			)
		{
			sprintf(filename, "%sGDB\\chants\\%s\\%s%s", GetPESInfo()->gdbDir, itHome->second._anthemFolder.c_str(), fileNames[GetGameMode()], ".adx");
			if (!(FileExists(filename)))
			{
				ZeroMemory(filename, sizeof(filename));
				sprintf(filename, "%sGDB\\chants\\%s\\%s%s", GetPESInfo()->gdbDir, itHome->second._anthemFolder.c_str(), fileNames[sizeof(fileNames) - 1], ".adx");
			}
			LOG(&k_chants, "loading anthem (%s)", filename);


		}
		else
		{
			return result;
		}

		LOG(&k_chants, "loading: (%s) file id: (%d)", filename, fileId);
		HANDLE handle = CreateFile(
		filename, 
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
		if (handle!=INVALID_HANDLE_VALUE) 
		{
			filesize = GetFileSize(handle,NULL);
			DWORD curr_offset = offset - GetOffsetByFileId(afsId, fileId);
			/*LOG(&k_chants, 
			"offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
			offset, GetOffsetByFileId(afsId, fileId), curr_offset);*/
			DWORD bytesToRead = *lpNumberOfBytesRead;
			if (filesize < curr_offset + *lpNumberOfBytesRead) 
			{
				bytesToRead = filesize - curr_offset;
			}
			DWORD bytesRead = 0;
			SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
			/*LOG(&k_chants, "reading 0x%x bytes (0x%x) from {%s}", 
			bytesToRead, *lpNumberOfBytesRead, filename);*/
			ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
			//LOG(&k_chants, "read 0x%x bytes from {%s}", bytesRead, filename);
			if (*lpNumberOfBytesRead - bytesRead > 0) 
			{
				/*DEBUGLOG(&k_chants, "zeroing out 0x%0x bytes",
				*lpNumberOfBytesRead - bytesRead);*/
				memset(
				(BYTE*)lpBuffer+bytesRead, 0, *lpNumberOfBytesRead-bytesRead);
			}
			CloseHandle(handle);
			// set next likely read
			if (filesize > curr_offset + bytesRead) 
			{
				SetNextProbableReadForHandle(
				afsId, offset+bytesRead, fileId, hFile);
			}
			result = true;
		}
		else 
		{
			LOG(&k_chants, "Unable to read file {%s} must be protected or opened", filename);
		}
		//return result;
	}
	return result;
}


void chantsBeginUniSelect()
{
    Log(&k_chants, "chantsBeginUniSelect");

    // check if home and away team has a chants designated in map.txt
	WORD homeTeamId = GetTeamId(HOME);
	WORD awayTeamId = GetTeamId(AWAY);
	unordered_map<WORD, SoundPack>::iterator itHome = g_chantsMap.find(homeTeamId);
	unordered_map<WORD, SoundPack>::iterator itAway = g_chantsMap.find(awayTeamId);
	homeHasChants = itHome != g_chantsMap.end() && !itHome->second._chantsFolder.empty();
	homeChant = "";
	awayHasChants = itAway != g_chantsMap.end() && !itAway->second._chantsFolder.empty();
	awayChant = "";

	relinkChants();
    return;
}

void relinkChants()
{
	if (VirtualProtect((LPVOID)dta[CHANTS_ADDRESS], dta[TOTAL_TEAMS] * 4, newProtection, &protection)) 
	{
		if (homeHasChants)
		{
			WORD homeTeamId = GetTeamId(HOME);
			*(WORD*)(dta[CHANTS_ADDRESS] + homeTeamId * 4) = dta[HOME_CHANT_ID];
			*(WORD*)(dta[CHANTS_ADDRESS] + homeTeamId * 4 + 2) = AFS_FILE;
		}
		if (awayHasChants)
		{
			WORD awayTeamId = GetTeamId(AWAY);
			*(WORD*)(dta[CHANTS_ADDRESS] + awayTeamId * 4) = dta[AWAY_CHANT_ID];
			*(WORD*)(dta[CHANTS_ADDRESS] + awayTeamId * 4 + 2) = AFS_FILE;
		}
	};

}

string getRandomChant(string& currDir) 
{
	if (currDir.empty()) return "";
	vector<string> chantsFiles;
	WIN32_FIND_DATA fData;
	char pattern[512] = { 0 };
	ZeroMemory(pattern, sizeof(pattern));

	sprintf(pattern, "%sGDB\\chants\\%s\\*",
		GetPESInfo()->gdbDir, currDir.c_str());
	LOG(&k_chants, "pattern: {%s}", pattern);


	HANDLE hff = FindFirstFile(pattern, &fData);
	if (hff != INVALID_HANDLE_VALUE) 
	{
		while (true) 
		{
			if (stricmp(fData.cFileName + strlen(fData.cFileName) - 4, ".adx") == 0) 
			{
				// a bin or str: consider it a banner texture
				chantsFiles.push_back(string(fData.cFileName));
				LogWithString(&k_chants,
					"Found chant file: %s", (char*)chantsFiles.back().c_str());
			}
			// proceed to next file
			if (!FindNextFile(hff, &fData)) break;
		}
		FindClose(hff);
	}
	if (chantsFiles.empty()) 
	{
		LOG(&k_chants, "Couldnt find any adx file in here.");
		return "";
	}

	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<> dis(0, static_cast<int>(chantsFiles.size()) - 1);
	int randomIndex = dis(gen);

	return chantsFiles[randomIndex];
}

void ReadMap()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char* comment = NULL;
	char buf[BUFLEN];
	WORD number = 0;

	strcpy(tmp, GetPESInfo()->gdbDir);
	strcat(tmp, "GDB\\chants\\map.txt");

	FILE* cfg = fopen(tmp, "rt");
	if (cfg == NULL) 
	{
		Log(&k_chants, "readMap: Couldn't find chants map!");
		return;
	}

	while (true) 
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN - 1, cfg);

		// skip comments
		comment = NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		char* p = strchr(str, ',');
		if (p) 
		{
			if (sscanf(str, "%d", &number) == 1) 
			{
				SoundPack pack;
				int token = 2;
				while (p) 
				{
					ZeroMemory(buf, BUFLEN);
					char* start = strchr(p + 1, '\"');
					if (!start) break;
					char* finish = strchr(start + 1, '\"');
					if (!finish) break;
					memcpy(buf, start + 1, finish - start - 1);
					LOG(&k_chants, "token (%d): {%s}", token, buf);
					switch (token) 
					{
						case 2: pack._chantsFolder = buf; break;
						case 3: pack._anthemFolder = buf; break;
					}
					token++;
					p = strchr(finish + 1, ',');
				}
				if (!pack._chantsFolder.empty() || !pack._anthemFolder.empty())
				{
					LOG(&k_chants, "id:%d, chantsFolder:{%s}, anthemFolder:{%s}",
						number, pack._chantsFolder.c_str(), pack._anthemFolder.c_str());
					g_chantsMap.insert(pair<WORD, SoundPack>(number, pack));
				}
			}
		}

		if (feof(cfg)) break;
	}
	fclose(cfg);
	LogWithNumber(&k_chants, "readMap: g_chantsMap.size() = %d", g_chantsMap.size());
}

bool isLeagueCup(BYTE competitionId) 
{
	return (
		competitionId == ISS_CUP ||
		competitionId == ITALIAN_LEAGUE_CUP ||
		competitionId == ENGLISH_LEAGUE_CUP ||
		competitionId == FRENCH_LEAGUE_CUP ||
		competitionId == GERMAN_LEAGUE_CUP ||
		competitionId == HOLLAND_LEAGUE_CUP ||
		competitionId == SPANISH_LEAGUE_CUP
		);
}

DWORD GetGameMode() 
{
	DWORD mainMenuMode = *(DWORD*)dta[MAIN_MENU_MODE];
	BYTE competitionId = *(BYTE*)dta[COMPETITION_ID];
	DWORD index;
	switch (mainMenuMode)
	{
	case 1:
		switch (competitionId)
		{
			case ML_DIV_1_LEAGUE:
				index = ML_DIV1_LEAGUE_FILE;
				break;
			case ML_DIV_1_CUP:
				index = ML_DIV1_CUP_FILE;
				break;
			case ML_DIV_2_LEAGUE:
				index = ML_DIV2_LEAGUE_FILE;
				break;
			case ML_DIV_2_CUP:
				index = ML_DIV2_CUP_FILE;
				break;
			case WEFA_CHAMPIONSHIP_FIRST_ROUND:
			case WEFA_CHAMPIONSHIP_PLAY_OFF:
			case WEFA_CHAMPIONSHIP_SECOND_ROUND:
				index = WEFA_CHAMPIONSHIP_FILE;
				break;
			case WEFA_MASTERS_CUP:
				index = WEFA_MASTERS_CUP;
				break;
			default:
				index = DEFAULT_FILE;
				break;
		}
		break;
	case 2:
		index = LEAGUE_FILE;
		if (isLeagueCup(competitionId))
		{
			index = LEAGUE_CUP_FILE;
		}
		break;
	case 3:
		index = CUP_FILE;
		if (competitionId == INTERNATIONAL_CUP_1 || competitionId == INTERNATIONAL_CUP_2)
		{
			index = WORLD_CUP_FILE;
		}
		else if (competitionId == KONAMI_CUP)
		{
			index = KONAMI_CUP_FILE;
		} 
		break;
	default:
		index = DEFAULT_FILE;
		break;
	}

	return index;
}

WORD GetTeamId(int which)
{
    BYTE* mlData;
    if (dta[TEAM_IDS]==0) return 0xffff;
    WORD id = ((WORD*)dta[TEAM_IDS])[which];
    if (id==0xf2 || id==0xf3) 
	{
        switch (id) 
		{
            case 0xf2:
                // master league team (home)
                mlData = *((BYTE**)dta[ML_HOME_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
            case 0xf3:
                // master league team (away)
                mlData = *((BYTE**)dta[ML_AWAY_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
        }
    }
    return id;
}

