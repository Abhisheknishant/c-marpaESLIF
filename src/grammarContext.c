
/*****************************************************************************/
static inline void _marpaESLIF_grammarContext_resetv(marpaESLIF_t *marpaESLIFp, marpaESLIF_grammarContext_t *marpaESLIF_grammarContextp)
/*****************************************************************************/
{
  int                   i;
  int                   usedi;
  genericStack_t       *outputStackp;
  genericStack_t       *itemTypeStackp;
  genericStack_t       *grammarStackp;
  marpaESLIF_grammar_t *grammarp;

  /* In the grammar context, outputStack contain items that are typed using itemTypeStackp */
  if (marpaESLIF_grammarContextp != NULL) {
    outputStackp   = marpaESLIF_grammarContextp->outputStackp;
    itemTypeStackp = marpaESLIF_grammarContextp->itemTypeStackp;
    grammarStackp  = marpaESLIF_grammarContextp->grammarStackp;

    /* If one of them is NULL, this cannot be done */
    if ((outputStackp != NULL) && (itemTypeStackp != NULL)) {
      usedi = (int) GENERICSTACK_USED(outputStackp);
      for (i = usedi - 1; i >= 0; i--) {
        _marpaESLIF_grammarContext_i_resetb(marpaESLIFp, outputStackp, itemTypeStackp, i);
      }
      /* We know there are "on the stack" */
      GENERICSTACK_RESET(outputStackp);
      GENERICSTACK_RESET(itemTypeStackp);
    }

    if (grammarStackp != NULL) {
      usedi = (int) GENERICSTACK_USED(grammarStackp);
      for (i = usedi - 1; i >= 0; i--) {
        if (! GENERICSTACK_IS_PTR(grammarStackp, i)) {
          continue;
        }
        grammarp = (marpaESLIF_grammar_t *) GENERICSTACK_GET_PTR(grammarStackp, i);
        _marpaESLIF_grammar_freev(grammarp);
      }
      /* We know it is on the "heap" */
      GENERICSTACK_FREE(grammarStackp);
    }
  }
}

/*****************************************************************************/
static inline short _marpaESLIF_grammarContext_i_resetb(marpaESLIF_t *marpaESLIFp, genericStack_t *outputStackp, genericStack_t *itemTypeStackp, int i)
/*****************************************************************************/
{
  static const char            *funcs = "_marpaESLIF_grammarContext_i_resetb";
  marpaESLIF_grammarItemType_t  itemType;
  int                           j;
  short                         rcb;

  if ((outputStackp != NULL) && (itemTypeStackp != NULL)) {
    if (GENERICSTACK_IS_INT(itemTypeStackp, i)) {
      itemType = (marpaESLIF_grammarItemType_t) GENERICSTACK_GET_INT(itemTypeStackp, i);

      switch (itemType) {
      case MARPAESLIF_GRAMMARITEMTYPE_LEXEME:        /* ARRAY */
        if (GENERICSTACK_IS_ARRAY(outputStackp, i)) {
          GENERICSTACKITEMTYPE2TYPE_ARRAY array =  GENERICSTACK_GET_ARRAY(outputStackp, i);
          if (GENERICSTACK_ARRAY_PTR(array) != NULL) {
            free(GENERICSTACK_ARRAY_PTR(array));
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_OP_DECLARE:         /* INT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ACTION_NAME:        /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ACTION:             /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_ACTION: /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_AUTORANK: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LEFT: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RIGHT: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_GROUP: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_SEPARATOR: /* String */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          marpaESLIF_string_t *stringp = (marpaESLIF_string_t *) GENERICSTACK_GET_PTR(outputStackp, i);
          _marpaESLIF_string_freev(stringp);
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PROPER: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RANK: /* INT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL_RANKING: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PRIORITY: /* INT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PAUSE: /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LATM: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NAMING: /* String */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          marpaESLIF_string_t *stringp = (marpaESLIF_string_t *) GENERICSTACK_GET_PTR(outputStackp, i);
          _marpaESLIF_string_freev(stringp);
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL: /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST_ITEMS:  /* Stack of marpaESLIF_adverbItem_t */
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST:        /* Alias to MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST_ITEMS - the two are exclusives (they share the same indice in outputStackp) */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          genericStack_t *stackp = (genericStack_t *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (stackp != NULL) {
            for (j = 0; j < GENERICSTACK_USED(stackp); j++) {
              if (GENERICSTACK_IS_PTR(stackp, j)) {
                marpaESLIF_adverbItem_t *adverbItemp = (marpaESLIF_adverbItem_t *) GENERICSTACK_GET_PTR(stackp, j);
                if (adverbItemp != NULL) {
                  switch (adverbItemp->type) {
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_ACTION: /* ASCII string */
                    if (adverbItemp->u.asciis != NULL) {
                      free(adverbItemp->u.asciis);
                    }
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_AUTORANK: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LEFT: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RIGHT: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_GROUP: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_SEPARATOR: /* String */
                    _marpaESLIF_string_freev(adverbItemp->u.stringp);
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PROPER: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RANK: /* INT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL_RANKING: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PRIORITY: /* INT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PAUSE: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LATM: /* SHORT */
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NAMING: /* String */
                    _marpaESLIF_string_freev(adverbItemp->u.stringp);
                    break;
                  case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL: /* NA */
                    break;
                  default:
                    break;
                  }
                  free(adverbItemp);
                }
              }
            }
            GENERICSTACK_FREE(stackp);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_LATM:               /* SHORT */
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_SYMBOL_NAME:        /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_SYMBOL:             /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_LHS:                /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_SINGLE_SYMBOL:       /* ASCII string */
        if (GENERICSTACK_IS_PTR(outputStackp, i)) {
          char *asciis = (char *) GENERICSTACK_GET_PTR(outputStackp, i);
          if (asciis != NULL) {
            free(asciis);
          }
        }
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_QUANTIFIER:          /* INT */
        break;
      default:
        break;
      }
    }

    GENERICSTACK_SET_NA(itemTypeStackp, i);
    if (GENERICSTACK_ERROR(itemTypeStackp)) {
      MARPAESLIF_ERRORF(marpaESLIFp, "itemTypeStackp set failure, %s", strerror(errno));
      goto err;
    }
    GENERICSTACK_SET_NA(outputStackp, i);
    if (GENERICSTACK_ERROR(outputStackp)) {
      MARPAESLIF_ERRORF(marpaESLIFp, "outputStackp set failure, %s", strerror(errno));
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
static inline const char *_marpaESLIF_grammarContext_i_types(marpaESLIF_t *marpaESLIFp, genericStack_t *outputStackp, genericStack_t *itemTypeStackp, int i)
/*****************************************************************************/
{
  static const char            *funcs = "_marpaESLIF_grammarContext_i_types";
  marpaESLIF_grammarItemType_t  itemType;
  const char                   *rcs = marpESLIF_grammarContext_UNKNOWN_types;

  if ((outputStackp != NULL) && (itemTypeStackp != NULL)) {
    if (GENERICSTACK_IS_INT(itemTypeStackp, i)) {
      itemType = (marpaESLIF_grammarItemType_t) GENERICSTACK_GET_INT(itemTypeStackp, i);

      switch (itemType) {
      case MARPAESLIF_GRAMMARITEMTYPE_NA:        /* N/A */
        rcs = marpESLIF_grammarContext_NA_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_LEXEME:        /* ARRAY */
        rcs = marpESLIF_grammarContext_LEXEME_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_OP_DECLARE:         /* INT */
        rcs = marpESLIF_grammarContext_OP_DECLARE_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ACTION_NAME:        /* ASCII string */
        rcs = marpESLIF_grammarContext_ACTION_NAME_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ACTION:             /* ASCII string */
        rcs = marpESLIF_grammarContext_ACTION_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_ACTION: /* ASCII string */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_ACTION_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_AUTORANK: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_AUTORANK_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LEFT: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_LEFT_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RIGHT: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_RIGHT_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_GROUP: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_GROUP_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_SEPARATOR: /* String */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_SEPARATOR_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PROPER: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_PROPER_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_RANK: /* INT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_RANK_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL_RANKING: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_NULL_RANKING_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PRIORITY: /* INT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_PRIORITY_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_PAUSE: /* ASCII string */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_PAUSE_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_LATM: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_LATM_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NAMING: /* String */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_NAMING_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_ITEM_NULL: /* SHORT */
        rcs = marpESLIF_grammarContext_ADVERB_ITEM_NULL_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST_ITEMS:  /* Stack of marpaESLIF_adverbItem_t */
        rcs = marpESLIF_grammarContext_ADVERB_LIST_ITEMS_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST:        /* Alias to MARPAESLIF_GRAMMARITEMTYPE_ADVERB_LIST_ITEMS - the two are exclusives (they share the same indice in outputStackp) */
        rcs = marpESLIF_grammarContext_ADVERB_LIST_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_LATM:               /* SHORT */
        rcs = marpESLIF_grammarContext_LATM_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_SYMBOL_NAME:        /* ASCII string */
        rcs = marpESLIF_grammarContext_SYMBOL_NAME_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_SYMBOL:             /* ASCII string */
        rcs = marpESLIF_grammarContext_SYMBOL_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_LHS:                /* ASCII string */
        rcs = marpESLIF_grammarContext_LHS_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_SINGLE_SYMBOL:       /* ASCII string */
        rcs = marpESLIF_grammarContext_SINGLE_SYMBOL_types;
        break;
      case MARPAESLIF_GRAMMARITEMTYPE_QUANTIFIER:          /* INT */
        rcs = marpESLIF_grammarContext_QUANTIFIER_types;
        break;
      default:
        break;
      }
    }
  }

  return rcs;
}

/*****************************************************************************/
static inline short _marpaESLIF_grammarContext_get_typeb(marpaESLIF_t *marpaESLIFp, genericStack_t *itemTypeStackp, int i, marpaESLIF_grammarItemType_t *typep)
/*****************************************************************************/
{
  static const char            *funcs = "_marpaESLIF_grammarContext_get_typeb";
  marpaESLIF_grammarItemType_t  type;
  short                         rcb;

  if (GENERICSTACK_IS_INT(itemTypeStackp, i)) {
    type = (marpaESLIF_grammarItemType_t) GENERICSTACK_GET_INT(itemTypeStackp, i);
  } else {
    MARPAESLIF_ERRORF(marpaESLIFp, "Not an INT in itemTypeStackp at indice %d", i);
    goto err;
  }

  if (typep != NULL) {
    *typep = type;
  }
  rcb = 1;
  goto done;

 err:
  rcb = 0;

 done:
  return rcb;
}

/*****************************************************************************/
static inline short _marpaESLIF_grammarContext_set_typeb(marpaESLIF_t *marpaESLIFp, genericStack_t *itemTypeStackp, int i, marpaESLIF_grammarItemType_t type)
/*****************************************************************************/
{
  static const char *funcs = "_marpaESLIF_grammarContext_set_typeb";
  short              rcb;

  GENERICSTACK_SET_INT(itemTypeStackp, type, i);
  if (GENERICSTACK_ERROR(itemTypeStackp)) {
    MARPAESLIF_ERRORF(marpaESLIFp, "itemTypeStackp set failure at indice %d, %s", i, strerror(errno));
    goto err;
  }

  rcb = 1;
  goto done;

 err:
  rcb = 0;

 done:
  return rcb;
}
