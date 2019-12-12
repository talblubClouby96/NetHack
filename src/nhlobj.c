/* NetHack 3.7	nhlobj.c	$NHDT-Date: 1576097301 2019/12/11 20:48:21 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.0 $ */
/*      Copyright (c) 2019 by Pasi Kallinen */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "sp_lev.h"

struct _lua_obj {
    int state; /* UNUSED */
    struct obj *obj;
};

struct _lua_obj *
l_obj_check(L, index)
lua_State *L;
int index;
{
    struct _lua_obj *lo;

    luaL_checktype(L, index, LUA_TUSERDATA);
    lo = (struct _lua_obj *)luaL_checkudata(L, index, "obj");
    if (!lo)
        nhl_error(L, "Obj error");
    return lo;
}

static int
l_obj_gc(L)
lua_State *L;
{
    struct _lua_obj *lo = l_obj_check(L, 1);

    if (lo && lo->obj) {
        /* free-floating objects are deallocated */
        if (lo->obj->where == OBJ_FREE) {
            if (Has_contents(lo->obj)) {
                struct obj *otmp;
                while ((otmp = lo->obj->cobj) != 0) {
                    obj_extract_self(otmp);
                    dealloc_obj(otmp);
                }
            }
            dealloc_obj(lo->obj);
        }
        lo->obj = NULL;
    }

    return 0;
}

static struct _lua_obj *
l_obj_push(L, otmp)
lua_State *L;
struct obj *otmp;
{
    struct _lua_obj *lo = (struct _lua_obj *)lua_newuserdata(L, sizeof(struct _lua_obj));
    luaL_getmetatable(L, "obj");
    lua_setmetatable(L, -2);

    lo->state = 0;
    lo->obj = otmp;

    return lo;
}

/* local o = obj.new("large chest");
   local cobj = o:contents(); */
static int
l_obj_getcontents(L)
lua_State *L;
{
    struct _lua_obj *lo = l_obj_check(L, 1);
    struct obj *obj = lo->obj;

    if (!obj)
        nhl_error(L, "l_obj_getcontents: no obj");
    if (!obj->cobj)
        nhl_error(L, "l_obj_getcontents: no cobj");

    (void) l_obj_push(L, obj->cobj);
    return 1;
}

/* local box = obj.new("large chest");
   box.addcontent(obj.new("rock"));
*/
static int
l_obj_add_to_container(L)
lua_State *L;
{
    struct _lua_obj *lobox = l_obj_check(L, 1);
    struct _lua_obj *lo = l_obj_check(L, 2);
    struct obj *otmp;

    if (!lo->obj || !lobox->obj)
        nhl_error(L, "l_obj_add_to_container: no obj");

    if (!Is_container(lobox->obj))
        nhl_error(L, "l_obj_add_to_container: not a container");

    otmp = add_to_container(lobox->obj, lo->obj);

    /* was lo->obj merged? FIXME: causes problems if both lo->obj and
       the one it merged with are handled by lua. Use lo->state? */
    if (otmp != lo->obj)
        lo->obj = otmp;

    return 0;
}

/* Create a lua table representation of the object, unpacking all the
   object fields.
   local o = obj.new("rock");
   local otbl = o:totable(); */
static int
l_obj_to_table(L)
lua_State *L;
{
    struct _lua_obj *lo = l_obj_check(L, 1);
    struct obj *obj = lo->obj;

    lua_newtable(L);

    if (!obj) {
        nhl_add_table_entry_int(L, "NO_OBJ", 1);
        return 1;
    }

    nhl_add_table_entry_int(L, "has_contents", Has_contents(obj));
    nhl_add_table_entry_int(L, "is_container", Is_container(obj));
    nhl_add_table_entry_int(L, "o_id", obj->o_id);
    nhl_add_table_entry_int(L, "ox", obj->ox);
    nhl_add_table_entry_int(L, "oy", obj->oy);
    nhl_add_table_entry_int(L, "otyp", obj->otyp);
    if (OBJ_NAME(objects[obj->otyp]))
        nhl_add_table_entry_str(L, "otyp_name", OBJ_NAME(objects[obj->otyp]));
    if (OBJ_DESCR(objects[obj->otyp]))
        nhl_add_table_entry_str(L, "otyp_descr",
                                OBJ_DESCR(objects[obj->otyp]));
    nhl_add_table_entry_int(L, "owt", obj->owt);
    nhl_add_table_entry_int(L, "quan", obj->quan);
    nhl_add_table_entry_int(L, "spe", obj->spe);

    if (obj->otyp == STATUE) {
        nhl_add_table_entry_int(L, "historic", (obj->spe & STATUE_HISTORIC));
        nhl_add_table_entry_int(L, "statue_male", (obj->spe & STATUE_MALE));
        nhl_add_table_entry_int(L, "statue_female", (obj->spe & STATUE_FEMALE));
    }

    nhl_add_table_entry_char(L, "oclass",
                             def_oc_syms[(uchar) obj->oclass].sym);
    nhl_add_table_entry_char(L, "invlet", obj->invlet);
    /* TODO: nhl_add_table_entry_char(L, "oartifact", obj->oartifact);*/
    nhl_add_table_entry_int(L, "where", obj->where);
    /* TODO: nhl_add_table_entry_int(L, "timed", obj->timed); */
    nhl_add_table_entry_int(L, "cursed", obj->cursed);
    nhl_add_table_entry_int(L, "blessed", obj->blessed);
    nhl_add_table_entry_int(L, "unpaid", obj->unpaid);
    nhl_add_table_entry_int(L, "no_charge", obj->no_charge);
    nhl_add_table_entry_int(L, "known", obj->known);
    nhl_add_table_entry_int(L, "dknown", obj->dknown);
    nhl_add_table_entry_int(L, "bknown", obj->bknown);
    nhl_add_table_entry_int(L, "rknown", obj->rknown);
    if (obj->oclass == POTION_CLASS)
        nhl_add_table_entry_int(L, "odiluted", obj->odiluted);
    else
        nhl_add_table_entry_int(L, "oeroded", obj->oeroded);
    nhl_add_table_entry_int(L, "oeroded2", obj->oeroded2);
    /* TODO: orotten, norevive */
    nhl_add_table_entry_int(L, "oerodeproof", obj->oerodeproof);
    nhl_add_table_entry_int(L, "olocked", obj->olocked);
    nhl_add_table_entry_int(L, "obroken", obj->obroken);
    if (is_poisonable(obj))
        nhl_add_table_entry_int(L, "opoisoned", obj->opoisoned);
    else
        nhl_add_table_entry_int(L, "otrapped", obj->otrapped);
    /* TODO: degraded_horn */
    nhl_add_table_entry_int(L, "recharged", obj->recharged);
    /* TODO: on_ice */
    nhl_add_table_entry_int(L, "lamplit", obj->lamplit);
    nhl_add_table_entry_int(L, "globby", obj->globby);
    nhl_add_table_entry_int(L, "greased", obj->greased);
    nhl_add_table_entry_int(L, "nomerge", obj->nomerge);
    nhl_add_table_entry_int(L, "was_thrown", obj->was_thrown);
    nhl_add_table_entry_int(L, "in_use", obj->in_use);
    nhl_add_table_entry_int(L, "bypass", obj->bypass);
    nhl_add_table_entry_int(L, "cknown", obj->cknown);
    nhl_add_table_entry_int(L, "lknown", obj->lknown);
    nhl_add_table_entry_int(L, "corpsenm", obj->corpsenm);
    if (obj->corpsenm != NON_PM
        && (obj->otyp == TIN || obj->otyp == CORPSE || obj->otyp == EGG
            || obj->otyp == FIGURINE || obj->otyp == STATUE))
        nhl_add_table_entry_str(L, "corpsenm_name", mons[obj->corpsenm].mname);
    /* TODO: leashmon, fromsink, novelidx, record_achieve_special */
    nhl_add_table_entry_int(L, "usecount", obj->usecount);
    /* TODO: spestudied */
    nhl_add_table_entry_int(L, "oeaten", obj->oeaten);
    nhl_add_table_entry_int(L, "age", obj->age);
    nhl_add_table_entry_int(L, "owornmask", obj->owornmask);
    /* TODO: more of oextra */
    nhl_add_table_entry_int(L, "has_oname", has_oname(obj));
    if (has_oname(obj))
        nhl_add_table_entry_str(L, "oname", ONAME(obj));

    return 1;
}

/* create a new object via wishing routine */
/* local o = obj.new("rock"); */
static int
l_obj_new_readobjnam(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 1) {
        char buf[BUFSZ];
        struct obj *otmp;
        Sprintf(buf, "%s", luaL_checkstring(L, 1));
        lua_pop(L, 1);
        otmp = readobjnam(buf, NULL);
        (void) l_obj_push(L, otmp);
        return 1;
    } else
        nhl_error(L, "l_obj_new_readobjname: Wrong args");
    return 0;
}

/* Get the topmost object on the map at x,y */
/* local o = obj.at(x, y); */
static int
l_obj_at(L)
lua_State *L;
{
    int argc = lua_gettop(L);

    if (argc == 2) {
        int x, y;
        x = luaL_checkinteger(L, 1);
        y = luaL_checkinteger(L, 2);
        lua_pop(L, 2);
        (void) l_obj_push(L, g.level.objects[x][y]);
        return 1;
    } else
        nhl_error(L, "l_obj_at: Wrong args");
    return 0;
}

/* Place an object on the map at (x,y).
   local o = obj.new("rock");
   o:placeobj(u.ux, u.uy); */
static int
l_obj_placeobj(L)
lua_State *L;
{
    int argc = lua_gettop(L);
    struct _lua_obj *lo = l_obj_check(L, 1);

    if (argc != 3)
        nhl_error(L, "l_obj_placeobj: Wrong args");

    if (lo && lo->obj) {
        int x, y;
        x = luaL_checkinteger(L, 2);
        y = luaL_checkinteger(L, 3);
        lua_pop(L, 3);

        place_object(lo->obj, x, y);
    } else
        nhl_error(L, "l_obj_placeobj: Wrong args");

    return 0;
}

/* Get the next object in the object chain */
/* local o = obj.at(x, y);
   local o2 = o:next();
*/
static int
l_obj_nextobj(L)
lua_State *L;
{
    struct _lua_obj *lo = l_obj_check(L, 1);

    if (lo && lo->obj)
        (void) l_obj_push(L, lo->obj->where == OBJ_FLOOR ? lo->obj->nexthere : lo->obj->nobj);
    return 1;
}

/* Get the container object is in */
/* local box = o:container(); */
static int
l_obj_container(L)
lua_State *L;
{
    struct _lua_obj *lo = l_obj_check(L, 1);

    if (lo && lo->obj && lo->obj->where == OBJ_CONTAINED) {
        (void) l_obj_push(L, lo->obj->ocontainer);
        return 1;
    }
    (void) l_obj_push(L, NULL);
    return 1;
}

/* Is the object a null? */
/* local badobj = o:isnull(); */
static int
l_obj_isnull(L)
lua_State *L;
{
    struct _lua_obj *lo = l_obj_check(L, 1);

    lua_pushboolean(L, lo && lo->obj);
    return 1;
}


static const struct luaL_Reg l_obj_methods[] = {
    { "new", l_obj_new_readobjnam },
    { "isnull", l_obj_isnull },
    { "at", l_obj_at },
    { "next", l_obj_nextobj },
    { "totable", l_obj_to_table },
    { "placeobj", l_obj_placeobj },
    { "container", l_obj_container },
    { "contents", l_obj_getcontents },
    { "addcontent", l_obj_add_to_container },
    { NULL, NULL }
};

static const luaL_Reg l_obj_meta[] = {
    { "__gc", l_obj_gc },
    { NULL, NULL }
};

int
l_obj_register(L)
lua_State *L;
{
    int lib_id, meta_id;

    /* newclass = {} */
    lua_createtable(L, 0, 0);
    lib_id = lua_gettop(L);

    /* metatable = {} */
    luaL_newmetatable(L, "obj");
    meta_id = lua_gettop(L);
    luaL_setfuncs(L, l_obj_meta, 0);

    /* metatable.__index = _methods */
    luaL_newlib(L, l_obj_methods);
    lua_setfield(L, meta_id, "__index");

    /* metatable.__metatable = _meta */
    luaL_newlib(L, l_obj_meta);
    lua_setfield(L, meta_id, "__metatable");

    /* class.__metatable = metatable */
    lua_setmetatable(L, lib_id);

    /* _G["obj"] = newclass */
    lua_setglobal(L, "obj");

    return 0;
}
