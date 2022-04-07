#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "lua.h"
#include "lauxlib.h"
#define DEFAULT_LIMIT (2^20)

#define uchar(c)	((unsigned char)(c))

/*
#define LUA_TNONE		(-1)
#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8
#define LUA_NUMTYPES		9
*/

typedef struct WriteBuffer {
    size_t size;
    size_t head;
    char*  data;
} Buffer;

static void append_table(lua_State *L, int index, Buffer *buf, int ele_cnt);
static void append_val(lua_State *L, int index, Buffer *buf, int elecnt);

static void 
buffer_init(lua_State *L, Buffer *buf)
{
    buf->size = 128;
    buf->head = 0;
    if (!(buf->data = malloc(buf->size))){
        luaL_error(L, "Out of memory!");
    }
}

static void 
bufer_free(lua_State* L, Buffer *buf)
{
    free(buf->data);
}

static int 
bufer_write(lua_State* L, const char *str, size_t len, Buffer *buf)
{
    if (len > UINT32_MAX) {
        luaL_error(L, "buffer too long!");
    }
    if (buf->size - buf->head < len) {
        size_t new_size = buf->size << 1;
        size_t cur_head = buf->head;
        while (new_size - cur_head <= len) {
            new_size = new_size << 1;
        }
        char * p = NULL;
        if (!(p = realloc(buf->data, new_size))) {
            luaL_error(L, "Out of memory!");
        }
        buf->data = p;
        buf->size = new_size;
    }
    memcpy(&buf->data[buf->head], str, len);
    buf->head += len;
    return 0;
}

//write char
static int 
bufer_write_char(lua_State* L, char str, Buffer *buf)
{
    if (buf->size - buf->head < 1) {
        size_t new_size = buf->size << 1;
        size_t cur_head = buf->head;
        while (new_size - cur_head <= 1) {
            new_size = new_size << 1;
        }
        char * p = NULL;
        if (!(p = realloc(buf->data, new_size))) {
            luaL_error(L, "Out of memory!");
        }
        buf->data = p;
        buf->size = new_size;
    }
    buf->data[buf->head] = str;
    buf->head += 1;
    return 0;
}

// write bool
static void
buffer_write_bool(lua_State *L, int index, Buffer *buf) {
    const char * str = lua_toboolean(L, index) ? "true" : "false";
    bufer_write(L, str, strlen(str), buf);
}

// write string
static void
buffer_write_string(lua_State *L, int index, Buffer *buf) {
    size_t len;
    const char *s = lua_tolstring(L, index, &len);
    bufer_write_char(L, '\"',  buf);
    while (len--) {
        if (*s == '"' || *s == '\\' || *s == '\n') {
            bufer_write_char(L, (char)'\\',  buf);
            bufer_write_char(L, *s,  buf);
        } else if (iscntrl(uchar(*s))) {
            char buff[10];
            if (!isdigit(uchar(*(s+1)))){
                sprintf(buff, "\\%d", (int)uchar(*s));
                bufer_write(L, buff, strlen(buff), buf);
            } else {
                sprintf(buff, "\\%03d", (int)uchar(*s));
                bufer_write(L, buff, strlen(buff), buf);
            }
        }
        else {
            bufer_write_char(L, *s,  buf);
        }
        s++;
    }
    bufer_write_char(L, '\"', buf);
}

#define MAXNUMBER2STR	44
static void
buffer_write_number(lua_State *L, int index, Buffer *buf) {
    size_t len;
    char buff[MAXNUMBER2STR];
    if (lua_isinteger(L, index)){
        len = lua_integer2str(buff, MAXNUMBER2STR, lua_tointeger(L, index));
        bufer_write(L, buff, len, buf);
    } else {
        len = lua_number2str(buff, MAXNUMBER2STR, lua_tonumber(L, index));
        if (buff[strspn(buff, "-0123456789")] == '\0') {  /* looks like an int? */
        buff[len++] = '.';
        buff[len++] = '0';  /* adds '.0' to result */
        }
        bufer_write(L, buff, len, buf);
    }
}

static void
buffer_write_point(lua_State *L, int index, Buffer *buf) {
	char tmp[128];
	memset(tmp,0,sizeof(tmp));
    sprintf(tmp,"\"%s: %p\"", luaL_typename(L, index), lua_topointer(L, index));
    bufer_write(L, tmp, strlen(tmp), buf);
}

static void
append_table(lua_State *L, int index, Buffer *buf, int ele_cnt) {
    bufer_write_char(L, '{', buf);
	
	if(ele_cnt <= 0){
	    bufer_write_char(L, '}',  buf);
		return;
	}
	
    int base = lua_gettop(L);
	if(index < 0){
		index = base + index + 1;
	}

	if (luaL_getmetafield(L, index, "__pairs") == LUA_TNIL) {  /* no metamethod? */
		lua_pushnil(L);
		while (lua_next(L, index) != 0) {
			lua_pushvalue(L, -2);
			bufer_write_char(L, '[', buf);
			append_val(L, -2, buf, ele_cnt);
			bufer_write(L, "]=", 2, buf); 
            lua_pop(L, 1);

			append_val(L, -1, buf, ele_cnt);
			bufer_write_char(L, ',', buf);
			lua_pop(L, 1);
		}
	}else{
		int top,iter_func,iter_table,iter_key;
		lua_pushvalue(L, index); /* argument 'self' to metamethod */
		lua_call(L, 1, 3);    /* get 3 values from metamethod */
		top = lua_gettop(L);
		iter_func = top - 2;
		iter_table = top - 1;
		iter_key = top;
		for(;;) {
			lua_pushvalue(L, iter_func);
			lua_pushvalue(L, iter_table);
			lua_pushvalue(L, -3);
			lua_call(L, 2, 2);
			int type = lua_type(L, -2);
			if (type == LUA_TNIL) {
				lua_settop(L,base);
				break;
			}
			lua_pushvalue(L, -2);
			bufer_write_char(L, '[', buf);
			append_val(L, -1, buf, ele_cnt);
			bufer_write(L, "]=", 2, buf);
			lua_pop(L, 1);
			append_val(L, -1, buf, ele_cnt);
			bufer_write_char(L, ',', buf);
			lua_pop(L, 1);
		}
	}
    bufer_write_char(L, '}',  buf);
}

static void
append_val(lua_State *L, int index, Buffer *buf, int ele_cnt) {
    int vtype = lua_type(L, index);
    switch (vtype) {
        case LUA_TSTRING:{
            buffer_write_string(L, index, buf);
            break;
        }
        case LUA_TNUMBER:{
            buffer_write_number(L, index, buf);
            break;
        }
        case LUA_TTABLE:{
            append_table(L, index, buf, ele_cnt-1);
            break;
        }
        case LUA_TBOOLEAN:{
            buffer_write_bool(L, index, buf);        
            break;
        }
        case LUA_TNIL:{
            bufer_write(L, "nil", 3, buf);
            break;
        }
        default:{
			buffer_write_point(L, index, buf);
            break;
        }
    }
}

static int 
encode(lua_State *L) {
	struct Buffer *buf = lua_touserdata(L, 2);
	int ele_cnt = 20;
	append_val(L, 1, buf, ele_cnt);
	return 0;
}

static int
lencode(lua_State *L) {
	lua_settop(L,1);
	luaL_checktype(L, 1, LUA_TTABLE);
	
	Buffer buf;
	buffer_init(L, &buf);
	lua_pushcfunction(L, encode);
	lua_pushvalue(L, 1);
	lua_pushlightuserdata(L, &buf);
	if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
		bufer_free(L, &buf);
		return lua_error(L);
	}
	lua_pushlstring(L, buf.data, buf.head);
	bufer_free(L, &buf);
	return 1;
}

LUALIB_API int luaopen_serialize(lua_State *L) {
    luaL_Reg libs[] = {
    	{ "lencode", lencode },
        { NULL, NULL }
    };
    luaL_newlib(L, libs);
    return 1;
}
