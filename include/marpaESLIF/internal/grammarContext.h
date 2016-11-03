#ifndef MARPAESLIF_INTERNAL_GRAMMARCONTEXT_H
#define MARPAESLIF_INTERNAL_GRAMMARCONTEXT_H

/* Management of grammarContext is veyr generic, almost all code is generated with macros */

typedef struct  marpaESLIF_grammarContext  marpaESLIF_grammarContext_t;
typedef enum    marpaESLIF_grammarItemType marpaESLIF_grammarItemType_t;
typedef struct  marpaESLIF_adverbItem      marpaESLIF_adverbItem_t;

/* Internal structure to have value context information */
/* This is used in the grammar generation context */
/* We maintain in parallel thress stacks:
   - the outputStack as per Marpa,
   - a description of what is at every indice of this outputStack
   - grammars
/* Grammar themselves are in grammarStackp */
struct marpaESLIF_grammarContext {
  genericStack_t              outputStack; /* This stack is temporary: GENERICSTACK_INIT() */
  genericStack_t             *outputStackp;
  genericStack_t              itemTypeStack; /* This stack is temporary: GENERICSTACK_INIT() */
  genericStack_t             *itemTypeStackp;
  genericStack_t             *grammarStackp; /* This stack will have to survive if success: GENERICSTACK_NEW() */
  marpaESLIF_grammar_t       *current_grammarp;
};

enum marpaESLIF_grammarItemType {
  MARPAESLIF_GRAMMARITEMTYPE_NA = 0,
  MARPAESLIF_GRAMMARITEMTYPE_LEXEME,
  MARPAESLIF_GRAMMARITEMTYPE_OP_DECLARE,
  MARPAESLIF_GRAMMARITEMTYPE_ACTION_NAME,
  MARPAESLIF_GRAMMARITEMTYPE_ACTION,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_ACTION,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_AUTORANK,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LEFT,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RIGHT,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_GROUP,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_SEPARATOR,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PROPER,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RANK,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL_RANKING,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PRIORITY,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PAUSE,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LATM,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NAMING,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST_ITEMS,
  MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST
};

struct marpaESLIF_adverbItem {
  marpaESLIF_grammarItemType_t type;
  union {
    /* Four raw types possible */
    char                *asciis;
    marpaESLIF_string_t *stringp;
    int                  i;
    short                b;
  } u;
};

static inline void  _marpaESLIF_grammarContext_resetv(marpaESLIF_t *marpaESLIFp, marpaESLIF_grammarContext_t *marpaESLIF_grammarContextp);
static inline short _marpaESLIF_grammarContext_i_resetb(marpaESLIF_t *marpaESLIFp, genericStack_t *outputStackp, genericStack_t *itemTypeStackp, int i);
static inline short _marpaESLIF_grammarContext_get_typeb(marpaESLIF_t *marpaESLIFp, genericStack_t *itemTypeStackp, int i, marpaESLIF_grammarItemType_t *typep);
static inline short _marpaESLIF_grammarContext_set_typeb(marpaESLIF_t *marpaESLIFp, genericStack_t *itemTypeStackp, int i, marpaESLIF_grammarItemType_t type);

#define GENERATE_MARPAESLIF_GRAMMARCONTEXT_GETTER_BODY(genericStackType, itemType, methodBaseName, CType) \
  static const char            *funcs = "_marpaESLIF_grammarContext_get_" #methodBaseName "b"; \
  marpaESLIF_grammarItemType_t  type;                                   \
  short                         rcb;                                    \
  CType                         value;                                  \
                                                                        \
  if (! _marpaESLIF_grammarContext_get_typeb(marpaESLIFp, itemTypeStackp, i, &type)) { \
    goto err;                                                           \
  }                                                                     \
                                                                        \
  if (type == MARPAESLIF_GRAMMARITEMTYPE_##itemType) {                  \
    if (GENERICSTACK_IS_##genericStackType(outputStackp, i)) {          \
      value = (CType) GENERICSTACK_GET_##genericStackType(outputStackp, i); \
    } else {                                                            \
      MARPAESLIF_ERRORF(marpaESLIFp, "Not a %s in outputStackp at indice %d", #genericStackType, i); \
      goto err;                                                         \
    }                                                                   \
  } else {                                                              \
    MARPAESLIF_ERRORF(marpaESLIFp, "Not a MARPAESLIF_GRAMMARITEMTYPE_%s in itemTypeStackp at indice %d, got %d instead of %d", #itemType, i, type, MARPAESLIF_GRAMMARITEMTYPE_##itemType); \
    goto err;                                                           \
  }                                                                     \
                                                                        \
  if (valuep != NULL) {                                                 \
    *valuep = value;                                                    \
  }                                                                     \
  rcb = 1;                                                              \
  goto done;                                                            \
                                                                        \
err:                                                                    \
 rcb = 0;                                                               \
                                                                        \
done:                                                                   \
 return rcb

#define GENERATE_MARPAESLIF_GRAMMARCONTEXT_SETTER_BODY(genericStackType, itemType, methodBaseName, CType) \
  static const char            *funcs = "_marpaESLIF_grammarContext_set_" #methodBaseName "b"; \
  short                         rcb;                                    \
                                                                        \
  if (! _marpaESLIF_grammarContext_i_resetb(marpaESLIFp, outputStackp, itemTypeStackp, i)) { \
    goto err;                                                           \
  }                                                                     \
                                                                        \
  GENERICSTACK_SET_INT(itemTypeStackp, MARPAESLIF_GRAMMARITEMTYPE_##itemType, i); \
  if (GENERICSTACK_ERROR(itemTypeStackp)) {                             \
    MARPAESLIF_ERRORF(marpaESLIFp, "itemTypeStackp set failure, %s", strerror(errno)); \
    goto err;                                                           \
  }                                                                     \
  GENERICSTACK_SET_##genericStackType(outputStackp, value, i);          \
  if (GENERICSTACK_ERROR(outputStackp)) {                               \
    MARPAESLIF_ERRORF(marpaESLIFp, "outputStackp set failure, %s", strerror(errno)); \
    goto err;                                                           \
  }                                                                     \
                                                                        \
  rcb = 1;                                                              \
  goto done;                                                            \
                                                                        \
err:                                                                    \
 rcb = 0;                                                               \
                                                                        \
done:                                                                   \
 return rcb

#define MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(genericStackType, itemType, name, CType) \
  static inline short _marpaESLIF_grammarContext_get_##name##b(marpaESLIF_t *marpaESLIFp, genericStack_t *outputStackp, genericStack_t *itemTypeStackp, int i, CType *valuep) \
  {                                                                     \
    GENERATE_MARPAESLIF_GRAMMARCONTEXT_GETTER_BODY(genericStackType, itemType, ascii, CType); \
  }                                                                     \
  static inline short _marpaESLIF_grammarContext_set_##name##b(marpaESLIF_t *marpaESLIFp, genericStack_t *outputStackp, genericStack_t *itemTypeStackp, int i, CType value) \
  {                                                                     \
    GENERATE_MARPAESLIF_GRAMMARCONTEXT_SETTER_BODY(genericStackType, itemType, ascii, CType); \
  }

#define MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DECLARE_ACCESSORS(name, CType) \
  static inline short _marpaESLIF_grammarContext_get_##name##b(marpaESLIF_t *marpaESLIFp, genericStack_t *outputStackp, genericStack_t *itemTypeStackp, int i, CType *valuep); \
  static inline short _marpaESLIF_grammarContext_set_##name##b(marpaESLIF_t *marpaESLIFp, genericStack_t *outputStackp, genericStack_t *itemTypeStackp, int i, CType value);

MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(ARRAY, LEXEME,                   lexeme,                   GENERICSTACKITEMTYPE2TYPE_ARRAY)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(INT,   OP_DECLARE,               op_declare,               int)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ACTION_NAME,              action_name,              char *)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ACTION,                   action,                   char *)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ADVERB_ITEM_ACTION,       adverb_item_action,       char *)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_AUTORANK,     adverb_item_autorank,     short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_LEFT,         adverb_item_left,         short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_RIGHT,        adverb_item_right,        short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_GROUP,        adverb_item_group,        short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ADVERB_ITEM_SEPARATOR,    adverb_item_separator,    marpaESLIF_string_t *)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_PROPER,       adverb_item_proper,       short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(INT,   ADVERB_ITEM_RANK,         adverb_item_rank,         int)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_NULL_RANKING, adverb_item_null_ranking, short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(INT,   ADVERB_ITEM_PRIORITY,     adverb_item_priority,     int)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ADVERB_ITEM_PAUSE,        adverb_item_pause,        marpaESLIF_string_t *)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_LATM,         adverb_item_latm,         short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ADVERB_ITEM_NAMING,       adverb_item_naming,       marpaESLIF_string_t *)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(SHORT, ADVERB_ITEM_NULL,         adverb_item_null,         short)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ADVERB_LIST_ITEMS,        adverb_list_items,        genericStack_t *)
MARPAESLIF_INTERNAL_GRAMMARCONTEXT_DEFINE_ACCESSORS(PTR,   ADVERB_LIST,              adverb_list,              genericStack_t *)

#endif /* MARPAESLIF_INTERNAL_GRAMMARCONTEXT_H */
