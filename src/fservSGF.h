#ifndef _FSERV_SGF_H_
#define _FSERV_SGF_H_

//exported to the savegame manager module

typedef void   (*FSERVSETPLAYERREPLAYFILENAME)(BYTE,BYTE,BYTE,char*);
typedef void   (*FSERVSETPLAYERREPLAYDATA)(BYTE,BYTE,BYTE,DWORD,BYTE*);
typedef void   (*FSERVRESETPLAYERSREPLAYDATA)();

typedef struct _FSERVSAVEGAMEFUNCS {
	FSERVSETPLAYERREPLAYFILENAME SetPlayerReplayFilename;
	FSERVSETPLAYERREPLAYDATA SetPlayerReplayData;
	FSERVRESETPLAYERSREPLAYDATA ResetPlayersReplayData;
} FSERVSAVEGAMEFUNCS;

#endif