#include "movieclipbinder.h"
#include "movieclip2.h"
#include "stackchecker.h"
#include "stringid.h"
#include "luaapplication.h"
#include <luautil.h>

#define FRAME "frame"
#define TIME "time"

MovieClipBinder::MovieClipBinder(lua_State* L)
{
	Binder binder(L);

	static const luaL_Reg functionList[] = {
		{"play", play},
		{"stop", stop},
		{"gotoAndPlay", gotoAndPlay},
		{"gotoAndStop", gotoAndStop},
		{"setStopAction", setStopAction},
		{"setGotoAction", setGotoAction},
		{"clearAction", clearAction},
		{NULL, NULL},
	};

	binder.createClass("MovieClip", "Sprite", create, destruct, functionList);


    lua_getglobal(L, "MovieClip");

    lua_pushstring(L, FRAME);
    lua_setfield(L, -2, "FRAME");

    lua_pushstring(L, TIME);
    lua_setfield(L, -2, "TIME");

    lua_pop(L, 1);
}


struct AutoUnref
{
public:
	AutoUnref(GReferenced* refptr) : refptr_(refptr)
	{
	}

	~AutoUnref()
	{
		if (refptr_)
			refptr_->unref();
	}

	void release()
	{
		refptr_ = NULL;
	}

private:
	GReferenced* refptr_;
};

#ifndef abs_index

/* convert a stack index to positive */
#define abs_index(L, i)		((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : \
	lua_gettop(L) + (i) + 1)

#endif

int MovieClipBinder::create(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::create", 1);

    LuaApplication* application = static_cast<LuaApplication*>(luaL_getdata(L));

    Binder binder(L);


    int index;
    MovieClip::Type type;

    if (lua_type(L, 1) == LUA_TTABLE)
    {
        index = 1;
        type = MovieClip::eFrame;
    }
    else if (lua_type(L, 1) == LUA_TSTRING)
    {
        if (lua_type(L, 2) != LUA_TTABLE)
            return luaL_typerror(L, 2, "table");

        const char *t = lua_tostring(L, 1);

        if (!strcmp(t, FRAME))
            type = MovieClip::eFrame;
        if (!strcmp(t, TIME))
            type = MovieClip::eTime;
        else
        {
            GStatus status(2008, "type");	// Parameter %s must be one of the accepted values.
            return luaL_error(binder.L, status.errorString());
        }

        index = 2;
    }
    else
        return luaL_typerror(L, 1, "string or table");

    if (lua_objlen(L, index) == 0)
        luaL_error(L, GStatus(2102).errorString());     // Error #2102 Timeline array doesn't contain any elements.

    MovieClip* movieclip = new MovieClip(type, application->getApplication());	// box movieclip to unref

	AutoUnref autounref(movieclip);

    int len = lua_objlen(L, index);
	for (int i = 1; i <= len; ++i)
	{
        lua_rawgeti(L, index, i);

        if (lua_type(L, -1) != LUA_TTABLE)
            luaL_error(L, GStatus(2103).errorString());     // Error #2102 Timeline element is not a table

		lua_rawgeti(L, -1, 1);
		int start = luaL_checkinteger(L, -1);
		lua_pop(L, 1);

		lua_rawgeti(L, -1, 2);
		int end = luaL_checkinteger(L, -1);
		lua_pop(L, 1);

		lua_rawgeti(L, -1, 3);
		GSprite* sprite = static_cast<GSprite*>(binder.getInstance("Sprite", -1));
		lua_pop(L, 1);

		std::vector<Parameter> parameters;

		lua_rawgeti(L, -1, 4);
		if (lua_istable(L, -1))
		{
			int t = abs_index(L, -1);

			/* table is in the stack at index 't' */
			lua_pushnil(L);  /* first key */
			while (lua_next(L, t) != 0) {
				/* uses 'key' (at index -2) and 'value' (at index -1) */
#if 0
				printf("%s - %s\n",
					lua_typename(L, lua_type(L, -2)),
					lua_typename(L, lua_type(L, -1)));
#endif
				
				const char* param = luaL_checkstring(L, -2);
				TweenType tweenType = eEaseLinear;
				
				lua_Number start, end;
				if (lua_istable(L, -1))
				{
					lua_rawgeti(L, -1, 1);
					start = luaL_checknumber(L, -1);
					lua_pop(L, 1);

					lua_rawgeti(L, -1, 2);
					end = luaL_checknumber(L, -1);
					lua_pop(L, 1);

					lua_rawgeti(L, -1, 3);
					if (!lua_isnil(L, -1))
						tweenType = (TweenType)StringId::instance().id(luaL_checkstring(L, -1));
					lua_pop(L, 1);
				}
				else
				{
					start = end = luaL_checkinteger(L, -1);
				}

				parameters.push_back(Parameter(param, start, end, tweenType));

				/* removes 'value'; keeps 'key' for next iteration */
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		movieclip->addFrame(start, end, sprite, parameters);

		lua_pop(L, 1);
	}

	autounref.release();

	movieclip->finalize();

	binder.pushInstance("MovieClip", movieclip);

	return 1;
}

int MovieClipBinder::destruct(lua_State* L)
{
	void* ptr = *(void**)lua_touserdata(L, 1);
	MovieClip* movieclip = static_cast<MovieClip*>(ptr);
	movieclip->unref();

	return 0;
}

int MovieClipBinder::play(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::play", 0);

	Binder binder(L);
	MovieClip* movieclip = static_cast<MovieClip*>(binder.getInstance("MovieClip", 1));	

	movieclip->play();

	return 0;
}

int MovieClipBinder::stop(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::stop", 0);

	Binder binder(L);
	MovieClip* movieclip = static_cast<MovieClip*>(binder.getInstance("MovieClip", 1));	

	movieclip->stop();

	return 0;
}

int MovieClipBinder::gotoAndPlay(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::gotoAndPlay", 0);

	Binder binder(L);
	MovieClip* movieclip = static_cast<MovieClip*>(binder.getInstance("MovieClip", 1));	

	int frame = luaL_checkinteger(L, 2);
	movieclip->gotoAndPlay(frame);

	return 0;
}

int MovieClipBinder::gotoAndStop(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::gotoAndStop", 0);

	Binder binder(L);
	MovieClip* movieclip = static_cast<MovieClip*>(binder.getInstance("MovieClip", 1));	

	int frame = luaL_checkinteger(L, 2);
	movieclip->gotoAndStop(frame);

	return 0;
}

int MovieClipBinder::setStopAction(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::setStopAction", 0);

	Binder binder(L);
	MovieClip* movieclip = static_cast<MovieClip*>(binder.getInstance("MovieClip", 1));

	int frame = luaL_checkinteger(L, 2);

	movieclip->setStopAction(frame);

	return 0;
}

int MovieClipBinder::setGotoAction(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::setGotoAction", 0);

	Binder binder(L);
	MovieClip* movieclip = static_cast<MovieClip*>(binder.getInstance("MovieClip", 1));

	int frame = luaL_checkinteger(L, 2);
	int destframe = luaL_checkinteger(L, 3);

	movieclip->setGotoAction(frame, destframe);

	return 0;
}

int MovieClipBinder::clearAction(lua_State* L)
{
	StackChecker checker(L, "MovieClipBinder::clearAction", 0);

	Binder binder(L);
	MovieClip* movieclip = static_cast<MovieClip*>(binder.getInstance("MovieClip", 1));

	int frame = luaL_checkinteger(L, 2);

	movieclip->clearAction(frame);

	return 0;
}
