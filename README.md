# sv_a2s_fix
## It's a plugin for GMod 9 Lua502 Extensions - https://github.com/nikolaytihonov/lua502

Yesterday, Valve has broke old master servers, so GMod 9 players can't see other servers with sv_master_legacy_mode 1, 
but sv_master_legacy_mode 0 is broken too. The bug happends when server with sv_master_legacy_mode 0 sends A2S_INFO to
client, the Protocol Version is 17, but GMod 9 and Source Engine 2006 runs Protocol Version 7, so this plugin replaces
Protocol Version and (!) calls lua callback to let you change the server info (like server name, map name, gamemode
or even version).

## Lua callbacks
```
function onServerQuery(info,addr)
  return info
end
```

The ```info``` is a table that holds original server information (you can see full list of fields via tprint),

To sends changed info, just return table. If don't want send anything, return nil.
The ```addr``` is just ip of a client

```
function onClientResponse(addr,req)
	return true
end
```

This callback called when server tried to respond for connectionless packet. The ```req``` holds ID of a packet,
(for example 0x49 for A2S_INFO)

Return true to allow response send, false to block.
