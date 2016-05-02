#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <unicode/uconfig.h>
#include <unicode/ucnv.h>
#if !UCONFIG_NO_TRANSLITERATION
#include <unicode/utrans.h>
#endif
#include <unicode/uset.h>
#include <unicode/ustring.h>

#include "tconv/convert/ICU.h"
/* Because this is a built-in, it can take advantage of TCONV_TRACE macro */
#include "tconv_config.h"

#define TCONV_ICU_IGNORE   "//IGNORE"
#define TCONV_ICU_TRANSLIT "//TRANSLIT"

/* Default option */
#define TCONV_ENV_CONVERT_ICU_UCHARCAPACITY "TCONV_ENV_CONVERT_ICU_UCHARCAPACITY"
#define TCONV_ENV_CONVERT_ICU_FALLBACK      "TCONV_ENV_CONVERT_ICU_FALLBACK"
#define TCONV_ENV_CONVERT_ICU_SIGNATURE     "TCONV_ENV_CONVERT_ICU_SIGNATURE"

tconv_convert_ICU_option_t tconv_convert_icu_option_default = {
  4096, /* uCharCapacityl - take care we "lie" by allocating uCharCapacityl+1 for the eventual signature */
  0,    /* fallbackb */
  0,    /* signaturei */
};

/* Context */
typedef struct tconv_convert_ICU_context {
  UConverter                 *uConverterFromp;   /* Input => UChar  */
  UChar                      *uCharBufp;         /* UChar buffer    */
  int32_t                    *fromOffsetp;       /* Capacity known in advance: uCharCapacityl */
  UChar                      *uCharBufLimitp;    /* UChar buffer limit */
  int32_t                     uCharCapacityl;    /* Allocated Length (not bytes) */
  char                       *charTmpp;          /* used to reconstruct the fom converter state */
  int32_t                     charTmpCapacityl;  /* Allocated Length (not bytes) */
  UConverter                 *uConverterTop;     /* UChar => Output */
  int8_t                      signaturei;
  UBool                       firstb;
#if !UCONFIG_NO_TRANSLITERATION
  UChar                      *chunkp;
  UChar                      *chunkCopyp;        /* Because no pre-fighting is possible with utrans_transUchars() */
  int32_t                     chunkCapacityl;    /* Used Length (not bytes) */
  int32_t                     chunkUsedl;        /* Used Length (not bytes) */
  UChar                      *translitp;
  int32_t                     translitCapacityl; /* Transformed used Length (not bytes) */
  int32_t                     translitUsedl;     /* Transformed used Length (not bytes) */
  UTransliterator            *uTransliteratorp;  /* Transliteration */
#endif
} tconv_convert_ICU_context_t;

size_t _tconv_convert_ICU_run(tconv_t tconvp, tconv_convert_ICU_context_t *contextp, char **inbufpp, size_t *inbytesleftlp, char **outbufpp, size_t *outbytesleftlp, UBool flushb);

#if !UCONFIG_NO_TRANSLITERATION
static TCONV_C_INLINE int32_t _getChunkLimit(const UChar *prevp, int32_t prevlenl, const UChar *p, int32_t lengthl);
static TCONV_C_INLINE int32_t _cnvSigType(UConverter *uConverterp);
static TCONV_C_INLINE UBool   _increaseChunkBuffer(tconv_convert_ICU_context_t *contextp, int32_t chunkcapacity);
static TCONV_C_INLINE UBool   _increaseCharTmpBuffer(tconv_convert_ICU_context_t *contextp, int32_t charTmpCapacityl);
#endif

/*****************************************************************************/
void  *tconv_convert_ICU_new(tconv_t tconvp, const char *tocodes, const char *fromcodes, void *voidp)
/*****************************************************************************/
{
  static const char            funcs[]          = "tconv_convert_ICU_new";
  tconv_convert_ICU_option_t  *optionp          = (tconv_convert_ICU_option_t *) voidp;
  UBool                        ignoreb          = FALSE;
  UBool                        translitb        = FALSE;
  char                        *realToCodes      = NULL;
  tconv_convert_ICU_context_t *contextp         = NULL;
  char                        *ignorep          = NULL;
  char                        *endIgnorep       = NULL;
  char                        *translitp        = NULL;
  char                        *endTranslitp     = NULL;
  UConverter                  *uConverterFromp  = NULL;
  UChar                       *uCharBufp        = NULL;
  int32_t                     *fromOffsetp      = NULL;
  UChar                       *uCharBufLimitp   = NULL;
  int32_t                      uCharCapacityl   = 0;
#if !UCONFIG_NO_TRANSLITERATION
  UTransliterator             *uTransliteratorp = NULL;
#endif
  UConverter                  *uConverterTop    = NULL;
  UConverterFromUCallback      fromUCallbackp   = NULL;
  const void                  *fromuContextp    = NULL;
  UConverterToUCallback        toUCallbackp     = NULL;
  const void                  *toUContextp      = NULL;
  UBool                        fallbackb        = FALSE;
  int8_t                       signaturei       = 0;
  int32_t                      uSetPatternTol   = 0;
  UChar                       *uSetPatternTos   = NULL;
  USet                        *uSetTop          = NULL;
  USet                        *uSetp            = NULL;
  UErrorCode                   uErrorCode;
  char                        *p, *q;
  UConverterUnicodeSet         whichSet;
#define universalTransliteratorsLength 22
  U_STRING_DECL(universalTransliterators, "Any-Latin; Latin-ASCII", universalTransliteratorsLength);

  U_STRING_INIT(universalTransliterators, "Any-Latin; Latin-ASCII", universalTransliteratorsLength);

  if ((tocodes == NULL) || (fromcodes == NULL)) {
    errno = EINVAL;
    goto err;
  }

  /* ----------------------------------------------------------- */
  /* Duplicate tocodes and manage //IGNORE and //TRANSLIT option */
  /* ----------------------------------------------------------- */
  realToCodes = strdup(tocodes);
  if (realToCodes == NULL) {
    goto err;
  }
  ignorep   = strstr(realToCodes, TCONV_ICU_IGNORE);
  translitp = strstr(realToCodes, TCONV_ICU_TRANSLIT);
  /* They must end the string or be followed by another (maybe) option */
  if (ignorep != NULL) {
    endIgnorep = ignorep + strlen(TCONV_ICU_IGNORE);
    if ((*endIgnorep == '\0') || (*(endIgnorep + 1) == '/')) {
      ignoreb = TRUE;
    }
  }
  if (translitp != NULL) {
    endTranslitp = translitp + strlen(TCONV_ICU_TRANSLIT);
    if ((*endTranslitp == '\0') || (*(endTranslitp + 1) == '/')) {
      translitb = TRUE;
    }
  }
  /* ... Remove options from realToCodes  */
  for (p = q = realToCodes; *p != '\0'; ++p) {    /* Note that a valid charset cannot contain \0 */
    if ((ignoreb == TRUE) && ((p >= ignorep) && (p < endIgnorep))) {
      continue;
    }
    if ((translitb == TRUE) && ((p >= translitp) && (p < endTranslitp))) {
      continue;
    }
    if (p != q) {
      *q++ = *p;
    } else {
      q++;
    }
  }
  *q = '\0';
  TCONV_TRACE(tconvp, "%s - \"%s\" gives {codeset=\"%s\", ignore=%s translit=%s}", funcs, tocodes, realToCodes, (translitb == TRUE) ? "TRUE" : "FALSE", (ignoreb == TRUE) ? "TRUE" : "FALSE");

  /* ----------------------------------------------------------- */
  /* Get options                                                 */
  /* ----------------------------------------------------------- */
  if (optionp == NULL) {
    optionp = &tconv_convert_icu_option_default;
  }

  uCharCapacityl = optionp->uCharCapacityl;
  fallbackb      = (optionp->fallbackb !=0 ) ? TRUE : FALSE;
  signaturei     = (optionp->signaturei < 0) ? -1 : ((optionp->signaturei > 0) ? 1 : 0);

  /* These can be overwriten with environment variables */
  p = getenv(TCONV_ENV_CONVERT_ICU_UCHARCAPACITY);
  if (p != NULL) {
    uCharCapacityl = atoi(p);
  }
  if (uCharCapacityl <= 0) {
    errno = EINVAL;
    goto err;
  }
  p = getenv(TCONV_ENV_CONVERT_ICU_FALLBACK);
  if (p != NULL) {
    fallbackb = (atoi(p) != 0) ? TRUE : FALSE;
  }
  p = getenv(TCONV_ENV_CONVERT_ICU_SIGNATURE);
  if (p != NULL) {
    int i;
    i = atoi(p);
    signaturei = (i < 0) ? -1 : ((i > 0) ? 1 : 0);
  }
  TCONV_TRACE(tconvp, "%s - options are {uCharCapacityl=%lld, fallbackb=%s, signaturei=%d}", funcs, (unsigned long long) uCharCapacityl, (fallbackb == TRUE) ? "TRUE" : "FALSE", signaturei);

  /* ----------------------------------------------------------- */
  /* Setup the from converter                                    */
  /* ----------------------------------------------------------- */
  fromUCallbackp = (ignoreb == TRUE) ? UCNV_FROM_U_CALLBACK_SKIP : UCNV_FROM_U_CALLBACK_STOP;
  fromuContextp  = NULL;

  uErrorCode = U_ZERO_ERROR;
  uConverterFromp = ucnv_open(fromcodes, &uErrorCode);
  if (U_FAILURE(uErrorCode)) {
    errno = ENOSYS;
    goto err;
  }

  uErrorCode = U_ZERO_ERROR;
  ucnv_setFromUCallBack(uConverterFromp, fromUCallbackp, fromuContextp, NULL, NULL, &uErrorCode);
  if (U_FAILURE(uErrorCode)) {
    errno = ENOSYS;
    goto err;
  }

  ucnv_setFallback(uConverterFromp, fallbackb);

  /* ----------------------------------------------------------- */
  /* Setup the proxy unicode buffer ./..                         */
  /* ----------------------------------------------------------- */
  {
    size_t uCharSizel = uCharCapacityl * sizeof(UChar);
    /* + sizeof(UChar) for the eventual signature */
    uCharBufp = (UChar *) malloc(uCharSizel + sizeof(UChar));
    if (uCharBufp == NULL) {
      goto err;
    }
    uCharBufLimitp = uCharBufp + uCharCapacityl; /* In unit of UChar */
  }
  
  /* ----------------------------------------------------------- */
  /* ../. and associated offsets - used to be POSIX compliant    */
  /* ----------------------------------------------------------- */
  {
    size_t fromOffsetSizel = uCharCapacityl * sizeof(int32_t);
    fromOffsetp = (int32_t *) malloc(fromOffsetSizel);
    if (fromOffsetp == NULL) {
      goto err;
    }
  }
  
  /* ----------------------------------------------------------- */
  /* Setup the to converter                                      */
  /* ----------------------------------------------------------- */
  toUCallbackp   = (ignoreb == TRUE) ? UCNV_TO_U_CALLBACK_SKIP   : UCNV_TO_U_CALLBACK_STOP;
  toUContextp    = NULL;

  uErrorCode = U_ZERO_ERROR;
  uConverterTop = ucnv_open(realToCodes, &uErrorCode);
  if (U_FAILURE(uErrorCode)) {
    errno = ENOSYS;
    goto err;
  }
  /* No need anymore of realToCodes */
  free(realToCodes);
  realToCodes = NULL;

  uErrorCode = U_ZERO_ERROR;
  ucnv_setToUCallBack(uConverterTop, toUCallbackp, toUContextp, NULL, NULL, &uErrorCode);
  if (U_FAILURE(uErrorCode)) {
    errno = ENOSYS;
    goto err;
  }

  ucnv_setFallback(uConverterTop, fallbackb);

  /* ----------------------------------------------------------- */
  /* Setup the transliterator                                    */
  /* ----------------------------------------------------------- */
  if (translitb == TRUE) {
#if UCONFIG_NO_TRANSLITERATION
    TCONV_TRACE(tconvp, "%s - translitb is TRUE but config says UCONFIG_NO_TRANSLITERATION", funcs);
    errno = ENOSYS;
    goto err;
#else
    whichSet = (fallbackb == TRUE) ? UCNV_ROUNDTRIP_AND_FALLBACK_SET : UCNV_ROUNDTRIP_SET;

    /* Transliterator is generated on-the-fly using the unicode */
    /* sets from the two converters.                            */
    
    /* ------------------------- */
    /* Uset for the to converter */
    /* ------------------------- */
    uSetTop = uset_openEmpty();
    if (uSetTop == NULL) { /* errno ? */
      errno = ENOSYS;
      goto err;
    }

    uErrorCode = U_ZERO_ERROR;
    ucnv_getUnicodeSet(uConverterTop, uSetTop, whichSet, &uErrorCode);
    if (U_FAILURE(uErrorCode)) {
      errno = ENOSYS;
      goto err;
    }

    uErrorCode = U_ZERO_ERROR;
    uSetPatternTol = uset_toPattern(uSetTop, NULL, 0, TRUE, &uErrorCode);
    if (uErrorCode != U_BUFFER_OVERFLOW_ERROR) {
      errno = ENOSYS;
      goto err;
    }
    uSetPatternTos = malloc((uSetPatternTol + 1) * sizeof(UChar));
    if (uSetPatternTos == NULL) {
      goto err;
    }
    uErrorCode = U_ZERO_ERROR;
    uset_toPattern(uSetTop, uSetPatternTos, uSetPatternTol, TRUE, &uErrorCode);
    if (U_FAILURE(uErrorCode)) {
      errno = ENOSYS;
      goto err;
    }
    /* Make sure uSetPatternTos is NULL terminated (a-la UTF-16) */
    p = (char *) (uSetPatternTos + uSetPatternTol);
    *p++ = '\0';
    *p   = '\0';

    /* ---------------------------------------------------------------------------- */
    /* Create transliterator: "[toPattern] Any-Latin; Latin-ASCII"                  */
    /* ---------------------------------------------------------------------------- */
    TCONV_TRACE(tconvp, "%s - creating a \"%s\" transliterator", funcs, "Any-Latin; Latin-ASCII");
    uErrorCode = U_ZERO_ERROR;
    uTransliteratorp = utrans_openU(universalTransliterators,
                                    universalTransliteratorsLength,
                                    UTRANS_FORWARD,
                                    NULL,
                                    0,
                                    NULL,
                                    &uErrorCode);
    if (U_FAILURE(uErrorCode)) {
      errno = ENOSYS;
      goto err;
    }

    /* Add a filter to this transliterator using "to" pattern */
#ifndef TCONV_NTRACE
    {
      int32_t  patternCapacityl = 0;
  
      uErrorCode = U_ZERO_ERROR;
      u_strToUTF8(NULL, 0, &patternCapacityl, uSetPatternTos, uSetPatternTol, &uErrorCode);
      if (U_SUCCESS(uErrorCode)) {
	char *patterns = (char *) malloc(patternCapacityl + 1);
	if (patterns != NULL) {
	  uErrorCode = U_ZERO_ERROR;
	  u_strToUTF8(patterns, patternCapacityl, &patternCapacityl, uSetPatternTos, uSetPatternTol, &uErrorCode);
	  if (U_SUCCESS(uErrorCode)) {
	    patterns[patternCapacityl] = '\0';
	    /* In theory the pattern should have no non-ASCII character - if false, tant pis -; */
	    TCONV_TRACE(tconvp, "%s - filtering transliterator with \"to\" converter pattern: %s", funcs, patterns);
	  }
	  free(patterns);
	}
      }
    }
#endif

    uErrorCode = U_ZERO_ERROR;
    utrans_setFilter(uTransliteratorp,
                     uSetPatternTos,
                     uSetPatternTol,
                     &uErrorCode);
    if (U_FAILURE(uErrorCode)) {
      errno = ENOSYS;
      goto err;
    }

    /* Cleanup */
    uset_close(uSetTop);
    uSetTop = NULL;

    uset_close(uSetp);
    uSetp = NULL;

    free(uSetPatternTos);
    uSetPatternTos = NULL;
#endif /* UCONFIG_NO_TRANSLITERATION */
  }

  /* ----------------------------------------------------------- */
  /* Setup the context                                           /
  /* ----------------------------------------------------------- */
  contextp = (tconv_convert_ICU_context_t *) malloc(sizeof(tconv_convert_ICU_context_t));
  if (contextp == NULL) {
    goto err;
  }

  contextp->uConverterFromp   = uConverterFromp;
  contextp->uCharBufp         = uCharBufp;
  contextp->charTmpp          = NULL;
  contextp->charTmpCapacityl  = 0;
  contextp->fromOffsetp       = fromOffsetp;
  contextp->uCharBufLimitp    = uCharBufLimitp;
  contextp->uCharCapacityl    = uCharCapacityl;
  contextp->uConverterTop     = uConverterTop;
  contextp->signaturei        = signaturei;
  contextp->firstb            = TRUE;
#if !UCONFIG_NO_TRANSLITERATION
  contextp->chunkp            = NULL;
  contextp->chunkCopyp        = NULL;
  contextp->chunkCapacityl    = 0;
  contextp->chunkUsedl        = 0;
  contextp->translitp         = NULL;
  contextp->translitCapacityl = 0;
  contextp->translitUsedl     = 0;
  contextp->uTransliteratorp  = uTransliteratorp;
#endif

  return contextp;

 err:
  {
    int errnol = errno;
    if (realToCodes != NULL) {
      free(realToCodes);
    }
    if (U_FAILURE(uErrorCode)) {
      tconv_error_set(tconvp, u_errorName(uErrorCode));
    }
    if (uConverterFromp != NULL) {
      ucnv_close (uConverterFromp);
    }
    if (uCharBufp != NULL) {
      free(uCharBufp);
    }
    if (fromOffsetp != NULL) {
      free(fromOffsetp);
    }
    if (uConverterTop != NULL) {
      ucnv_close (uConverterTop);
    }
    if (uSetPatternTos == NULL) {
      free(uSetPatternTos);
    }
    if (uSetTop != NULL) {
      uset_close(uSetTop);
    }
    if (uSetp != NULL) {
      uset_close(uSetp);
    }
#if !UCONFIG_NO_TRANSLITERATION
    if (uTransliteratorp != NULL) {
      utrans_close(uTransliteratorp);
    }
#endif
    if (contextp != NULL) {
      free(contextp);
    }
    errno = errnol;
  }
  TCONV_TRACE(tconvp, "%s - return -1", funcs);
  return (void *)-1;
}

enum {
  uSP  = 0x20,         /* space */
  uCR  = 0xd,          /* carriage return */
  uLF  = 0xa,          /* line feed */
  uNL  = 0x85,         /* newline */
  uLS  = 0x2028,       /* line separator */
  uPS  = 0x2029,       /* paragraph separator */
  uSig = 0xfeff        /* signature/BOM character */
};

enum {
  CNV_NO_FEFF,    /* cannot convert the U+FEFF Unicode signature character (BOM) */
  CNV_WITH_FEFF,  /* can convert the U+FEFF signature character */
  CNV_ADDS_FEFF   /* automatically adds/detects the U+FEFF signature character */
};

/*****************************************************************************/
size_t tconv_convert_ICU_run(tconv_t tconvp, void *voidp, char **inbufpp, size_t *inbytesleftlp, char **outbufpp, size_t *outbytesleftlp)
/*****************************************************************************/
{
  static const char            funcs[]  = "tconv_convert_ICU_run";
  tconv_convert_ICU_context_t *contextp = (tconv_convert_ICU_context_t *) voidp;
  char                        *dummys   = "";
  size_t                       rcl      = 0;
  size_t                       chunkl;
  char                        *inbufp;
  size_t                       inbytesleftl;
  char                        *outbufp;
  size_t                       outbytesleftl;
  UBool                        flushb;

  /* Sanity check */
  if ((outbufpp == NULL) || (outbytesleftlp == NULL)) {
    errno = EINVAL;
    goto err;
  }

  /* Prepare work variables */
  inbufp        = (inbufpp       != NULL) ? *inbufpp       : dummys;
  inbytesleftl  = (inbytesleftlp != NULL) ? *inbytesleftlp : 0;
  outbufp       = *outbufpp;
  outbytesleftl = *outbytesleftlp;
  flushb        = ((inbufpp != NULL) && (*inbufpp != NULL)) ? FALSE : TRUE;
  if ((flushb == TRUE) && (inbytesleftl != 0)) {
    /* No byte to read in any case if this is a flush */
    inbytesleftl = 0;
  }

  /* Do the work by chunks */
  while (1) {
    chunkl = _tconv_convert_ICU_run(tconvp, contextp, &inbufp, &inbytesleftl, &outbufp, &outbytesleftl, flushb);
    if (chunkl == (size_t)-1) {
      rcl = chunkl;
      break;
    }
    rcl += chunkl;
  }

  /* Commit if success or E2BIG */
  if ((rcl >= 0) || (errno == E2BIG)) {
    if (inbufpp != NULL) {
      *inbufpp = inbufp;
      *inbytesleftlp = inbytesleftl;
    }
    *outbytesleftlp = outbytesleftl;
    *outbufpp = outbufp;
  }
  
  return rcl;

 err:
  return (size_t)-1;
}

/*****************************************************************************/
size_t _tconv_convert_ICU_run(tconv_t tconvp, tconv_convert_ICU_context_t *contextp, char **inbufpp, size_t *inbytesleftlp, char **outbufpp, size_t *outbytesleftlp, UBool flushb)
/*****************************************************************************/
/* This method differs from tconv_convert_ICU_run() in the sense that it     */
/* works on a "chunk", where the chunk is not necessarily *inbytesleftlp.    */
/* If there is no transliteration, the chunk IS *inbytesleftlp, but if there */
/* is transliteration, the chunk is very likely to be smaller.               */
/* This method guarantees that if E2BIG is reached, converters and pointers  */
/* are in a coherent state.                                                  */
/*****************************************************************************/
{
  /* Constants */
  static const char funcs[]           = "_tconv_convert_ICU_run";
  UConverter       *uConverterFromp   = contextp->uConverterFromp;
  UConverter       *uConverterTop     = contextp->uConverterTop;
#if !UCONFIG_NO_TRANSLITERATION
  UTransliterator  *uTransliteratorp  = contextp->uTransliteratorp;
#endif
  int32_t          *fromOffsetp       = contextp->fromOffsetp;
  const UChar      *uCharBuforigp     = (const UChar *) contextp->uCharBufp;
  const UChar      *uCharBufLimitp    = (const UChar *) contextp->uCharBufLimitp;
  
  const char       *inbufOrigp        = (const char *) *inbufpp;
  const size_t      inbytesleftOrigl  = *inbytesleftlp;
  const char       *inbufLimitp       = (const char *) (inbufOrigp + inbytesleftOrigl);

  const char       *outbufOrigp       = (const char *) *outbufpp;
  const size_t      outbytesleftOrigl = *outbytesleftlp;
  const char       *outbufLimitp      = (const char *) (outbufOrigp + outbytesleftOrigl);
  /* Variables */
  UErrorCode        uErrorCode;
  UChar            *uCharBufp         = (UChar *) uCharBuforigp;
  UBool             firstb            = contextp->firstb;
  const char       *inbufp            = (const char *) inbufOrigp;
  char             *outbufp           = *outbufpp;
  size_t            outbytesleftl     = *outbytesleftlp;
  UChar            *up;
  int32_t           uLengthl;
  int32_t           textCapacityl;
  int32_t           limitl;
  const char       *lastOkCharp;
  int32_t           charTmpCapacityl;
  size_t            inbytesleftl;

  /* --------------------------------------------------------------------- */
  /* Input => UChar                                                        */
  /* --------------------------------------------------------------------- */
  uErrorCode = U_ZERO_ERROR;
  ucnv_toUnicode(uConverterFromp,
		 &uCharBufp,
		 uCharBufLimitp,
		 &inbufp,
		 inbufLimitp,
		 fromOffsetp,
		 flushb,
		 &uErrorCode);
  if (U_FAILURE(uErrorCode) && (uErrorCode != U_BUFFER_OVERFLOW_ERROR)) {
    errno = ENOSYS;
    goto err;
  }
  up           = (UChar *) uCharBuforigp;
  uLengthl     = uCharBufp - up;
  inbytesleftl = inbufLimitp - inbufp;
#ifndef TCONV_NTRACE
  {
    TCONV_TRACE(tconvp, "%s - converted %lld bytes into %lld UChars",
		funcs,
		(unsigned long long) (inbytesleftOrigl - inbytesleftl),
		(unsigned long long) uLengthl);
  }
#endif
  /* --------------------------------------------------------------------- */
  /* Eventually remove signature                                           */
  /* --------------------------------------------------------------------- */
  if (firstb) {
    if (contextp->signaturei < 0) {
      if (uLengthl > 0) {
	if (up[0] == uSig) {
	  TCONV_TRACE(tconvp, "%s - removing signature", funcs);
	  up++;
	  --uLengthl;
	}
      }
    }
  }
  /* --------------------------------------------------------------------- */
  /* Get the chunk definition                                              */
  /* --------------------------------------------------------------------- */
#if !UCONFIG_NO_TRANSLITERATION
  if (uTransliteratorp != NULL) {
    /* ------------------------------------------------------------------- */
    /* UChar => UChar (transliterated)                                     */
    /* ------------------------------------------------------------------- */
    int32_t chunkl = _getChunkLimit(contextp->chunkp,
				    contextp->chunkUsedl,
				    up,
				    uLengthl);
    if (chunkl < 0) {       /* No paragraph ending found */
      if (flushb == TRUE) { /* But this is end of the input */
	chunkl = uLengthl;
      }
    }
    TCONV_TRACE(tconvp, "%s - chunk length is %lld", funcs, (signed long long) chunkl);
    if (chunkl >= 0) {
      int32_t  textLengthl;
	
      /* Concatenate accepted UChars with ones from previous round if any */
      if (uLengthl > 0) {
	int32_t newchunkused = contextp->chunkUsedl + uLengthl;
	if (newchunkused > contextp->chunkCapacityl) {
	  if (_increaseChunkBuffer(contextp, newchunkused) == FALSE) {
	    goto err;
	  }
	}
	if (contextp->chunkUsedl > 0) {
	  memmove(contextp->chunkp + uLengthl, contextp->chunkp, contextp->chunkUsedl);
	}
	memcpy(contextp->chunkp, uCharBuforigp, uLengthl);
	contextp->chunkUsedl += uLengthl;
      }
      /* utrans_transUChars() is not very user-friendly, in the sense  */
      /* that prefighting is not possible without affecting the buffer */
      if (chunkl > 0) {
	/* Copy of original chunk if we have to retry */
	memcpy(contextp->chunkCopyp, contextp->chunkp, contextp->chunkUsedl);
	textLengthl   = contextp->chunkUsedl;
	textCapacityl = contextp->chunkCapacityl;
	limitl        = contextp->chunkUsedl;
	uErrorCode = U_ZERO_ERROR;
	utrans_transUChars(contextp->uTransliteratorp,
			   contextp->chunkp,
			   &textLengthl,
			   textCapacityl,
			   0,
			   &limitl,
			   &uErrorCode);
	if (uErrorCode == U_BUFFER_OVERFLOW_ERROR) {
	  /* Voila... Increase chunk allocated size */
	  if (_increaseChunkBuffer(contextp, textLengthl) == FALSE) {
	    goto err;
	  }
	  /* Restore chunk data */
	  memcpy(contextp->chunkp, contextp->chunkCopyp, contextp->chunkUsedl);
	  /* And retry. This should never fail a second time */
	  textLengthl   = contextp->chunkUsedl;
	  textCapacityl = contextp->chunkCapacityl;
	  limitl        = contextp->chunkUsedl;
	  uErrorCode = U_ZERO_ERROR;
	  utrans_transUChars(contextp->uTransliteratorp,
			     contextp->chunkp,
			     &textLengthl,
			     textCapacityl,
			     0,
			     &limitl,
			     &uErrorCode);
	  if (U_FAILURE(uErrorCode)) {
	    goto err;
	  }
	}
      }
      /* Transliterated chunk */
      up       = contextp->chunkp;
      uLengthl = textLengthl;
    } else {
      /* No transliterated chunk */
      up       = NULL;
      uLengthl = 0;
    }
  }
#endif
  /* Whatever happened, handle eventual signature add */
  if (firstb) {
    if (contextp->signaturei > 0) {
      /* Whatever buffer is used: contextp->uCharBufp or */
      /* contextp->chunkp, there is always a + 1 hiden   */
      if (((uLengthl > 0) &&
	   (up[0] != uSig) &&
	   (_cnvSigType(contextp->uConverterTop) == CNV_WITH_FEFF))
	  ||
	  (uLengthl <= 0)
	  ) {
	TCONV_TRACE(tconvp, "%s - adding signature", funcs);
	up[0] = (UChar)uSig;
	++uLengthl;
      }
    }
  }
  /* ------------------------------------------------------------------- */
  /* UChar => Output                                                     */
  /* ------------------------------------------------------------------- */
  if (uLengthl >= 0) {  /* Can be zero when flushing */
    const char  *targetLimitp = (const char *) (outbufp + *outbytesleftlp);
    const UChar  *sourcep = (const UChar *) uCharBuforigp;
    const UChar  *sourceLimitp = (const UChar *) (sourcep + uLengthl);
    const UChar  *uCharBuforigp;
    const int32_t targetCapacity = (int32_t) (targetLimitp - outbufp);

    uErrorCode = U_ZERO_ERROR;
    ucnv_fromUnicode(uConverterTop,
		     &outbufp,
		     targetLimitp,
		     &sourcep,
		     sourceLimitp,
		     NULL,
		     flushb,
		     &uErrorCode);

    if (U_FAILURE(uErrorCode)) {
      if (uErrorCode == U_BUFFER_OVERFLOW_ERROR) {
	/* POSIX compliance: input buffer should point just prior the */
	/* overflow, as well as output buffer. But the from converter */
	/* is stateful. This is why we kept in a very small buffer of */
	/* the last character that got completely plus its trailing   */
	/* for the from converter.                                    */
	/* The to converter does not need such thing, because it is   */
	/* always working on well-formed full UChar sequences. Reset  */
	/* on it is done for safety measure, I believe it is even not */
	/* necessary.                                                 */
	ucnv_reset(contextp->uConverterTop);
	ucnv_reset(contextp->uConverterFromp);
	if (contextp->charTmpp != NULL) {
	  /* Content of the internal UChar buffer is not an issue     */
	  UChar       *targetp      = contextp->uCharBufp;
	  const UChar *targetLimitp = (const UChar *) contextp->uCharBufLimitp;
	  const char  *sourcep      = (const char*) contextp->charTmpp;
	  const char  *sourceLimitp = (const char*) (sourcep + contextp->charTmpCapacityl);

	  /* Replay the last successful char transform and its trail  */
	  /* data.                                                    */
	  /* It is not expected to fail, definitely.                  */
	  uErrorCode = U_ZERO_ERROR;
	  ucnv_toUnicode(uConverterFromp,
			 &targetp,
			 targetLimitp,
			 &sourcep,
			 sourceLimitp,
			 NULL,
			 FALSE,
			 &uErrorCode);
	  /* This must not fail, isn'it */
	  if (U_FAILURE(uErrorCode)) {
	    errno = ENOSYS;
	    goto err;
	  }
	  /* Explicitely return -1 with E2BIG */
	  errno = E2BIG;
	  return (size_t)-1;
	}
      } else {
	errno = ENOSYS;
	goto err;
      }
    } else {
      outbytesleftl = targetLimitp - outbufp;

      /* Keep a copy of the last source characters that is ok plus */
      /* its trailing data.                                        */
      lastOkCharp      = (const char *) (inbufOrigp + fromOffsetp[uLengthl - 1]);
      charTmpCapacityl = (int32_t) (inbufLimitp - lastOkCharp);

      if (charTmpCapacityl > contextp->charTmpCapacityl) {
	if (_increaseCharTmpBuffer(contextp, charTmpCapacityl) == FALSE) {
	  goto err;
	}
      }
      memcpy(contextp->charTmpp, lastOkCharp, charTmpCapacityl);
    }
  }

  if (uLengthl >= 0) {
    if (firstb == TRUE) {
      contextp->firstb = FALSE;
    }
  }

  *inbufpp        = (char *) inbufp;
  *inbytesleftlp  = inbytesleftl;
  *outbufpp       = outbufp;
  *outbytesleftlp = outbytesleftl;
    
  return uLengthl;

 err:
  if (U_FAILURE(uErrorCode)) {
    tconv_error_set(tconvp, u_errorName(uErrorCode));
  }
  return (size_t)-1;
}

/*****************************************************************************/
int tconv_convert_ICU_free(tconv_t tconvp, void *voidp)
/*****************************************************************************/
{
  static const char            funcs[]  = "tconv_convert_ICU_free";
  tconv_convert_ICU_context_t *contextp = (tconv_convert_ICU_context_t *) voidp;

  if (contextp == NULL) {
    errno = EINVAL;
    goto err;
  } else {
    if (contextp->uConverterFromp != NULL) {
      ucnv_close(contextp->uConverterFromp);
    }
    if (contextp->uCharBufp != NULL) {
      free(contextp->uCharBufp);
    }
    if (contextp->charTmpp != NULL) {
      free(contextp->charTmpp);
    }
    if (contextp->fromOffsetp != NULL) {
      free(contextp->fromOffsetp);
    }
    if (contextp->uConverterTop != NULL) {
      ucnv_close(contextp->uConverterTop);
    }
#if !UCONFIG_NO_TRANSLITERATION
    if (contextp->chunkp != NULL) {
      free(contextp->chunkp);
    }
    if (contextp->chunkCopyp != NULL) {
      free(contextp->chunkCopyp);
    }
    if (contextp->uTransliteratorp != NULL) {
      utrans_close(contextp->uTransliteratorp);
    }
    free(contextp);
#endif
  }

  return 0;

 err:
  return -1;
}

#if !UCONFIG_NO_TRANSLITERATION
/* 
   Note from http://userguide.icu-project.org/strings :
   Endianness is not an issue on this level because the interpretation of an integer is fixed within any given platform.
*/
static const UChar paraEnds[] = {
  0xd, 0xa, 0x85, 0x2028, 0x2029
};
enum {
  iCR = 0, iLF, iNL, iLS, iPS, iCount
};

/*****************************************************************************/
static TCONV_C_INLINE int32_t _getChunkLimit(const UChar *prevp, int32_t prevlenl, const UChar *p, int32_t lengthl)
/*****************************************************************************/
{
  const UChar *up     = p;
  const UChar *limitp = p + lengthl;
  UChar        c;
  /*
    find one of
    CR, LF, CRLF, NL, LS, PS
    for paragraph ends (see UAX #13/Unicode 4)
    and include it in the chunk
    all of these characters are on the BMP
    do not include FF or VT in case they are part of a paragraph
    (important for bidi contexts)
  */
  /* first, see if there is a CRLF split between prevp and p */
  if ((prevlenl > 0) && (prevp[prevlenl - 1] == paraEnds[iCR])) {
    if ((lengthl > 0) && (p[0] == paraEnds[iLF])) {
      return 1; /* split CRLF, include the LF */
    } else if (lengthl > 0) {
      return 0; /* complete the last chunk */
    } else {
      return -1; /* wait for actual further contents to arrive */
    }
  }

  while (up < limitp) {
    c = *up++;
    if (
        ((c < uSP) && (c == uCR || c == uLF)) ||
        (c == uNL) ||
        ((c & uLS) == uLS)
        ) {
      if (c == uCR) {
        /* check for CRLF */
        if (up == limitp) {
          return -1; /* LF may be in the next chunk */
        } else if (*up == uLF) {
          ++up; /* include the LF in this chunk */
        }
      }
      return (int32_t)(up - p); /* In units of UChar */
    }
  }

  return -1; /* continue collecting the chunk */
}

/*****************************************************************************/
static TCONV_C_INLINE int32_t _cnvSigType(UConverter *uConverterp)
/*****************************************************************************/
/* Note: it is guaranteed that _cnvSigType() is called for a converter       */
/* before it is used to effectively convert data.                            */
/*****************************************************************************/
{
  UErrorCode uErrorcode;
  int32_t result;

  /* test if the output charset can convert U+FEFF */
  USet *set = uset_open(1, 0);

  uErrorcode = U_ZERO_ERROR;
  ucnv_getUnicodeSet(uConverterp, set, UCNV_ROUNDTRIP_SET, &uErrorcode);
  if (U_SUCCESS(uErrorcode) && uset_contains(set, uSig)) {
    result = CNV_WITH_FEFF;
  } else {
    result = CNV_NO_FEFF; /* an error occurred or U+FEFF cannot be converted */
  }
  uset_close(set);

  if (result == CNV_WITH_FEFF) {
    /* test if the output charset emits a signature anyway */
    const UChar a[1] = { 0x61 }; /* "a" */
    const UChar *in;

    char buffer[20];
    char *out;

    in = a;
    out = buffer;
    uErrorcode = U_ZERO_ERROR;
    ucnv_fromUnicode(uConverterp,
                     &out,
		     buffer + sizeof(buffer),
                     &in,
		     a + 1,
                     NULL,
		     TRUE,
		     &uErrorcode);
    ucnv_resetFromUnicode(uConverterp);

    if (NULL != ucnv_detectUnicodeSignature(buffer, (int32_t)(out - buffer), NULL, &uErrorcode) &&
        U_SUCCESS(uErrorcode)
        ) {
      result = CNV_ADDS_FEFF;
    }
  }

  return result;
}

/*****************************************************************************/
static TCONV_C_INLINE UBool _increaseChunkBuffer(tconv_convert_ICU_context_t *contextp, int32_t chunkCapacityl)
/*****************************************************************************/
{
  /* + 1 for the eventual signature */
  size_t chunkSizel = (chunkCapacityl + 1) * sizeof(UChar);
  UChar *chunkp     = contextp->chunkp;
  UChar *chunkCopyp = contextp->chunkCopyp;

  chunkp     = (chunkp     == NULL) ? (UChar *) malloc(chunkSizel) : (UChar *) realloc(chunkp,     chunkSizel);
  chunkCopyp = (chunkCopyp == NULL) ? (UChar *) malloc(chunkSizel) : (UChar *) realloc(chunkCopyp, chunkSizel);

  if ((chunkp == NULL) || (chunkCopyp == NULL)) {
    return FALSE;
  }
  contextp->chunkp         = chunkp;
  contextp->chunkCopyp     = chunkCopyp;
  contextp->chunkCapacityl = chunkCapacityl;

  return TRUE;
}

/*****************************************************************************/
static TCONV_C_INLINE UBool   _increaseCharTmpBuffer(tconv_convert_ICU_context_t *contextp, int32_t charTmpCapacityl)
/*****************************************************************************/
{
  size_t charTmpSizel = charTmpCapacityl * sizeof(char);
  char  *charTmpp     = contextp->charTmpp;

  charTmpp = (charTmpp == NULL) ? (char *) malloc(charTmpSizel) : (char *) realloc(charTmpp, charTmpSizel);

  if (charTmpp == NULL) {
    return FALSE;
  }
  contextp->charTmpp         = charTmpp;
  contextp->charTmpCapacityl = charTmpCapacityl;

  return TRUE;
}

#endif /* !UCONFIG_NO_TRANSLITERATION */
