#/bin/sh
stap -v mini_lua_bt.stp -g -c "/root/lua5.4.4/src/lua a.lua" "/root/lua5.4.4/src/lua" -BCONFIG_MODULE_SIG=n --suppress-time-limits -DMAXSTRINGLEN=65536  |tee a.bt

./flamegraph.pl --width=2400 a.bt >lua.svg
