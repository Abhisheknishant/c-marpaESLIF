#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <genericLogger.h>
#include <stdarg.h>
#include "tconv.h"
#include "tconv_config.h"

/* Maximum size for last error. */
#ifndef TCONV_ERROR_SIZE
#define TCONV_ERROR_SIZE 1024
#endif

/* For logging */
#undef  TCONV_ENV_TRACE
#define TCONV_ENV_TRACE "TCONV_ENV_TRACE"
static inline void _tconvTraceCallbackProxy(void *userDatavp, genericLoggerLevel_t logLeveli, const char *msgs);

/* For options */
#undef  TCONV_ENV_CHARSET
#define TCONV_ENV_CHARSET      "TCONV_ENV_CHARSET"
#undef  TCONV_ENV_CHARSET_NEW
#define TCONV_ENV_CHARSET_NEW  "TCONV_ENV_CHARSET_NEW"
#undef  TCONV_ENV_CHARSET_RUN
#define TCONV_ENV_CHARSET_RUN  "TCONV_ENV_CHARSET_RUN"
#undef  TCONV_ENV_CHARSET_FREE
#define TCONV_ENV_CHARSET_FREE "TCONV_ENV_CHARSET_FREE"
#undef  TCONV_ENV_CONVERT
#define TCONV_ENV_CONVERT      "TCONV_ENV_CONVERT"
#undef  TCONV_ENV_CONVERT_NEW
#define TCONV_ENV_CONVERT_NEW  "TCONV_ENV_CONVERT_NEW"
#undef  TCONV_ENV_CONVERT_RUN
#define TCONV_ENV_CONVERT_RUN  "TCONV_ENV_CONVERT_RUN"
#undef  TCONV_ENV_CONVERT_FREE
#define TCONV_ENV_CONVERT_FREE "TCONV_ENV_CONVERT_FREE"

/* Internal structure */
struct tconv {
  /* 1. trace */
  short                    traceb;
  tconvTraceCallback_t     traceCallbackp;
  void                    *traceUserDatavp;
  genericLogger_t         *genericLoggerp;
  /* 2. encodings */
  char                    *tocodes;
  char                    *fromcodes;
  /* 3. runtime */
  void                    *charsetContextp;
  void                    *convertContextp;
  /* 4. for cleanup */
  void                    *sharedLibraryHandlep;
  /* 5. options - we always end up in an "external"-like configuration */
  tconv_charset_external_t charsetExternal;
  tconv_convert_external_t convertExternal;
  /* 6. last error */
  char                     errors[TCONV_ERROR_SIZE];
  /* 7. fuzzy state */
  short                    fuzzyb;
};

#ifndef TCONV_HELPER_BUFSIZ
#define TCONV_HELPER_BUFSIZ 4096 /* Why ? Why not -; */
#endif

struct tconv_helper {
  tconv_t           tconvp;
  void             *contextp;
  tconv_producer_t  producerp;
  tconv_consumer_t  consumerp;
};

#undef  TCONV_MAX
#define TCONV_MAX(tconvp, literalA, literalB) (((literalA) > (literalB)) ? (literalA) : (literalB))
#undef  TCONV_MIN
#define TCONV_MIN(tconvp, literalA, literalB) (((literalA) < (literalB)) ? (literalA) : (literalB))

/* All our functions have an err label if necessary */
#undef  TCONV_FREE
#define TCONV_FREE(tconvp, funcs, ptr) do {		\
    TCONV_TRACE((tconvp), "%s - free(%p)", (funcs), (ptr));		\
    free(ptr);								\
    (ptr) = NULL;							\
  } while (0)

#undef  TCONV_MALLOC
#define TCONV_MALLOC(tconvp, funcs, ptr, type, size) do {		\
    TCONV_TRACE((tconvp), "%s - malloc(%lld)", (funcs), (unsigned long long) (size)); \
    (ptr) = (type) malloc(size);					\
    if ((ptr) == NULL) {						\
      TCONV_TRACE((tconvp), "%s - malloc(%lld) failure, %s", (funcs), (unsigned long long) (size), strerror(errno)); \
      goto err;								\
    } else {								\
      TCONV_TRACE((tconvp), "%s - malloc(%lld) success: %p", (funcs), (unsigned long long) (size), (ptr)); \
    }									\
  } while (0)

#undef  TCONV_REALLOC
#define TCONV_REALLOC(tconvp, funcs, ptr, type, size) do {		\
    type tmp;								\
    TCONV_TRACE((tconvp), "%s - realloc(%p, %lld)", (funcs), (ptr), (unsigned long long) (size)); \
    tmp = (type) realloc((ptr), (size));				\
    if (tmp == NULL) {							\
      TCONV_TRACE((tconvp), "%s - realloc(%p, %lld) failure, %s", (funcs), (ptr), (unsigned long long) (size), strerror(errno)); \
      goto err;								\
    } else {								\
      TCONV_TRACE((tconvp), "%s - realloc(%p, %lld) success: %p", (funcs), (ptr), (unsigned long long) (size), (ptr)); \
      (ptr) = tmp;							\
    }									\
  } while (0)

#undef  TCONV_STRDUP
#define TCONV_STRDUP(tconvp, funcs, dst, src) do {			\
    TCONV_TRACE((tconvp), "%s - strdup(\"%s\")", (funcs), (src));	\
    (dst) = strdup(src);						\
    if ((dst) == NULL) {						\
      TCONV_TRACE((tconvp), "%s - strdup(\"%s\") failure, %s", (funcs), (src), strerror(errno)); \
      goto err;								\
    } else {								\
      TCONV_TRACE((tconvp), "%s - strdup(\"%s\") success: %p", (funcs), (src), (dst)); \
    }									\
  } while (0)

static inline short _tconvDefaultCharsetAndConvertOptions(tconv_t tconvp);
static inline short _tconvDefaultCharsetOption(tconv_t tconvp, tconv_charset_external_t *tconvCharsetExternalp);
static inline short _tconvDefaultConvertOption(tconv_t tconvp, tconv_convert_external_t *tconvConvertExternalp);

/****************************************************************************/
tconv_t tconv_open(const char *tocodes, const char *fromcodes)
/****************************************************************************/
{
  return tconv_open_ext(tocodes, fromcodes, NULL);
}

/****************************************************************************/
void tconv_trace_on(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[] = "tconv_trace_on";
  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (tconvp != NULL) {
    tconvp->traceb = 1;
  }
}

/****************************************************************************/
void tconv_trace_off(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[] = "tconv_trace_off";
  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (tconvp != NULL) {
    tconvp->traceb = 0;
  }
}

/****************************************************************************/
void tconv_trace(tconv_t tconvp, const char *fmts, ...)
/****************************************************************************/
{
  va_list ap;
  int     errnol;
  
  if ((tconvp != NULL) && (tconvp->traceb != 0) && (tconvp->genericLoggerp != NULL)) {
    /* In any case we do not want errno to change when doing logging */
    errnol = errno;
    va_start(ap, fmts);
    GENERICLOGGER_LOGAP(tconvp->genericLoggerp, GENERICLOGGER_LOGLEVEL_TRACE, fmts, ap);
    va_end(ap);
    errno = errnol;
  }
}

/****************************************************************************/
int tconv_close(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[] = "tconv_close";
  int               rci     = 0;
  genericLogger_t  *genericLoggerp;

  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (tconvp != NULL) {
    if (tconvp->charsetContextp != NULL) {
      if (tconvp->charsetExternal.tconv_charset_freep != NULL) {
        TCONV_TRACE(tconvp, "%s - freeing charset engine: %p(%p, %p)", funcs, tconvp->charsetExternal.tconv_charset_freep, tconvp, tconvp->charsetContextp);
        tconvp->charsetExternal.tconv_charset_freep(tconvp, tconvp->charsetContextp);
      }
    }
    if (tconvp->convertContextp != NULL) {
      if (tconvp->convertExternal.tconv_convert_freep != NULL) {
        TCONV_TRACE(tconvp, "%s - freeing convert engine: %p(%p, %p)", funcs, tconvp->convertExternal.tconv_convert_freep, tconvp, tconvp->convertContextp);
        tconvp->convertExternal.tconv_convert_freep(tconvp, tconvp->convertContextp);
      }
    }
    if (tconvp->tocodes != NULL) {
      TCONV_TRACE(tconvp, "%s - freeing copy of \"to\" charset %s", funcs, tconvp->tocodes);
      TCONV_FREE(tconvp, funcs, tconvp->tocodes);
    }
    if (tconvp->fromcodes != NULL) {
      TCONV_TRACE(tconvp, "%s - freeing copy of \"from\" charset %s", funcs, tconvp->fromcodes);
      TCONV_FREE(tconvp, funcs, tconvp->fromcodes);
    }
    if (tconvp->sharedLibraryHandlep != NULL) {
      TCONV_TRACE(tconvp, "%s - closing shared library: dlclose(%p)", funcs, tconvp->sharedLibraryHandlep);
      if (dlclose(tconvp->sharedLibraryHandlep) != 0) {
        TCONV_TRACE(tconvp, "%s - dlclose failure, %s", funcs, dlerror());
        errno = EFAULT;
        rci = -1;
      }
    }
    /* Remember the logger to log the maxiumum as possible */
    genericLoggerp = tconvp->genericLoggerp;
#ifndef TCONV_NTRACE
    if (genericLoggerp != NULL) {
      TCONV_TRACE(tconvp, "%s - prepared to free genericLogger %p", funcs, genericLoggerp);
    }
#endif
    TCONV_TRACE(tconvp, "%s - freeing tconv context", funcs);
    TCONV_FREE(tconvp, funcs, tconvp);

    if (genericLoggerp != NULL) {
      GENERICLOGGER_FREE(genericLoggerp);
    }
  }

  return rci;
}

/****************************************************************************/
tconv_t tconv_open_ext(const char *tocodes, const char *fromcodes, tconv_option_t *tconvOptionp)
/****************************************************************************/
{
  static const char funcs[]              = "tconv_open_ext";
  tconv_t           tconvp               = NULL;
  char             *traces               = NULL;
  char             *charset_news         = NULL;
  char             *charset_runs         = NULL;
  char             *charset_frees        = NULL;
  char             *convert_news         = NULL;
  char             *convert_runs         = NULL;
  char             *convert_frees        = NULL;

  /* The very initial malloc cannot be done with TCONV_MALLOC */
  tconvp = (tconv_t) malloc(sizeof(struct tconv));
  if (tconvp == NULL) {
    goto err;
  }
  /* Make sure everything is initialized NOW regardless of what is coded after */
  /* 1. trace */
  tconvp->traceb          = 0;
  tconvp->traceCallbackp  = NULL;
  tconvp->traceUserDatavp = NULL;
  tconvp->genericLoggerp  = NULL;
  /* 2. encodings */
  tconvp->tocodes   = NULL;
  tconvp->fromcodes = NULL;
  /* 3. runtime */
  tconvp->charsetContextp = NULL;
  tconvp->convertContextp = NULL;
  /* 4. for cleanup */
  tconvp->sharedLibraryHandlep = NULL;
  /* 5. options - we always end up in an "external"-like configuration */
  /* charsetExternal and convertExternal do not contain malloc thingies */
  /* 6. last error */
  tconvp->errors[0] = '\0';
  /* Last byte can never change, because we do an strncpy */
  tconvp->errors[TCONV_ERROR_SIZE - 1] = '\0';
  /* 7. fuzzy state */
  tconvp->fuzzyb = 0;

  /* 1. trace */
  traces                       = getenv(TCONV_ENV_TRACE);
  tconvp->traceb               = (traces != NULL) ? (atoi(traces) != 0 ? 1 : 0) : 0;
  if (tconvOptionp != NULL) {
    tconvp->traceCallbackp  = tconvOptionp->traceCallbackp;
    tconvp->traceUserDatavp = tconvOptionp->traceUserDatavp;
    tconvp->genericLoggerp  = genericLogger_newp(_tconvTraceCallbackProxy, tconvp, GENERICLOGGER_LOGLEVEL_TRACE);
  } else {
    tconvp->traceCallbackp  = NULL;
    tconvp->traceUserDatavp = NULL;
    tconvp->genericLoggerp  = NULL;
  }

  /* From now on, we can log */
#ifndef TCONV_NTRACE
  if (tconvp->genericLoggerp != NULL) {
    TCONV_TRACE(tconvp, "%s - genericLogger created: %p ", funcs, tconvp->genericLoggerp);
  }
#endif
  TCONV_TRACE(tconvp, "%s - trace flag set to %d ", funcs, (int) tconvp->traceb);

  /* 2. encodings */
  if (tocodes != NULL) {
    TCONV_STRDUP(tconvp, funcs, tconvp->tocodes, tocodes);
  } else {
    tconvp->tocodes = NULL;
  }
  if (fromcodes != NULL) {
    TCONV_STRDUP(tconvp, funcs, tconvp->fromcodes, fromcodes);
  } else {
    tconvp->fromcodes = NULL;
  }

  /* 3. runtime */
  tconvp->charsetContextp      = NULL;
  tconvp->convertContextp      = NULL;

   /* 4. for cleanup */
  tconvp->sharedLibraryHandlep = NULL;

  /* 5. options - we always end up in an "external"-like configuration */
  if (tconvOptionp != NULL) {

    TCONV_TRACE(tconvp, "%s - validating user option", funcs);

    /* Charset */
    if (tconvOptionp->charsetp != NULL) {

      switch (tconvOptionp->charsetp->charseti) {
      case TCONV_CHARSET_EXTERNAL:
        TCONV_TRACE(tconvp, "%s - charset detector type is external", funcs);
        tconvp->charsetExternal = tconvOptionp->charsetp->u.external;
        break;
      case TCONV_CHARSET_PLUGIN:
        TCONV_TRACE(tconvp, "%s - charset detector type is plugin", funcs);
        if (tconvOptionp->charsetp->u.plugin.filenames == NULL) {
          TCONV_TRACE(tconvp, "%s - null charset plugin filename", funcs);
          errno = EINVAL;
          goto err;
        }
        TCONV_TRACE(tconvp, "%s - opening shared library %s", funcs, tconvOptionp->charsetp->u.plugin.filenames);
        tconvp->sharedLibraryHandlep = dlopen(tconvOptionp->charsetp->u.plugin.filenames, RTLD_LAZY);
        if (tconvp->sharedLibraryHandlep == NULL) {
          TCONV_TRACE(tconvp, "%s - dlopen failure, %s", funcs, dlerror());
          errno = EINVAL;
          goto err;
        }
	if ((charset_news = tconvOptionp->charsetp->u.plugin.news) == NULL) {
	  if ((charset_news = getenv(TCONV_ENV_CHARSET_NEW)) == NULL) {
	    charset_news = "tconv_charset_newp";
	  }
	}
	if ((charset_runs = tconvOptionp->charsetp->u.plugin.runs) == NULL) {
	  if ((charset_runs = getenv(TCONV_ENV_CHARSET_RUN)) == NULL) {
	    charset_runs = "tconv_charset_runp";
	  }
	}
	if ((charset_frees = tconvOptionp->charsetp->u.plugin.frees) == NULL) {
	  if ((charset_frees = getenv(TCONV_ENV_CHARSET_FREE)) == NULL) {
	    charset_frees = "tconv_charset_freep";
	  }
	}
        tconvp->charsetExternal.tconv_charset_newp  = dlsym(tconvp->sharedLibraryHandlep, charset_news);
        tconvp->charsetExternal.tconv_charset_runp  = dlsym(tconvp->sharedLibraryHandlep, charset_runs);
        tconvp->charsetExternal.tconv_charset_freep = dlsym(tconvp->sharedLibraryHandlep, charset_frees);
        tconvp->charsetExternal.optionp             = tconvOptionp->charsetp->u.plugin.optionp;
        break;
      case TCONV_CHARSET_ICU:
#ifdef TCONV_HAVE_ICU
        TCONV_TRACE(tconvp, "%s - charset detector type is built-in ICU", funcs);
        tconvp->charsetExternal.tconv_charset_newp  = tconv_charset_ICU_new;
        tconvp->charsetExternal.tconv_charset_runp  = tconv_charset_ICU_run;
        tconvp->charsetExternal.tconv_charset_freep = tconv_charset_ICU_free;
        tconvp->charsetExternal.optionp             = tconvOptionp->charsetp->u.cchardetOptionp;
#endif
        break;
      case TCONV_CHARSET_CCHARDET:
        TCONV_TRACE(tconvp, "%s - charset detector type is built-in cchardet", funcs);
        tconvp->charsetExternal.tconv_charset_newp  = tconv_charset_cchardet_new;
        tconvp->charsetExternal.tconv_charset_runp  = tconv_charset_cchardet_run;
        tconvp->charsetExternal.tconv_charset_freep = tconv_charset_cchardet_free;
        tconvp->charsetExternal.optionp             = tconvOptionp->charsetp->u.ICUOptionp;
        break;
      default:
        TCONV_TRACE(tconvp, "%s - charset detector type is unknown", funcs);
        tconvp->charsetExternal.tconv_charset_newp  = NULL;
        tconvp->charsetExternal.tconv_charset_runp  = NULL;
        tconvp->charsetExternal.tconv_charset_freep = NULL;
        tconvp->charsetExternal.optionp             = NULL;
        break;
      }
      if (tconvp->charsetExternal.tconv_charset_runp == NULL) {
        /* Formally, only the "run" entry point is required */
        TCONV_TRACE(tconvp, "%s - tconv_charset_runp is NULL", funcs);
        errno = EINVAL;
        goto err;
      }
    } else {    
      TCONV_TRACE(tconvp, "%s - setting default charset options", funcs);
      if (_tconvDefaultCharsetOption(tconvp, &(tconvp->charsetExternal)) == 0) {
        goto err;
      }
    }
    if (tconvOptionp->convertp != NULL) {
      switch (tconvOptionp->convertp->converti) {
      case TCONV_CONVERT_EXTERNAL:
        TCONV_TRACE(tconvp, "%s - converter type is external", funcs);
        tconvp->convertExternal = tconvOptionp->convertp->u.external;
        break;
      case TCONV_CONVERT_PLUGIN:
        TCONV_TRACE(tconvp, "%s - converter type is plugin", funcs);
        if (tconvOptionp->convertp->u.plugin.filenames == NULL) {
          TCONV_TRACE(tconvp, "%s - null convert filename", funcs);
          errno = EINVAL;
          goto err;
        }
        TCONV_TRACE(tconvp, "%s - opening shared library %s", funcs, tconvOptionp->convertp->u.plugin.filenames);
        tconvp->sharedLibraryHandlep = dlopen(tconvOptionp->convertp->u.plugin.filenames, RTLD_LAZY);
        if (tconvp->sharedLibraryHandlep == NULL) {
          TCONV_TRACE(tconvp, "%s - dlopen failure, %s", funcs, dlerror());
          errno = EINVAL;
          goto err;
        }
	if ((convert_news = tconvOptionp->convertp->u.plugin.news) == NULL) {
	  if ((convert_news = getenv(TCONV_ENV_CONVERT_NEW)) == NULL) {
	    convert_news = "tconv_convert_newp";
	  }
	}
	if ((convert_runs = tconvOptionp->convertp->u.plugin.runs) == NULL) {
	  if ((convert_runs = getenv(TCONV_ENV_CONVERT_RUN)) == NULL) {
	    convert_runs = "tconv_convert_runp";
	  }
	}
	if ((convert_frees = tconvOptionp->convertp->u.plugin.frees) == NULL) {
	  if ((convert_frees = getenv(TCONV_ENV_CONVERT_FREE)) == NULL) {
	    convert_frees = "tconv_convert_freep";
	  }
	}
        tconvp->convertExternal.tconv_convert_newp  = dlsym(tconvp->sharedLibraryHandlep, convert_news);
        tconvp->convertExternal.tconv_convert_runp  = dlsym(tconvp->sharedLibraryHandlep, convert_runs);
        tconvp->convertExternal.tconv_convert_freep = dlsym(tconvp->sharedLibraryHandlep, convert_frees);
        tconvp->convertExternal.optionp             = tconvOptionp->convertp->u.plugin.optionp;
        break;
      case TCONV_CONVERT_ICU:
#ifdef TCONV_HAVE_ICU
        TCONV_TRACE(tconvp, "%s - converter type is built-in ICU", funcs);
        tconvp->convertExternal.tconv_convert_newp  = tconv_convert_ICU_new;
        tconvp->convertExternal.tconv_convert_runp  = tconv_convert_ICU_run;
        tconvp->convertExternal.tconv_convert_freep = tconv_convert_ICU_free;
        tconvp->convertExternal.optionp             = tconvOptionp->convertp->u.ICUOptionp;
#endif
        break;
      case TCONV_CONVERT_ICONV:
#ifdef TCONV_HAVE_ICONV
        TCONV_TRACE(tconvp, "%s - converter type is built-in iconv", funcs);
        tconvp->convertExternal.tconv_convert_newp  = tconv_convert_iconv_new;
        tconvp->convertExternal.tconv_convert_runp  = tconv_convert_iconv_run;
        tconvp->convertExternal.tconv_convert_freep = tconv_convert_iconv_free;
        tconvp->convertExternal.optionp             = tconvOptionp->convertp->u.iconvOptionp;
#endif
        break;
      default:
        TCONV_TRACE(tconvp, "%s - converter type is unknown", funcs);
        tconvp->convertExternal.tconv_convert_newp  = NULL;
        tconvp->convertExternal.tconv_convert_runp  = NULL;
        tconvp->convertExternal.tconv_convert_freep = NULL;
        tconvp->convertExternal.optionp             = NULL;
        break;
      }
      if (tconvp->convertExternal.tconv_convert_runp == NULL) {
        /* Formally, only the "run" entry point is required */
        TCONV_TRACE(tconvp, "%s - tconv_convert_runp is NULL", funcs);
        errno = EINVAL;
        goto err;
      }
    } else {    
      TCONV_TRACE(tconvp, "%s - setting default converter options", funcs);
      if (_tconvDefaultConvertOption(tconvp, &(tconvp->convertExternal)) == 0) {
        goto err;
      }
    }
  } else {
    if (_tconvDefaultCharsetAndConvertOptions(tconvp) == 0) {
      goto err;
    }
  }

  TCONV_TRACE(tconvp, "%s - return %p", funcs, tconvp);
  return tconvp;
  
 err:
  {
    int errnol = errno;
    tconv_close(tconvp);
    errno = errnol;
  }
  return (tconv_t)-1;
}

/****************************************************************************/
size_t tconv(tconv_t tconvp, char **inbufsp, size_t *inbytesleftlp, char **outbufsp, size_t *outbytesleftlp)
/****************************************************************************/
{
  static const char funcs[]         = "tconv";
  void             *charsetContextp = NULL;
  void             *charsetOptionp  = NULL;
  char             *fromcodes       = NULL;
  void             *convertContextp = NULL;
  void             *convertOptionp  = NULL;
  size_t            rcl;

  TCONV_TRACE(tconvp, "%s(%p, %p, %p, %p, %p)", funcs, tconvp, inbufsp, inbytesleftlp, outbufsp, outbytesleftlp);

  if (tconvp == NULL) {
    errno = EINVAL;
    goto err;
  }

  if ((tconvp->fromcodes == NULL) &&
      (inbufsp != NULL)           &&
      (*inbufsp != NULL)          &&
      (inbytesleftlp != NULL)     &&
      (*inbytesleftlp > 0)) {
    
    /* it is legal to have no new() for charset engine, but if there is one */
    /* it must return something */
    if (tconvp->charsetExternal.tconv_charset_newp != NULL) {
      charsetOptionp = tconvp->charsetExternal.optionp;
      TCONV_TRACE(tconvp, "%s - initializing charset detection: %p(%p, %p)", funcs, tconvp->charsetExternal.tconv_charset_newp, tconvp, charsetOptionp);
      tconvp->errors[0] = '\0';
      charsetContextp = tconvp->charsetExternal.tconv_charset_newp(tconvp, charsetOptionp);
      if (charsetContextp == NULL) {
	goto err;
      }
    }
    tconvp->charsetContextp = charsetContextp;
    TCONV_TRACE(tconvp, "%s - calling charset detection: %p(%p, %p, %p, %p)", funcs, tconvp->charsetExternal.tconv_charset_runp, tconvp, charsetContextp, *inbufsp, *inbytesleftlp);
    tconvp->errors[0] = '\0';
    fromcodes = tconvp->charsetExternal.tconv_charset_runp(tconvp, charsetContextp, *inbufsp, *inbytesleftlp);
    if (fromcodes == NULL) {
      goto err;
    }
    TCONV_TRACE(tconvp, "%s - charset detection returned %s", funcs, fromcodes);
    TCONV_STRDUP(tconvp, funcs, tconvp->fromcodes, fromcodes);
    strcpy(tconvp->fromcodes, fromcodes);
    if (tconvp->charsetExternal.tconv_charset_freep != NULL) {
      TCONV_TRACE(tconvp, "%s - ending charset detection: %p(%p, %p)", funcs, tconvp->charsetExternal.tconv_charset_freep, tconvp, charsetContextp);
      tconvp->charsetExternal.tconv_charset_freep(tconvp, charsetContextp);
    }
    tconvp->charsetContextp = NULL;
  }

  if ((tconvp->tocodes == NULL) && (tconvp->fromcodes != NULL)) {
    TCONV_TRACE(tconvp, "%s - duplicating from charset \"%s\" into \"to\" charset", funcs, tconvp->fromcodes);
    TCONV_STRDUP(tconvp, funcs, tconvp->tocodes, tconvp->fromcodes);
  }

  if (tconvp->convertContextp == NULL) {
    /* Initialize converter context if not already done */
    /* it is legal to have no new() for convert engine, but if there is one */
    /* it must return something */
    if (tconvp->convertExternal.tconv_convert_newp != NULL) {
      convertOptionp = tconvp->convertExternal.optionp;
      TCONV_TRACE(tconvp, "%s - initializing convert engine: %p(%p, %p, %p, %p)", funcs, tconvp->convertExternal.tconv_convert_newp, tconvp, tconvp->tocodes, tconvp->fromcodes, convertOptionp);
      tconvp->errors[0] = '\0';
      convertContextp = tconvp->convertExternal.tconv_convert_newp(tconvp, tconvp->tocodes, tconvp->fromcodes, convertOptionp);
      if (convertContextp == (void *)-1) {
	goto err;
      }
    }
    tconvp->convertContextp = convertContextp;
  } else {
    convertContextp = tconvp->convertContextp;
  }

  TCONV_TRACE(tconvp, "%s - calling convert engine: %p(%p, %p, %p, %p, %p, %p)", funcs, tconvp->convertExternal.tconv_convert_runp, tconvp, convertContextp, inbufsp, inbytesleftlp, outbufsp, outbytesleftlp);
  tconvp->errors[0] = '\0';
  rcl = tconvp->convertExternal.tconv_convert_runp(tconvp, convertContextp, inbufsp, inbytesleftlp, outbufsp, outbytesleftlp);
  if (rcl == (size_t)-1) {
    goto err;
  }
  
  TCONV_TRACE(tconvp, "%s - return %lld", funcs, (signed long long) rcl);
  return rcl;

 err:
  if (tconvp->errors[0] == '\0') {
    tconv_error_set(tconvp, strerror(errno));
  }
  return (size_t)-1;
}

/****************************************************************************/
static inline short _tconvDefaultCharsetAndConvertOptions(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[] = "_tconvDefaultCharsetAndConvertOptions";

  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (_tconvDefaultCharsetOption(tconvp, &(tconvp->charsetExternal)) == 0) {
    goto err;
  }
  if (_tconvDefaultConvertOption(tconvp, &(tconvp->convertExternal)) == 0) {
    goto err;
  }

  return 1;

 err:
  errno = EINVAL;
  return 0;
}

/****************************************************************************/
static inline short _tconvDefaultCharsetOption(tconv_t tconvp, tconv_charset_external_t *tconvCharsetExternalp)
/****************************************************************************/
{
  static const char funcs[]  = "_tconvDefaultCharsetOption";
  char             *charsets = getenv(TCONV_ENV_CHARSET);

  TCONV_TRACE(tconvp, "%s(%p, %p)", funcs, tconvp, tconvCharsetExternalp);

  if (charsets == NULL) {
    charsets = "CCHARDET";
  }

  if  (strcmp(charsets, "ICU") == 0) {
#ifdef TCONV_HAVE_ICU
    TCONV_TRACE(tconvp, "%s - setting default charset detector to ICU", funcs);
    tconvCharsetExternalp->optionp             = NULL;
    tconvCharsetExternalp->tconv_charset_newp  = tconv_charset_ICU_new;
    tconvCharsetExternalp->tconv_charset_runp  = tconv_charset_ICU_run;
    tconvCharsetExternalp->tconv_charset_freep = tconv_charset_ICU_free;
#else
    goto err;
#endif
  } else if (strcmp(charsets, "CCHARDET") == 0) {
    TCONV_TRACE(tconvp, "%s - setting default charset detector to cchardet", funcs);
    tconvCharsetExternalp->optionp             = NULL;
    tconvCharsetExternalp->tconv_charset_newp  = tconv_charset_cchardet_new;
    tconvCharsetExternalp->tconv_charset_runp  = tconv_charset_cchardet_run;
    tconvCharsetExternalp->tconv_charset_freep = tconv_charset_cchardet_free;
  } else {
    goto err;
  }

  return 1;

 err:
  TCONV_TRACE(tconvp, "%s - charset detector %s is not available", funcs, charsets);
  errno = ENOSYS;
  return 0;
}

/****************************************************************************/
 static inline short _tconvDefaultConvertOption(tconv_t tconvp, tconv_convert_external_t *tconvConvertExternalp)
/****************************************************************************/
{
  static const char funcs[]  = "_tconvDefaultConvertOption";
  char             *converts = getenv(TCONV_ENV_CONVERT);

  TCONV_TRACE(tconvp, "%s(%p, %p)", funcs, tconvp, tconvConvertExternalp);

  if (converts == NULL) {
#ifdef TCONV_HAVE_ICU
    converts = "ICU";
#else
#  ifdef TCONV_HAVE_ICONV
    converts = "ICONV";
# else
    converts = "<no built-in charset>";
#  endif
#endif
  }

  if (strcmp(converts, "ICU") == 0) {
#ifdef TCONV_HAVE_ICU
    TCONV_TRACE(tconvp, "%s - setting default converter to ICU", funcs);
    tconvConvertExternalp->optionp             = NULL;
    tconvConvertExternalp->tconv_convert_newp  = tconv_convert_ICU_new;
    tconvConvertExternalp->tconv_convert_runp  = tconv_convert_ICU_run;
    tconvConvertExternalp->tconv_convert_freep = tconv_convert_ICU_free;
#else
    goto err;
#endif
  } else if (strcmp(converts, "ICONV") == 0) {
#ifdef TCONV_HAVE_ICONV
    TCONV_TRACE(tconvp, "%s - setting default converter to iconv", funcs);
    tconvConvertExternalp->optionp             = NULL;
    tconvConvertExternalp->tconv_convert_newp  = tconv_convert_iconv_new;
    tconvConvertExternalp->tconv_convert_runp  = tconv_convert_iconv_run;
    tconvConvertExternalp->tconv_convert_freep = tconv_convert_iconv_free;
#else
    goto err;
#endif
  } else {
    goto err;
  }

  return 1;

 err:
  TCONV_TRACE(tconvp, "%s - character converter %s is not available", funcs);
  errno = ENOSYS;
  return 0;
}

/****************************************************************************/
static inline void _tconvTraceCallbackProxy(void *userDatavp, genericLoggerLevel_t logLeveli, const char *msgs)
/****************************************************************************/
{
  tconv_t tconvp = (tconv_t) userDatavp;

  if (tconvp->traceCallbackp != NULL) {
    tconvp->traceCallbackp(tconvp->traceUserDatavp, msgs);
  }
}

/****************************************************************************/
char *tconv_error_set(tconv_t tconvp, const char *msgs)
/****************************************************************************/
{
  static const char funcs[] = "tconv_error_set";
  char             *errors  = NULL;
  int               errnol = errno;  /* Make sure errno is never changed */

  TCONV_TRACE(tconvp, "%s(%p, %p)", funcs, tconvp, msgs);

  if (tconvp != NULL) {
    /* This is making sure that errors[TCONV_ERROR_SIZE - 1] is never touched */
    strncpy(tconvp->errors, msgs, TCONV_ERROR_SIZE - 1);
    errors = tconvp->errors;
  }

#ifndef TCONV_NTRACE
  if (errors != NULL) {
    TCONV_TRACE(tconvp, "%s - return %s", funcs, errors);
  } else {
    TCONV_TRACE(tconvp, "%s - return NULL", funcs);
  }
#endif

  errno = errnol;

  return errors;
}

/****************************************************************************/
char *tconv_error(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[] = "tconv_error";
  char             *errors  = NULL;

  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (tconvp != NULL) {
    errors = tconvp->errors;
  }

#ifndef TCONV_NTRACE
  if (errors != NULL) {
    TCONV_TRACE(tconvp, "%s - return %s", funcs, errors);
  } else {
    TCONV_TRACE(tconvp, "%s - return NULL", funcs);
  }
#endif

  return errors;
}

/****************************************************************************/
char *tconv_fromcode(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[]   = "tconv_fromcode";
  char             *fromcodes = NULL;

  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (tconvp != NULL) {
    fromcodes = tconvp->fromcodes;
  }

#ifndef TCONV_NTRACE
  if (fromcodes != NULL) {
    TCONV_TRACE(tconvp, "%s - return %s", funcs, fromcodes);
  } else {
    TCONV_TRACE(tconvp, "%s - return NULL", funcs);
  }
#endif

  return fromcodes;
}
/****************************************************************************/
char *tconv_tocode(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[]   = "tconv_tocode";
  char             *tocodes = NULL;

  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (tconvp != NULL) {
    tocodes = tconvp->tocodes;
  }

#ifndef TCONV_NTRACE
  if (tocodes != NULL) {
    TCONV_TRACE(tconvp, "%s - return %s", funcs, tocodes);
  } else {
    TCONV_TRACE(tconvp, "%s - return NULL", funcs);
  }
#endif

  return tocodes;
}

/****************************************************************************/
short tconv_fuzzy_set(tconv_t tconvp, short fuzzyb)
/****************************************************************************/
{
  static const char funcs[] = "tconv_fuzzy_set";

  TCONV_TRACE(tconvp, "%s(%p, %d)", funcs, tconvp, (int) fuzzyb);

  if (tconvp != NULL) {
    tconvp->fuzzyb = fuzzyb;
  }

  TCONV_TRACE(tconvp, "%s - return %d", funcs, (int) fuzzyb);
  return fuzzyb;
}

/****************************************************************************/
short tconv_fuzzy(tconv_t tconvp)
/****************************************************************************/
{
  static const char funcs[] = "tconv_fuzzy";
  short             fuzzyb  = 0;

  TCONV_TRACE(tconvp, "%s(%p)", funcs, tconvp);

  if (tconvp != NULL) {
    fuzzyb = tconvp->fuzzyb;
  }

  TCONV_TRACE(tconvp, "%s - return %d", funcs, (int) fuzzyb);

  return fuzzyb;
}

/****************************************************************************/
tconv_helper_t *tconv_helper_newp(char *tocodes, char *fromcodes, void *contextp, tconv_producer_t producerp, tconv_consumer_t consumerp)
/****************************************************************************/
{
  static const char  funcs[] = "tconv_helper_newp";
  tconv_helper_t    *tconv_helperp = NULL;
  char              *toclones = NULL;
  char              *fromclones = NULL;
  tconv_t            tconvp;

  if ((tocodes == NULL) || (fromcodes == NULL) || (producerp == NULL) || (consumerp == NULL)) {
    errno = EINVAL;
    return NULL;
  }

  tconvp = tconv_open(tocodes, fromcodes);
  if (tconvp == NULL) {
    return NULL;
  }

  /* From now on we can use TCONV macros */
  TCONV_MALLOC(tconvp, funcs, tconv_helperp, tconv_helper_t *, sizeof(tconv_helper_t));

  tconv_helperp->tconvp = NULL;
  tconv_helperp->contextp = NULL;
  tconv_helperp->producerp = NULL;
  tconv_helperp->consumerp = NULL;

  tconv_helperp->tconvp = tconv_open(tocodes, fromcodes);
  if (tconv_helperp->tconvp == NULL) {
    goto err;
  }
  tconv_helperp->contextp = contextp;
  tconv_helperp->producerp = producerp;
  tconv_helperp->consumerp = consumerp;

  goto done;

 err:
  tconv_helper_freev(tconv_helperp);
  tconv_helperp = NULL;

 done:
  return tconv_helperp;
}

/****************************************************************************/
tconv_EXPORT void tconv_helper_freev(tconv_helper_t *tconv_helperp)
/****************************************************************************/
{
  if (tconv_helperp != NULL) {
    tconv_close(tconv_helperp->tconvp);
    free(tconv_helperp);
  }
}

/****************************************************************************/
short tconv_helper(tconv_t  tconvp,
                   void    *contextp,
                   short   (*producerp)(void *contextp, char **bufpp, size_t *countlp, short *eofbp),
                   short   (*consumerp)(void *contextp, char *bufp, size_t countl, short eofb, size_t *resultlp)
                   )
/****************************************************************************/
{
  static const char funcs[]       = "tconv_helper";
  /* Local memory management */
  char             *inputp        = NULL;
  char             *outputp       = NULL;
  size_t            inputallocl   = 0;
  size_t            outputallocl; /* Initialized at runtime */
  size_t            inputguardl   = 0;
  size_t            outputguardl; /* Initialized at runtime */
  char             *inputguardp;  /* Always re-evaluated at runtime */
  char             *inputendp;    /* Always re-evaluated at runtime */
  char             *outputguardp; /* Always re-evaluated at runtime */
  char             *outputendp;   /* Always re-evaluated at runtime */
  /* Callbacks output */
  char             *producerbufp;
  size_t            producercountl;
  short             producereofb;
  char             *consumerbufp;
  size_t            consumercountl;
  size_t            consumerresultl;
  /* convertion parameters */
  char             *inbufp;
  size_t            inbufbytesleftl;
  char             *outbufp;
  size_t            outbufbytesleftl;
  /* temporary work variables */
  short             producerb;
  short             consumerb;
  size_t            tconvl;
  char             *tmps;
  size_t            allocl;
  size_t            notusedl;
  size_t            deltal;
  short             rcb;
  short             outputbufmemmovedb;

  TCONV_TRACE(tconvp, "%s(%p, %p, %p, %p)", funcs, tconvp, contextp, producerp, consumerp);

  if ((tconvp == NULL) || (producerp == NULL) || (consumerp == NULL)) {
    errno = EINVAL;
    return 0;
  }

  /* We always create an output buffer first, it is illegal to call tconv() with a null area */
  /* for the storage, with the exception of total reset, which we do not do here.  */
  TCONV_TRACE(tconvp, "%s - initialize output", funcs);
  TCONV_MALLOC(tconvp, funcs, outputp, char *, TCONV_HELPER_BUFSIZ);
  outputallocl = TCONV_HELPER_BUFSIZ;
  outputguardl = 0;
  
  producereofb   = 0;
  do {
    producerbufp   = NULL;
    producercountl = 0;
    TCONV_TRACE(tconvp, "%s - producerp(%p, %p, %p, %p)", funcs, contextp, &producerbufp, &producercountl, &producereofb);
    producerb = producerp(contextp, &producerbufp, &producercountl, &producereofb);
#ifndef TCONV_NTRACE
    if (producerb) {
      TCONV_TRACE(tconvp, "%s - producerp(...) success: producerbufp=%p, producercountl=%ld, producereofb=%d", funcs, producerbufp, (unsigned long) producercountl, (int) producereofb);
    } else {
      TCONV_TRACE(tconvp, "%s - producerp(...) failure", funcs);
    }
#endif
    if (producerb) {
      if ((producerbufp != NULL) && (producercountl > 0)) {
        if (inputp == NULL) {
          /* First time */
          TCONV_TRACE(tconvp, "%s - initialize input", funcs);
          TCONV_MALLOC(tconvp, funcs, inputp, char *, producercountl);
          inputallocl = producercountl;
          memcpy(inputp, producerbufp, producercountl);
          inputguardl = inputallocl;
        } else {
          /* Not the first time - check if there is enough room */
          notusedl = inputallocl - inputguardl;
          if (notusedl < producercountl) {
            deltal = producercountl - notusedl;
            /* Will that really happen in real-life ? Possible in theory, though. */
            allocl = inputallocl + deltal;
            if (allocl < inputallocl) {
              errno = ERANGE;
              goto err;
            }
            TCONV_TRACE(tconvp, "%s - increase input by %lld bytes", funcs, (unsigned long long) deltal);
            TCONV_REALLOC(tconvp, funcs, inputp, char *, allocl);
            inputallocl = allocl;
            memcpy(inputp + inputguardl, producerbufp, producercountl);
            inputguardl = inputallocl;
          } else {
            memcpy(inputp + inputguardl, producerbufp, producercountl);
            inputguardl += producercountl;
          }
        }
#ifndef TCONV_NTRACE
        if (inputguardl < inputallocl) {
          if (inputguardl > 0) {
            TCONV_TRACE(tconvp, "%s - ...... input  buffer is %p[0 ...used... %ld][%ld ...unused... %ld]", funcs, inputp, (unsigned long) (inputguardl - 1), (unsigned long) inputguardl, (unsigned long) inputallocl);
          } else {
            TCONV_TRACE(tconvp, "%s - ...... input  buffer is %p[%ld ...unused... %ld]", funcs, inputp, (unsigned long) inputguardl, (unsigned long) inputguardl, (unsigned long) inputallocl);
          }
        } else {
          TCONV_TRACE(tconvp, "%s - ...... input  buffer is %p[0 ...used... %ld]", funcs, inputp, (unsigned long) inputallocl);
        }
#endif
        inputguardp = inputp + inputguardl;
        inputendp   = inputp + inputallocl;

#ifndef TCONV_NTRACE
        if (outputguardl < outputallocl) {
          if (outputguardl > 0) {
            TCONV_TRACE(tconvp, "%s - ...... output buffer is %p[0 ...used... %ld][%ld ...unused... %ld]", funcs, outputp, (unsigned long) (outputguardl - 1), (unsigned long) outputguardl, (unsigned long) outputallocl);
          } else {
            TCONV_TRACE(tconvp, "%s - ...... output buffer is %p[%ld ...unused... %ld]", funcs, outputp, (unsigned long) outputguardl, (unsigned long) outputallocl);
          }
        } else {
          TCONV_TRACE(tconvp, "%s - ...... output buffer is %p[0 ...used... %ld]", funcs, outputp, (unsigned long) outputallocl);
        }
#endif
        outputguardp = outputp + outputguardl;
        outputendp   = outputp + outputallocl;

        inbufp           = inputp;
        inbufbytesleftl  = inputguardl;

        /*
         * local input buffer is like this:
         *
         * inputp                                     inputguardp              inputendp
         * 0                                          inputguardl              inputallocl
         * ---------------------------------------------------------------------
         * |      used area                           |  unused area           |
         * ---------------------------------------------------------------------
         * inbufp
         * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
         * input area of size inbufbytesleftl
         */
        
        outbufp          = outputguardp;
        outbufbytesleftl = outputallocl - outputguardl;

        /*
         * local output buffer is like this:
         *
         * outputp                                    outputguardp             outputendp
         * 0                                          outputguardl             outputallocl
         * ---------------------------------------------------------------------
         * |      used area                           |  unused area           |
         * ---------------------------------------------------------------------
         * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!outbufp
         * area not consumed by the end user          ^^^^^^^^^^^^^^^^^^^^^^^^^
         *                                            output area of size outbufbytesleftl
         *
         */

        TCONV_TRACE(tconvp, "%s - calling tconv with inbufbytesleftl=%ld and outbufbytesleftl=%ld", funcs, (unsigned long) inbufbytesleftl, (unsigned long) outbufbytesleftl);
        tconvl = tconv(tconvp, &inbufp, &inbufbytesleftl, &outbufp, &outbufbytesleftl);
#ifndef TCONV_NTRACE
        if (tconvl == (size_t)-1) {
          TCONV_TRACE(tconvp, "%s - tconv(...) returned -1, errno %d (%s)", funcs, errno, strerror(errno));
        } else {
          TCONV_TRACE(tconvp, "%s - tconv(...) returned %ld", funcs, (unsigned long) tconvl);
        }
#endif

        /*
         * local output buffer is now like this:
         *
         * outputp                                    outputguardp             outputendp
         * 0                                          outputguardl             outputallocl
         * ---------------------------------------------------------------------
         * |      used area                           | new bytes  |           |
         * ---------------------------------------------------------------------
         * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!outbufp
         * area not consumed by the end user                       ^^^^^^^^^^^^
         *                                                         unconsumed area
         *                                                         of size outbufbytesleftl
         */

        outputbufmemmovedb = 0;
        if (outbufp > outputp) { /* Regardless if new bytes were produced -; */

          /* For now on we do not need to use outbufp variable anymore */
          consumerbufp    = outputp;
          consumercountl  = outputallocl - outbufbytesleftl;
          consumerresultl = 0;
          consumerb = consumerp(contextp, consumerbufp, consumercountl, producereofb, &consumerresultl);
#ifndef TCONV_NTRACE
          if (consumerb) {
            TCONV_TRACE(tconvp, "%s - consumerp(...) success: consumerresultl=%ld", funcs, (unsigned long) consumerresultl);
          } else {
            TCONV_TRACE(tconvp, "%s - producerp(...) consumerp", funcs);
          }
#endif
          if (! consumerb) {
            goto err;
          } else {
            /* Consumer says it has used this number of bytes (can be zero) */
            if (consumerresultl > consumercountl) {
              /* Non sense, consumer says it has used more than what we provided -; */
              errno = ERANGE;
              goto err;
            }
            /*
             * outputp                                outputguardp             outputendp
             * 0                                      outputguardl             outputallocl
             * -----------------------------------------------------------------
             * |      used area                       | new bytes  |           |
             * -----------------------------------------------------------------
             *                    consumerresultl      deltal bytes
             * +++++++++++++++++++!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
             * user said so       area not consumed by the end user^^^^^^^^^^^^
             *                                                     unconsumed area
             *                                                     of size outbufbytesleftl
             */
            deltal = outputallocl - outbufbytesleftl - outputguardl;
            outputguardp += deltal;
            outputguardl += deltal;
            /*
             * outputp                                             outputguardp outputendp
             * 0                                                   outputguardl outputallocl
             * -----------------------------------------------------------------
             * |      used area                                    |           |
             * -----------------------------------------------------------------
             *                    consumerresultl
             * +++++++++++++++++++!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
             * user said so       area not consumed by the end user
             */

            memmove(outputp, outputp + consumerresultl, outputguardl - consumerresultl);
            outputguardp -= consumerresultl;
            outputguardl -= consumerresultl;
            outputbufmemmovedb = 1;

            /*
             * outputp                                    outputguardp             outputendp
             * 0                                          outputguardl             outputallocl
             * ---------------------------------------------------------------------
             * |      used area                           |                        |
             * ---------------------------------------------------------------------
             * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
             * area not consumed by the end user
             *
             */
          }
        }

        /*
         * local input buffer is now like this:
         *
         * inputp                                     inputp               inputp
         * +0                                         +inputguardl         +inputallocl
         * -----------------------------------------------------------------
         * |      consumed area                |      |  unused area       |
         * -----------------------------------------------------------------
         *                                     inbufp
         *                                     ^^^^^^^
         *                                     intentionaly leftover area
         *                                     of size inbufbytesleftl
         *
         */

        if (tconvl == (size_t)-1) {
          if (errno == E2BIG) {
            /* Regardless if we memmoved in the output buffer, handle a resize. */
            /* A resize failure should be fatal unless we get some free bytes via the */
            /* memmove indeed. We consider that fatal in any case -; */
            allocl = outputallocl * 2;
            /* Will that really happen in real-life ? Possible in theory, though. */
            if (allocl < outputallocl) {
              errno = ERANGE;
              goto err;
            }
            TCONV_REALLOC(tconvp, funcs, outputp, char *, allocl);
            outputallocl = allocl;
          } else if (errno == EINVAL) {
            /* Invalid byte sequence at the end of the input buffer */
            /* If inbufp moved this is handled at the end of the "if" on tconvl */
          } else {
            /* Invalid byte sequence or another error */
            goto err;
          }
        }
        /* In any case, move unconsumed bytes to the beginning */
        if ((inbufp > inputp) && (inbufbytesleftl > 0)) {
          TCONV_TRACE(tconvp, "%s - moving %ld unconsumed bytes at the beginning of the input buffer", funcs, (unsigned long) inbufbytesleftl);
          memmove(inputp, inbufp, inbufbytesleftl);
        }
        inputguardl = inbufbytesleftl;
      }
    }
  } while (! producereofb);

  rcb = 1;
  goto done;

 err:
  rcb = 0;

 done:
  if (inputp != NULL) {
    free(inputp);
  }
  if (outputp != NULL) {
    free(outputp);
  }
    
  TCONV_TRACE(tconvp, "%s - return %d", funcs, (int) rcb);

  return rcb;
}
