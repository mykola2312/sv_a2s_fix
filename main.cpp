#define GAME_DLL
#include "cbase.h"
#include "bitbuf.h"
#include "plugin.h"
#include "glua.h"
#include "chook.h"

typedef struct {
	uint16 family;
	uint16 port;
	uint32 addr;
} ws2_addr;

#define LUA_CALLBACK "onServerQuery"
#define LUA_CALLBACK2 "onClientResponse"

typedef int (__stdcall* SendTo_t)(int,const char*,int,int,void*,int);

DECLARE_PLUGIN(CA2SFix)
public:
	virtual bool Load(CreateInterfaceFn,
		CreateInterfaceFn);
	virtual void Unload();
	virtual bool LuaInit(lua_State*);
	bool IsResponseAllowed(uint32 naddr,unsigned char req);
	bool ProcessResponse(bf_read&,bf_write&,uint32);
	bool ProcessLua(bf_read&,bf_write&,uint32);

	bool m_bLua;
	lua_State* L;
	static CHook* s_pSendHook;
	static SendTo_t s_SendTo;
END_PLUGIN(CA2SFix,"A2S Fix");

CHook* CA2SFix::s_pSendHook = NULL;
SendTo_t CA2SFix::s_SendTo = NULL;

static int __stdcall SendTo_Hook(int s,const char* buf,
	int len,int flags,void* addr,int tolen)
{
	static char s_szBuf[1024];
	uint32 naddr = ((ws2_addr*)addr)->addr;
	if(*(uint32*)buf == 0xFFFFFFFF)
	{
		if(!s_CA2SFix.IsResponseAllowed(naddr,(unsigned char)buf[4]))
			return len;
	}
	if(*(uint32*)buf == 0xFFFFFFFF && buf[4] == 0x49)
	{
		memset(s_szBuf,'\0',1024);
		bf_read br(buf,len);
		bf_write bf(s_szBuf,1024);
		if(!s_CA2SFix.ProcessResponse(br,bf,naddr))
			return len;
		return CA2SFix::s_SendTo(s,(const char*)bf.GetData(),
			bf.GetNumBytesWritten(),flags,addr,tolen);
	}
	return CA2SFix::s_SendTo(s,buf,len,flags,addr,tolen);
}

bool CA2SFix::Load(CreateInterfaceFn engineFn,CreateInterfaceFn)
{
	s_pSendHook = new CHook((DWORD)GetProcAddress(
		GetModuleHandle("WS2_32.dll"),"sendto"),5);
	s_SendTo = (SendTo_t)s_pSendHook
		->HookFunction((DWORD)&SendTo_Hook);
	m_bLua = (GetModuleHandle("lua502.dll") != NULL);
	return true;
}

void CA2SFix::Unload()
{
	delete s_pSendHook;
}

bool CA2SFix::LuaInit(lua_State* _L)
{
	L = _L;
	return true;
}

bool CA2SFix::ProcessResponse(bf_read& br,bf_write& bf,uint32 naddr)
{
	bf.WriteLong(br.ReadLong());
	bf.WriteByte(br.ReadByte());
	br.ReadByte();
	bf.WriteByte(7); //Protocol version
	if(m_bLua)
	{
		if(!ProcessLua(br,bf,naddr))
			return false;
	}
	else bf.WriteBitsFromBuffer(&br,br.GetNumBitsLeft());
	return true;
}

#define lua_setstring(L,name,s)			\
	lua_pushstring(L,name);				\
	lua_pushstring(L,s);				\
	lua_settable(L,-3)

#define lua_setnumber(L,name,d)			\
	lua_pushstring(L,name);				\
	lua_pushnumber(L,(double)d);		\
	lua_settable(L,-3)

inline const char* lua_getstring(lua_State* L,
	const char* name)
{
	lua_pushstring(L,name);
	lua_gettable(L,-2);
	const char* pStr = lua_tostring(L,-1);
	lua_pop(L,1);
	return pStr;
}

inline int lua_getnumber(lua_State* L,
	const char* name)
{
	lua_pushstring(L,name);
	lua_gettable(L,-2);
	int pNum = lua_tonumber(L,-1);
	lua_pop(L,1);
	return pNum;
}

int _aterror(lua_State* L)
{
	return g_pLua502->GetLuaCallbacks()->OnLuaError(L);
}

inline char* addr2str(uint32 naddr)
{
	static char szBuf[32] = {0};
	unsigned char* ip = (unsigned char*)&naddr;
	sprintf(szBuf,"%d.%d.%d.%d",ip[0],
		ip[1],ip[2],ip[3]);
	return szBuf;
}

bool CA2SFix::IsResponseAllowed(uint32 naddr,unsigned char req)
{
	lua_pushcfunction(L,_aterror);
	int iHandler = lua_gettop(L);

	lua_getglobal(L,LUA_CALLBACK2);
	if(lua_isnil(L,-1))
	{
		lua_pop(L,2);
		return true;
	}

	lua_pushstring(L,addr2str(naddr));
	lua_pushnumber(L,req);
	lua_pcall(L,2,1,iHandler);
	bool bRet = lua_toboolean(L,-1);
	lua_pop(L,2);
	return bRet;
}

bool CA2SFix::ProcessLua(bf_read& br,bf_write& bf,uint32 naddr)
{
	lua_pushcfunction(L,_aterror);
	int iHandler = lua_gettop(L);

	lua_getglobal(L,LUA_CALLBACK);
	if(lua_isnil(L,-1))
	{
		lua_pop(L,2);
		bf.WriteBitsFromBuffer(&br,br.GetNumBitsLeft());
		return true;
	}

	static char szServer[256] = {0};
	static char szMapName[128] = {0};
	static char szFolder[128];
	static char szGame[64] = {0};
	static char szVersion[32] = {0};
	int iAppId,iPlayers,iMaxPlayers,iBots;
	char szSvType[2] = {0,0};
	char szSvEnv[2] = {0,0};
	bool bVisibility,bVac;

	br.ReadString(szServer,256);
	br.ReadString(szMapName,128);
	br.ReadString(szFolder,128);
	br.ReadString(szGame,64);

	iAppId = br.ReadWord();
	iPlayers = br.ReadByte();
	iMaxPlayers = br.ReadByte();
	iBots = br.ReadByte();

	szSvType[0] = br.ReadByte();
	szSvEnv[0] = br.ReadByte();

	bVisibility = br.ReadByte();
	bVac = br.ReadByte();
	br.ReadString(szVersion,32);

	lua_newtable(L);

	lua_setstring(L,"name",szServer);
	lua_setstring(L,"map",szMapName);
	lua_setstring(L,"folder",szFolder);
	lua_setstring(L,"game",szGame);

	lua_setnumber(L,"appid",iAppId);
	lua_setnumber(L,"players",iPlayers);
	lua_setnumber(L,"maxplayers",iMaxPlayers);
	lua_setnumber(L,"bots",iBots);
	
	lua_setstring(L,"type",szSvType);
	lua_setstring(L,"env",szSvEnv);

	lua_setnumber(L,"visibility",bVisibility);
	lua_setnumber(L,"vac",bVac);

	lua_setstring(L,"version",szVersion);

	lua_pushstring(L,addr2str(naddr));
	lua_pcall(L,2,1,iHandler);
	if(lua_isnil(L,-1))
	{
		lua_pop(L,2);
		bf.WriteBitsFromBuffer(&br,br.GetNumBitsLeft());
		return true;
	}

	bf.WriteString(lua_getstring(L,"name"));
	bf.WriteString(lua_getstring(L,"map"));
	bf.WriteString(lua_getstring(L,"folder"));
	bf.WriteString(lua_getstring(L,"game"));

	bf.WriteWord(lua_getnumber(L,"appid"));
	bf.WriteByte(lua_getnumber(L,"players"));
	bf.WriteByte(lua_getnumber(L,"maxplayers"));
	bf.WriteByte(lua_getnumber(L,"bots"));

	bf.WriteByte(lua_getstring(L,"type")[0]);
	bf.WriteByte(lua_getstring(L,"env")[0]);

	bf.WriteByte(lua_getnumber(L,"visibility"));
	bf.WriteByte(lua_getnumber(L,"vac"));

	bf.WriteString(lua_getstring(L,"version"));
	bf.WriteBitsFromBuffer(&br,br.GetNumBitsLeft());

	lua_pop(L,2);
	return true;
}