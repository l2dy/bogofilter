#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_VALUES_H
# include <values.h>
#endif

#ifdef HAVE_FLOAT_H
# include <float.h>
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

int ipmpar( int *i);

int ipmpar( int *i)
{
   static int imach[11] = {
      0, /* dummy zeroth index */
      /* integers */  
      2, /* base;  you are on the binary machine, aren't you? */
      sizeof(int)*(CHAR_BIT)-1,
      INT_MAX,
      /* all floats */
      FLT_RADIX,
      /* single precision floats */
      FLT_MANT_DIG,
      FLT_MIN_EXP,
      FLT_MAX_EXP,
      /* double precision floats */
      DBL_MANT_DIG,
      DBL_MIN_EXP,
      DBL_MAX_EXP,
   };
   return imach[*i];
}
