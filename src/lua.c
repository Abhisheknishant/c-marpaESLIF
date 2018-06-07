#include "marpaESLIF/internal/lua.h"
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

/* Note that a lua integer is:
 * - an int  if LUA_INT_TYPE = LUA_INT_INT
 * - a  long if LUA_INT_TYPE = LUA_INT_LONG
 * a lua number is:
 * - a float  if LUA_FLOAT_TYPE = LUA_FLOAT_FLOAT
 * - a double if LUA_FLOAT_TYPE = LUA_FLOAT_DOUBLE
 *
 * This is because we are forcing LUA_USE_C89 in our embedded lua. Then
 * remains these two possiblities for lua integer and number.
 */

#undef  FILENAMES
#define FILENAMES "lua.c" /* For logging */

/* Internal initialization */
static const char *MARPAESLIF_LUA_WRAPPER = "__marpaESLIFLuaWrapper";
static const char *MARPAESLIF_LUA_TABLE = "__marpaESLIFLuaTable";
static const char *MARPAESLIF_LUA_INIT =
  "--------------------------------------------------------\n"
  "-- Internal table used to communicate with outside world\n"
  "--------------------------------------------------------\n"
  "__marpaESLIFLuaTable = {}\n"
  "\n"
  "--------------------------------------------------------\n"
  "-- Function call wrapper\n"
  "--------------------------------------------------------\n"
  "function __marpaESLIFLuaWrapper(indice, funcname, ...)\n"
  "  local value = funcname(...)\n"
  "  __marpaESLIFLuaTable[indice + 1] = value\n"
  "  return value\n"
  "end\n";

static short _marpaESLIF_lua_newb(marpaESLIFValue_t *marpaESLIFValuep);
static short _marpaESLIF_lua_push_argb(marpaESLIFValue_t *marpaESLIFValuep, int i);
static short _marpaESLIF_lua_pop_argb(marpaESLIFValue_t *marpaESLIFValuep, int resulti);
static void  _marpaESLIF_lua_freeInternalActionv(void *userDatavp, int resulti);

#define LOG_PANIC_STRING(marpaESLIFValuep, f) do {                      \
    char *panicstring;							\
    if (luaunpanic_panicstring(&panicstring, marpaESLIFValuep->L)) {    \
      MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s panic", #f); \
    } else {								\
      MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s panic: %s", #f, panicstring); \
    }									\
  } while (0)

#define LOG_ERROR_STRING(marpaESLIFValuep, f) do {                      \
    const char *errorstring;                                            \
    if (luaunpanic_tostring(&errorstring, marpaESLIFValuep->L, -1)) {   \
      LOG_PANIC_STRING(marpaESLIFValuep, luaunpanic_tostring);          \
      MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s failure", #f); \
    } else {                                                            \
      if (errorstring == NULL) {                                        \
        MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s failure", #f); \
      } else {								\
        MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s failure: %s", #f, errorstring); \
      }									\
    }                                                                   \
  } while (0)

#define LUAL_CHECKVERSION(marpaESLIFValuep) do {                        \
    if (luaunpanicL_checkversion(marpaESLIFValuep->L)) {                \
      LOG_PANIC_STRING(marpaESLIFValuep, luaL_checkversion);            \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUAL_OPENLIBS(marpaESLIFValuep) do {                           \
    if (luaunpanicL_openlibs(marpaESLIFValuep->L)) {                   \
      LOG_PANIC_STRING(marpaESLIFValuep, luaL_openlibs);               \
      errno = ENOSYS;                                                  \
      goto err;                                                        \
    }                                                                  \
  } while (0)

#define LUA_PUSHNIL(marpaESLIFValuep) do {                             \
    if (luaunpanic_pushnil(marpaESLIFValuep->L)) {                     \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pushnil);                 \
      errno = ENOSYS;                                                  \
      goto err;                                                        \
    }                                                                  \
  } while (0)

#define LUA_PUSHLSTRING(marpaESLIFValuep, s, l) do {                    \
    if (luaunpanic_pushlstring(NULL, marpaESLIFValuep->L, s, l)) {      \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pushlstring);              \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUAL_DOSTRING(marpaESLIFValuep, string) do {                    \
    int rc;                                                             \
    if (luaunpanicL_dostring(&rc, marpaESLIFValuep->L, string)) {       \
      LOG_PANIC_STRING(marpaESLIFValuep, luaL_dostring);                \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
    if (rc) {                                                           \
      LOG_ERROR_STRING(marpaESLIFValuep, luaL_dostring);                \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_PUSHLIGHTUSERDATA(marpaESLIFValuep, p) do {                \
    if (luaunpanic_pushlightuserdata(marpaESLIFValuep->L, p)) {        \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pushlightuserdata);       \
      errno = ENOSYS;                                                  \
      goto err;                                                        \
    }                                                                  \
  } while (0)

#define LUA_NEWTABLE(marpaESLIFValuep) do {                            \
    if (luaunpanic_newtable(marpaESLIFValuep->L)) {                    \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_newtable);                \
      errno = ENOSYS;                                                  \
      goto err;                                                        \
    }                                                                  \
  } while (0)

#define LUA_PUSHINTEGER(marpaESLIFValuep, i) do {                      \
    if (luaunpanic_pushinteger(marpaESLIFValuep->L, i)) {              \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pushinteger);             \
      errno = ENOSYS;                                                  \
      goto err;                                                        \
    }                                                                  \
  } while (0)

#define LUA_PUSHNUMBER(marpaESLIFValuep, x) do {                       \
    if (luaunpanic_pushnumber(marpaESLIFValuep->L, x)) {               \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pushnumber);              \
      errno = ENOSYS;                                                  \
      goto err;                                                        \
    }                                                                  \
  } while (0)

#define LUA_PUSHBOOLEAN(marpaESLIFValuep, b) do {                      \
    if (luaunpanic_pushboolean(marpaESLIFValuep->L, b)) {              \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pushboolean);             \
      errno = ENOSYS;                                                  \
      goto err;                                                        \
    }                                                                  \
  } while (0)

#define LUA_RAWGETI(rcp, marpaESLIFValuep, idx, n) do {                 \
    if (luaunpanic_rawgeti(rcp, marpaESLIFValuep->L, idx, n)) {         \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_rawgeti);                  \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_RAWSETI(marpaESLIFValuep, idx, n) do {                      \
    if (luaunpanic_rawseti(marpaESLIFValuep->L, idx, n)) {              \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_rawseti);                  \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_REMOVE(marpaESLIFValuep, idx) do {                          \
    if (luaunpanic_remove(marpaESLIFValuep->L, idx)) {                  \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_remove);                   \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_GETGLOBAL(rcp, marpaESLIFValuep, name) do {                 \
    if (luaunpanic_getglobal(rcp, marpaESLIFValuep->L, name)) {         \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_getglobal);                \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_SETGLOBAL(marpaESLIFValuep, name) do {                      \
    if (luaunpanic_setglobal(marpaESLIFValuep->L, name)) {              \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_setglobal);                \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUAL_LOADBUFFER(marpaESLIFValuep, s, sz, n) do {                \
    int _rci = -1;                                                      \
    if (luaunpanicL_loadbuffer(&_rci, marpaESLIFValuep->L, s, sz, n)) { \
      LOG_PANIC_STRING(marpaESLIFValuep, luaL_loadbuffer);              \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
    if (_rci != 0) {                                                    \
      LOG_ERROR_STRING(marpaESLIFValuep, luaL_loadbuffer);              \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_CALL(marpaESLIFValuep, n, r) do {                           \
    if (luaunpanic_call(marpaESLIFValuep->L, n, r)) {                   \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_call);                     \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_SETTOP(marpaESLIFValuep, idx) do {                       \
    if (luaunpanic_settop(marpaESLIFValuep->L, idx)) {               \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_SETTOP);                \
      errno = ENOSYS;                                                \
      goto err;                                                      \
    }                                                                \
  } while (0)

#define LUA_TYPE(marpaESLIFValuep, rcp, idx) do {                    \
    if (luaunpanic_type(rcp, marpaESLIFValuep->L, idx)) {            \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_type);                  \
      errno = ENOSYS;                                                \
      goto err;                                                      \
    }                                                                \
  } while (0)

#define LUA_TOBOOLEAN(marpaESLIFValuep, rcp, idx) do {               \
    if (luaunpanic_toboolean(rcp, marpaESLIFValuep->L, idx)) {       \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_toboolean);             \
      errno = ENOSYS;                                                \
      goto err;                                                      \
    }                                                                \
  } while (0)

#define LUA_TONUMBER(marpaESLIFValuep, rcp, idx) do {                   \
    int isnum;                                                          \
    if (luaunpanic_tonumberx(rcp, marpaESLIFValuep->L, idx, &isnum)) {  \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_tonumberx);                \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
    if (! isnum) {                                                      \
      MARPAESLIF_ERROR(marpaESLIFValuep->marpaESLIFp, "lua_tonumberx failure"); \
    }                                                                   \
  } while (0)

#define LUA_TOLSTRING(marpaESLIFValuep, rcpp, idx, lenp) do {            \
    if (luaunpanic_tolstring(rcpp, marpaESLIFValuep->L, idx, lenp)) {   \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_tolstring);                \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)

#define LUA_TOPOINTER(marpaESLIFValuep, rcpp, idx) do {              \
    if (luaunpanic_topointer(rcpp, marpaESLIFValuep->L, idx)) {      \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_topointer);             \
      errno = ENOSYS;                                                \
      goto err;                                                      \
    }                                                                \
  } while (0)

#define LUA_TOUSERDATA(marpaESLIFValuep, rcpp, idx) do {              \
    if (luaunpanic_touserdata(rcpp, marpaESLIFValuep->L, idx)) {      \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_touserdata);             \
      errno = ENOSYS;                                                 \
      goto err;                                                       \
    }                                                                 \
  } while (0)

#define LUA_POP(marpaESLIFValuep, n) do {                       \
    if (luaunpanic_pop(marpaESLIFValuep->L, n)) {               \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pop);              \
      errno = ENOSYS;                                           \
      goto err;                                                 \
    }                                                           \
  } while (0)

#define LUA_PCALL(marpaESLIFValuep, n, r, f) do {                       \
    int _rci;                                                           \
    if (luaunpanic_pcall(&_rci, marpaESLIFValuep->L, n, r, f)) {        \
      LOG_PANIC_STRING(marpaESLIFValuep, lua_pcall);                    \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
    if (_rci != 0) {                                                    \
      LOG_ERROR_STRING(marpaESLIFValuep, lua_pcall);                    \
      errno = ENOSYS;                                                   \
      goto err;                                                         \
    }                                                                   \
  } while (0)


/*****************************************************************************/
static short _marpaESLIF_lua_newb(marpaESLIFValue_t *marpaESLIFValuep)
/*****************************************************************************/
{
  marpaESLIFGrammar_t *marpaESLIFGrammarp;
  short                rcb;

  if (marpaESLIFValuep->L != NULL) {
    /* Already done */
    rcb = 1;
    goto done;
  }

  marpaESLIFGrammarp = marpaESLIFValuep->marpaESLIFRecognizerp->marpaESLIFGrammarp;

  /* Create Lua state */
  if (luaunpanicL_newstate(&(marpaESLIFValuep->L))) {
    MARPAESLIF_ERROR(marpaESLIFValuep->marpaESLIFp, "luaunpanicL_newstate failure");
    errno = ENOSYS;
    goto err;
  }
  if (marpaESLIFValuep->L == NULL) {
    MARPAESLIF_ERROR(marpaESLIFValuep->marpaESLIFp, "luaunpanicL_success but lua_State is NULL");
    errno = ENOSYS;
    goto err;
  }

  /* Open all available libraries */
  LUAL_OPENLIBS(marpaESLIFValuep);

  /* Check Lua version */
  LUAL_CHECKVERSION(marpaESLIFValuep);

  /* We execute our own code */
  LUAL_DOSTRING(marpaESLIFValuep, MARPAESLIF_LUA_INIT);
  
  /* Execute lua script present in the grammar, eventually */
  if ((marpaESLIFGrammarp->luabytep != NULL) && (marpaESLIFGrammarp->luabytel > 0)) {
    LUAL_LOADBUFFER(marpaESLIFValuep, marpaESLIFGrammarp->luabytep, marpaESLIFGrammarp->luabytel, "=(luascript)");
    LUA_PCALL(marpaESLIFValuep, 0, LUA_MULTRET, 0);
    /* Clear the stack */
    LUA_SETTOP(marpaESLIFValuep, 0);
  }
  
  rcb = 1;
  goto done;

 err:
  if (marpaESLIFValuep->L != NULL) {
    if (luaunpanic_close(marpaESLIFValuep->L)) {
      LOG_PANIC_STRING(marpaESLIFValuep, lua_close);
    }
    marpaESLIFValuep->L = NULL;
  }
  rcb = 0;

 done:
  return rcb;
}

/*****************************************************************************/
static void _marpaESLIF_lua_freev(marpaESLIFValue_t *marpaESLIFValuep)
/*****************************************************************************/
{
  if (marpaESLIFValuep->L != NULL) {
    if (luaunpanic_close(marpaESLIFValuep->L)) {
      LOG_PANIC_STRING(marpaESLIFValuep, luaunpanic_close);
    }
    marpaESLIFValuep->L = NULL;
  }
}

/*****************************************************************************/
static short _marpaESLIFValue_lua_actionb(void *userDatavp, marpaESLIFValue_t *marpaESLIFValuep, int arg0i, int argni, int resulti, short nullableb)
/*****************************************************************************/
{
  static const char      *funcs                 = "_marpaESLIFValue_lua_actionb";
  marpaESLIFRecognizer_t *marpaESLIFRecognizerp = marpaESLIFValuep->marpaESLIFRecognizerp;
  int                     i;
  short                   rcb;
  int                     nargi;
  int                     typei;

  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_INC;
  MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "%s(userDatavp=%p, arg0i=%d, argni=%d, resulti=%d, nullableb=%d)", funcs, userDatavp, arg0i, argni, resulti, (int) nullableb);

  /* Create the lua state if needed */
  if (! _marpaESLIF_lua_newb(marpaESLIFValuep)) {
    goto err;
  }

  /* We going through our function wrapper __marpaESLIFLuaWrapper(indice, funcname, ...) */
  /* 1: __marpaESLIFLuaWrapper */
  LUA_GETGLOBAL(&typei, marpaESLIFValuep, MARPAESLIF_LUA_WRAPPER);
  if (typei != LUA_TFUNCTION) {
    MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s is not a function", MARPAESLIF_LUA_WRAPPER);
    goto err;
  }
  /* 2: indice */
  LUA_PUSHNUMBER(marpaESLIFValuep, (lua_Number) resulti);
  /* 3: user action */
  LUA_GETGLOBAL(&typei, marpaESLIFValuep, marpaESLIFValuep->actions);
  if (typei != LUA_TFUNCTION) {
    MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "Lua action %s is not a function", marpaESLIFValuep->actions);
    goto err;
  }
  /* 4...: user context */
  LUA_PUSHLIGHTUSERDATA(marpaESLIFValuep, userDatavp);
  /* 5...: arguments */
  if (! nullableb) {
    nargi = argni - arg0i + 1;
    for (i = arg0i; i <= argni; i++) {
      if (! _marpaESLIF_lua_push_argb(marpaESLIFValuep, i)) {
        goto err;
      }
    }
  } else {
    nargi = 0;
  }

  /* Lua will make sure there is a room for at least one argument on the stack at return */
  LUA_CALL(marpaESLIFValuep, nargi + 3 /* +3 is for (indice, function, userDatavp) */, 1);

  if (! _marpaESLIF_lua_pop_argb(marpaESLIFValuep, resulti)) {
    goto err;
  }

  /* Clear the stack */
  LUA_SETTOP(marpaESLIFValuep, 0);
  
  rcb = 1;
  goto done;

 err:
  rcb = 0;

 done:
  MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "return %d", (int) rcb);
  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_DEC;
  return rcb;
}

/*****************************************************************************/
static short _marpaESLIFValue_lua_symbolb(void *userDatavp, marpaESLIFValue_t *marpaESLIFValuep, char *bytep, size_t bytel, int resulti)
/*****************************************************************************/
{
  static const char      *funcs                 = "_marpaESLIFValue_lua_symbolb";
  marpaESLIFRecognizer_t *marpaESLIFRecognizerp = marpaESLIFValuep->marpaESLIFRecognizerp;
  short                   rcb;
  int                     typei;

  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_INC;
  MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "%s(userDatavp=%p, bytep=%p, bytel=%ld)", funcs, userDatavp, bytep, (unsigned long) bytel);

  /* Create the lua state if needed */
  if (! _marpaESLIF_lua_newb(marpaESLIFValuep)) {
    goto err;
  }

  /* We going through our function wrapper __marpaESLIFLuaWrapper(indice, funcname, ...) */
  /* 1: __marpaESLIFLuaWrapper */
  LUA_GETGLOBAL(&typei, marpaESLIFValuep, MARPAESLIF_LUA_WRAPPER);
  if (typei != LUA_TFUNCTION) {
    MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s is not a function", MARPAESLIF_LUA_WRAPPER);
    goto err;
  }
  /* 2: indice */
  LUA_PUSHNUMBER(marpaESLIFValuep, (lua_Number) resulti);
  /* 3: user action */
  LUA_GETGLOBAL(&typei, marpaESLIFValuep, marpaESLIFValuep->actions);
  if (typei != LUA_TFUNCTION) {
    MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "Lua action %s is not a function", marpaESLIFValuep->actions);
    goto err;
  }
  /* 4: context */
  LUA_PUSHLIGHTUSERDATA(marpaESLIFValuep, userDatavp);
  /* 5: lexeme */
  LUA_PUSHLSTRING(marpaESLIFValuep, bytep, bytel);

  /* Lua will make sure there is a room for at least one argument on the stack at return */
  LUA_CALL(marpaESLIFValuep, 4 /* (indice, funcname, context, lexeme) */, 1);

  if (! _marpaESLIF_lua_pop_argb(marpaESLIFValuep, resulti)) {
    goto err;
  }

  /* Clear the stack */
  LUA_SETTOP(marpaESLIFValuep, 0);
  
  rcb = 1;
  goto done;

 err:
  rcb = 0;

 done:
  MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "return %d", (int) rcb);
  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_DEC;
  return rcb;
}

/*****************************************************************************/
static short _marpaESLIF_lua_push_argb(marpaESLIFValue_t *marpaESLIFValuep, int i)
/*****************************************************************************/
{
  static const char      *funcs = "_marpaESLIF_lua_push_argb";
  short                   rcb;
  marpaESLIFValueResult_t marpaESLIFValueResult;
  int                     typei;
  int                    *indiceip;

  if (! _marpaESLIFValue_stack_getb(marpaESLIFValuep, i, &marpaESLIFValueResult)) {
    goto err;
  }

  /* Special values that comes from lua and have a meaning only for lua have the */
  /* context MARPAESLIFVALUE_LUA_CONTEXT. Then per def, this is a PTR that hosts */
  /* a malloced integer. This integer is the indice in an internal lua table. */
  if (marpaESLIFValueResult.contexti == MARPAESLIFVALUE_LUA_CONTEXT) {
    LUA_GETGLOBAL(&typei, marpaESLIFValuep, MARPAESLIF_LUA_TABLE);                                    /* stack: ..., table */
    if (typei != LUA_TTABLE) {
      MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s is not a table", MARPAESLIF_LUA_TABLE);
      goto err;
    }
    indiceip = (int *) marpaESLIFValueResult.u.p;
    MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing %s[++%d]", funcs, MARPAESLIF_LUA_TABLE, *indiceip);
    LUA_RAWGETI(NULL, marpaESLIFValuep, -1, (lua_Integer) (*indiceip + 1));                           /* stack: ..., table, table[*indiceip] */
    LUA_REMOVE(marpaESLIFValuep, -2);                                                                 /* stack: ..., table[*indiceip] */
  } else {
    switch (marpaESLIFValueResult.type) {
      /* This is coming from a world outside of Lua - we do a translation */
    case MARPAESLIF_VALUE_TYPE_UNDEF:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing UNDEF", funcs);
      LUA_PUSHNIL(marpaESLIFValuep);
      break;
    case MARPAESLIF_VALUE_TYPE_CHAR:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing CHAR", funcs);
      LUA_PUSHLSTRING(marpaESLIFValuep, &marpaESLIFValueResult.u.c, 1);
      break;
    case MARPAESLIF_VALUE_TYPE_SHORT:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing SHORT", funcs);
      LUA_PUSHINTEGER(marpaESLIFValuep, (lua_Integer) marpaESLIFValueResult.u.b);
      break;
    case MARPAESLIF_VALUE_TYPE_INT:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing INT", funcs);
      LUA_PUSHINTEGER(marpaESLIFValuep, (lua_Integer) marpaESLIFValueResult.u.i);
      break;
    case MARPAESLIF_VALUE_TYPE_LONG:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing LONG", funcs);
      LUA_PUSHINTEGER(marpaESLIFValuep, (lua_Integer) marpaESLIFValueResult.u.l);
      break;
    case MARPAESLIF_VALUE_TYPE_FLOAT:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing FLOAT", funcs);
      LUA_PUSHNUMBER(marpaESLIFValuep, (lua_Number) marpaESLIFValueResult.u.f);
      break;
    case MARPAESLIF_VALUE_TYPE_DOUBLE:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing DOUBLE", funcs);
      LUA_PUSHNUMBER(marpaESLIFValuep, (lua_Number) marpaESLIFValueResult.u.d);
      break;
    case MARPAESLIF_VALUE_TYPE_PTR:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing PTR", funcs);
      LUA_PUSHLIGHTUSERDATA(marpaESLIFValuep, marpaESLIFValueResult.u.p);
      break;
    case MARPAESLIF_VALUE_TYPE_ARRAY:
      MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Pushing ARRAY", funcs);
      LUA_PUSHLSTRING(marpaESLIFValuep, marpaESLIFValueResult.u.p, marpaESLIFValueResult.sizel);
      break;
    default:
      MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "Unsupported value type %d", marpaESLIFValueResult.type);
      goto err;
    }
  }

  rcb = 1;
  goto done;

 err:
  rcb = 0;

 done:
  return rcb;
}

/*****************************************************************************/
static short _marpaESLIF_lua_pop_argb(marpaESLIFValue_t *marpaESLIFValuep, int resulti)
/*****************************************************************************/
{
  static const char       *funcs    = "_marpaESLIF_lua_pop_argb";
  int                     *resultip = NULL;
  int                      typei;
  marpaESLIFValueResult_t  marpaESLIFValueResult;
  short                    rcb;
  const char              *bytep;
  size_t                   bytel;

  LUA_TYPE(marpaESLIFValuep, &typei, -1);

  switch (typei) {
  case LUA_TNIL:
    marpaESLIFValueResult.contexti        = 0;
    marpaESLIFValueResult.sizel           = 0;
    marpaESLIFValueResult.representationp = NULL;
    marpaESLIFValueResult.shallowb        = 0;
    marpaESLIFValueResult.luab            = 1;
    marpaESLIFValueResult.userDatavp      = NULL;
    marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_UNDEF;
    /* No need to keep that in our internal lua table */
    _marpaESLIF_lua_freeInternalActionv(marpaESLIFValuep /* userDatavp */, resulti);
  break;
  case LUA_TNUMBER:
    /* A number is our forced lua implementation is always a double */
    marpaESLIFValueResult.contexti        = 0;
    marpaESLIFValueResult.sizel           = 0;
    marpaESLIFValueResult.representationp = NULL;
    marpaESLIFValueResult.shallowb        = 0;
    marpaESLIFValueResult.luab            = 1;
    marpaESLIFValueResult.userDatavp      = NULL;
#if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT
    marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_FLOAT;
    LUA_TONUMBER(marpaESLIFValuep, &(marpaESLIFValueResult.u.f), -1);
#else
    marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_DOUBLE;
    LUA_TONUMBER(marpaESLIFValuep, &(marpaESLIFValueResult.u.d), -1);
#endif
    /* No need to keep that in our internal lua table */
    _marpaESLIF_lua_freeInternalActionv(marpaESLIFValuep /* userDatavp */, resulti);
    break;
  case LUA_TBOOLEAN:
    /* A boolean in lua maps to an int */
    marpaESLIFValueResult.contexti        = 0;
    marpaESLIFValueResult.sizel           = 0;
    marpaESLIFValueResult.representationp = NULL;
    marpaESLIFValueResult.shallowb        = 0;
    marpaESLIFValueResult.luab            = 1;
    marpaESLIFValueResult.userDatavp      = NULL;
    marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_INT;
    LUA_TOBOOLEAN(marpaESLIFValuep, &(marpaESLIFValueResult.u.i), -1);
    /* No need to keep that in our internal lua table */
    _marpaESLIF_lua_freeInternalActionv(marpaESLIFValuep /* userDatavp */, resulti);
    break;
  case LUA_TSTRING:
    /* A lua string is a sequence of 8-bits chars - so it can contain anything, and may not represent a valid string whatever the encoding */
    /* This first well with our ARRAY notion */
    LUA_TOLSTRING(marpaESLIFValuep, &bytep, -1, &bytel);
    if ((bytep == NULL) || (bytel <= 0)) {
      /* In reality this is a null string */
      marpaESLIFValueResult.contexti        = 0;
      marpaESLIFValueResult.sizel           = 0;
      marpaESLIFValueResult.representationp = NULL;
      marpaESLIFValueResult.shallowb        = 0;
      marpaESLIFValueResult.luab            = 1;
      marpaESLIFValueResult.userDatavp      = NULL;
      marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_UNDEF;
    } else {
      marpaESLIFValueResult.u.p = (char *) malloc(bytel);
      if (marpaESLIFValueResult.u.p == NULL) {
        MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "malloc failure, %s", strerror(errno));
        goto err;
      }
      marpaESLIFValueResult.contexti        = 0;
      marpaESLIFValueResult.sizel           = bytel;
      marpaESLIFValueResult.representationp = NULL;
      marpaESLIFValueResult.shallowb        = 0;
      marpaESLIFValueResult.luab            = 1;
      marpaESLIFValueResult.userDatavp      = NULL;
      marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_ARRAY;
      memcpy(marpaESLIFValueResult.u.p, bytep, bytel);
    }
    /* No need to keep that in our internal lua table */
    _marpaESLIF_lua_freeInternalActionv(marpaESLIFValuep /* userDatavp */, resulti);
    break;
    /* The following four cases are meaningul to lua only, and cannot be exported outside */
    /* We store the indice in our internal lua table - in order to trigger the free we */
    /* must allocate something - we allocate an int and store resulti in it. */
  case LUA_TTABLE:
  case LUA_TFUNCTION:
  case LUA_TUSERDATA:
  case LUA_TTHREAD:
    resultip = (int *) malloc(sizeof(int));
    if (resultip == NULL) {
      MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "malloc failure, %s", strerror(errno));
      goto err;
    }
    *resultip = resulti;
    marpaESLIFValueResult.contexti        = MARPAESLIFVALUE_LUA_CONTEXT;
    marpaESLIFValueResult.sizel           = sizeof(int);
    marpaESLIFValueResult.representationp = NULL;
    marpaESLIFValueResult.shallowb        = 0;
    marpaESLIFValueResult.luab            = 1;
    marpaESLIFValueResult.userDatavp      = (void *) marpaESLIFValuep;
    marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_PTR;
    marpaESLIFValueResult.u.p             = (void *) resultip;
    MARPAESLIF_NOTICEF(marpaESLIFValuep->marpaESLIFp, "%s: Keeping lua type %d in internal table at indice %d", funcs, typei, resulti);
    break;
  case LUA_TLIGHTUSERDATA:
    /* External pointer */
    marpaESLIFValueResult.contexti        = 0;
    marpaESLIFValueResult.sizel           = 0;
    marpaESLIFValueResult.representationp = NULL;
    marpaESLIFValueResult.shallowb        = 0;
    marpaESLIFValueResult.luab            = 1;
    marpaESLIFValueResult.userDatavp      = NULL;
    marpaESLIFValueResult.type            = MARPAESLIF_VALUE_TYPE_PTR;
    LUA_TOUSERDATA(marpaESLIFValuep, &(marpaESLIFValueResult.u.p), -1);
    /* No need to keep that in our internal lua table */
    _marpaESLIF_lua_freeInternalActionv(marpaESLIFValuep /* userDatavp */, resulti);
    break;
  default:
    MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "Unsupported lua type %d", typei);
    goto err;
  }

  if (! _marpaESLIFValue_stack_setb(marpaESLIFValuep, resulti, &marpaESLIFValueResult)) {
    goto err;
  }

  rcb = 1;
  goto done;

 err:
  if (resultip != NULL) {
    free(resultip);
  }
  rcb = 0;

 done:
  return rcb;
}

/*****************************************************************************/
static void _marpaESLIF_lua_freeDefaultActionv(void *userDatavp, int contexti, void *p, size_t sizel)
/*****************************************************************************/
{
  static const char       *funcs                 = "_marpaESLIF_lua_freeDefaultActionv";
  marpaESLIFValue_t       *marpaESLIFValuep      = (marpaESLIFValue_t *) userDatavp;
  marpaESLIFRecognizer_t  *marpaESLIFRecognizerp = marpaESLIFValuep->marpaESLIFRecognizerp;
  int                      typei;
  int                     *indiceip;

  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_INC;
  MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "%s(contexti=%d, p=%p, sizel=%ld)", funcs, contexti, p, (unsigned long) sizel);

  /* When called with no pointer, this is an internal call done by the pop of values */
  if ((p != NULL) && (sizel > 0)) {
    indiceip = (int *) p;

    /* The value is always in  MARPAESLIF_LUA_TABLE */
    LUA_GETGLOBAL(&typei, marpaESLIFValuep, MARPAESLIF_LUA_TABLE);                                      /* stack: table */
    if (typei != LUA_TTABLE) {
      MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s is not a table", MARPAESLIF_LUA_TABLE);
    } else {
      MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "%s: Setting __marpaESLIFLuaTable[++%d] = nil", funcs, *indiceip);
      LUA_PUSHNIL(marpaESLIFValuep);                                                                    /* stack: table, nil */
      LUA_RAWSETI(marpaESLIFValuep, -2, (lua_Integer) (*indiceip + 1));                                 /* stack: table */
    }

    /* Remove table from the stack */
    LUA_POP(marpaESLIFValuep, -1);
  }

  /* This function returns void, though we need the err label */
 err:
  MARPAESLIF_NOTICE(marpaESLIFRecognizerp->marpaESLIFp, "return");
  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_DEC;
}

/*****************************************************************************/
static void _marpaESLIF_lua_freeInternalActionv(void *userDatavp, int resulti)
/*****************************************************************************/
{
  static const char       *funcs                 = "_marpaESLIF_lua_freeInternalActionv";
  marpaESLIFValue_t       *marpaESLIFValuep      = (marpaESLIFValue_t *) userDatavp;
  marpaESLIFRecognizer_t  *marpaESLIFRecognizerp = marpaESLIFValuep->marpaESLIFRecognizerp;
  int                      typei;

  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_INC;
  MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "%s(resulti=%d)", funcs, resulti);

  /* The value is always in  MARPAESLIF_LUA_TABLE */
  LUA_GETGLOBAL(&typei, marpaESLIFValuep, MARPAESLIF_LUA_TABLE);                                    /* stack: table */
  if (typei != LUA_TTABLE) {
    MARPAESLIF_ERRORF(marpaESLIFValuep->marpaESLIFp, "%s is not a table", MARPAESLIF_LUA_TABLE);
  } else {
    MARPAESLIF_NOTICEF(marpaESLIFRecognizerp->marpaESLIFp, "%s: Setting __marpaESLIFLuaTable[++%d] = nil", funcs, resulti);
    LUA_PUSHNIL(marpaESLIFValuep);                                                                  /* stack: table, nil */
    LUA_RAWSETI(marpaESLIFValuep, -2, (lua_Integer) (resulti + 1));                                 /* stack: table */
  }

  /* Remove table from the stack */
  LUA_POP(marpaESLIFValuep, -1);

  /* This function returns void, though we need the err label */
 err:
  MARPAESLIF_NOTICE(marpaESLIFRecognizerp->marpaESLIFp, "return");
  MARPAESLIFRECOGNIZER_CALLSTACKCOUNTER_DEC;
}
