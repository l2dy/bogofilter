# Makefile for the RISC OS version of gsl


# Define which compiler to use:

CC = cc
#CC = gcc


#########################################
# DO NOT EDIT ANYTHING BELOW THIS LINE! #
#########################################

ifeq (${CC},cc)
CC = cc
LD = link
AR = libfile
DEPEND = -depend !Depend
CC_FLAGS = -Wpc -zo -fw -throwback -Otime -I^,@,Unix: -JUnix: -D__riscos__ -DHAVE_CONFIG_H -zpq262144
UNIXLIB = Unix:o.UnixLib
else
ifeq (${CC},gcc)
CC = gcc
LD = gcc
AR = ar
CC_FLAGS = -mthrowback -O3 -I^ -I@ -D__riscos__ -DHAVE_CONFIG_H
else
# No other compiler supported!
endif
endif

CDF_OBJS = cdf.o.chisq \
	cdf.o.gamma

ERR_OBJS = err.o.error \
	err.o.stream

SPECFUNC_OBJS = specfunc.o.elementary \
	specfunc.o.erfc \
	specfunc.o.exp \
	specfunc.o.expint \
	specfunc.o.gamma_inc \
	specfunc.o.log \
	specfunc.o.psi \
	specfunc.o.trig \
	specfunc.o.zeta

SYS_OBJS = sys.o.coerce \
	sys.o.fdiv \
	sys.o.infnan \
	sys.o.pow_int

.INIT:
	@cdir cdf.o
	@cdir err.o
	@cdir specfunc.o
	@cdir sys.o

## Rule Patterns ##

.SUFFIXES:	.c .o

.c.o:
	$(CC) $(CC_FLAGS) $(DEPEND) -c -o $@ $<

# Static dependencies:

libgsl: $(CDF_OBJS) $(ERR_OBJS) $(SPECFUNC_OBJS) $(SYS_OBJS)
	$(AR) $(AR_FLAGS) -c -o $@ $(CDF_OBJS) $(ERR_OBJS) $(SPECFUNC_OBJS) $(SYS_OBJS)

clean:
	-ifthere libgsl then wipe libgsl ~CFR~V
	-ifthere cdf.o.* then wipe cdf.o.* ~CFR~V
	-ifthere err.o.* then wipe err.o.* ~CFR~V
	-ifthere specfunc.o.* then wipe specfunc.o.* ~CFR~V
	-ifthere sys.o.* then wipe sys.o.* ~CFR~V

# The following have to be compiled with GCC as they produce an
# internal compiler error with Norcroft C (tested version 5.53)

specfunc.o.exp: specfunc.c.exp
	gcc -mthrowback -O3 -I^ -I@ -D__riscos__ -DHAVE_CONFIG_H -c -o $@ specfunc.c.exp

specfunc.o.gamma_inc: specfunc.c.gamma_inc
	gcc -mthrowback -O3 -I^ -I@ -D__riscos__ -DHAVE_CONFIG_H -c -o $@ specfunc.c.gamma_inc

# Dynamic dependencies:
err.o.stream:	err.c.stream
err.o.stream:	@.h.config
err.o.stream:	Unix:h.stddef
err.o.stream:	Unix:h.stdlib
err.o.stream:	Unix:unixlib.h.features
err.o.stream:	Unix:h.stddef
err.o.stream:	Unix:h.alloca
err.o.stream:	Unix:h.stddef
err.o.stream:	Unix:h.stdio
err.o.stream:	Unix:unixlib.h.features
err.o.stream:	Unix:unixlib.h.types
err.o.stream:	Unix:unixlib.h.features
err.o.stream:	Unix:h.stddef
err.o.stream:	Unix:h.stddef
err.o.stream:	Unix:h.stdarg
err.o.stream:	Unix:h.getopt
err.o.stream:	Unix:h.ctype
err.o.stream:	^.gsl.h.gsl_errno
err.o.stream:	Unix:h.stdio
err.o.stream:	Unix:h.errno
err.o.stream:	^.gsl.h.gsl_types
err.o.stream:	^.gsl.h.gsl_message
err.o.stream:	^.gsl.h.gsl_types
specfunc.o.zeta:	specfunc.c.zeta
specfunc.o.zeta:	@.h.config
specfunc.o.zeta:	^.gsl.h.gsl_math
specfunc.o.zeta:	Unix:h.math
specfunc.o.zeta:	Unix:unixlib.h.features
specfunc.o.zeta:	Unix:machine.h.huge_val
specfunc.o.zeta:	Unix:machine.h.nan
specfunc.o.zeta:	Unix:unixlib.h.types
specfunc.o.zeta:	Unix:unixlib.h.features
specfunc.o.zeta:	Unix:h.stddef
specfunc.o.zeta:	^.gsl.h.gsl_sys
specfunc.o.zeta:	^.gsl.h.gsl_machine
specfunc.o.zeta:	Unix:h.limits
specfunc.o.zeta:	Unix:h.float
specfunc.o.zeta:	^.gsl.h.gsl_precision
specfunc.o.zeta:	^.gsl.h.gsl_types
specfunc.o.zeta:	^.gsl.h.gsl_nan
specfunc.o.zeta:	^.gsl.h.gsl_pow_int
specfunc.o.zeta:	^.gsl.h.gsl_errno
specfunc.o.zeta:	Unix:h.stdio
specfunc.o.zeta:	Unix:unixlib.h.features
specfunc.o.zeta:	Unix:unixlib.h.types
specfunc.o.zeta:	Unix:h.stddef
specfunc.o.zeta:	Unix:h.stdarg
specfunc.o.zeta:	Unix:h.getopt
specfunc.o.zeta:	Unix:h.ctype
specfunc.o.zeta:	Unix:h.errno
specfunc.o.zeta:	^.gsl.h.gsl_types
specfunc.o.zeta:	^.gsl.h.gsl_sf_elementary
specfunc.o.zeta:	^.gsl.h.gsl_sf_result
specfunc.o.zeta:	^.gsl.h.gsl_sf_exp
specfunc.o.zeta:	^.gsl.h.gsl_sf_result
specfunc.o.zeta:	^.gsl.h.gsl_precision
specfunc.o.zeta:	^.gsl.h.gsl_sf_gamma
specfunc.o.zeta:	^.gsl.h.gsl_sf_result
specfunc.o.zeta:	^.gsl.h.gsl_sf_pow_int
specfunc.o.zeta:	^.gsl.h.gsl_sf_result
specfunc.o.zeta:	^.gsl.h.gsl_sf_zeta
specfunc.o.zeta:	^.gsl.h.gsl_sf_result
specfunc.o.zeta:	specfunc.h.error
specfunc.o.zeta:	specfunc.h.chebyshev
specfunc.o.zeta:	specfunc.c.cheb_eval
specfunc.o.zeta:	specfunc.h.eval
sys.o.fdiv:	sys.c.fdiv
sys.o.fdiv:	@.h.config
sys.o.fdiv:	Unix:h.math
sys.o.fdiv:	Unix:unixlib.h.features
sys.o.fdiv:	Unix:machine.h.huge_val
sys.o.fdiv:	Unix:machine.h.nan
sys.o.fdiv:	Unix:unixlib.h.types
sys.o.fdiv:	Unix:unixlib.h.features
sys.o.fdiv:	Unix:h.stddef
specfunc.o.log:	specfunc.c.log
specfunc.o.log:	@.h.config
specfunc.o.log:	^.gsl.h.gsl_math
specfunc.o.log:	Unix:h.math
specfunc.o.log:	Unix:unixlib.h.features
specfunc.o.log:	Unix:machine.h.huge_val
specfunc.o.log:	Unix:machine.h.nan
specfunc.o.log:	Unix:unixlib.h.types
specfunc.o.log:	Unix:unixlib.h.features
specfunc.o.log:	Unix:h.stddef
specfunc.o.log:	^.gsl.h.gsl_sys
specfunc.o.log:	^.gsl.h.gsl_machine
specfunc.o.log:	Unix:h.limits
specfunc.o.log:	Unix:h.float
specfunc.o.log:	^.gsl.h.gsl_precision
specfunc.o.log:	^.gsl.h.gsl_types
specfunc.o.log:	^.gsl.h.gsl_nan
specfunc.o.log:	^.gsl.h.gsl_pow_int
specfunc.o.log:	^.gsl.h.gsl_errno
specfunc.o.log:	Unix:h.stdio
specfunc.o.log:	Unix:unixlib.h.features
specfunc.o.log:	Unix:unixlib.h.types
specfunc.o.log:	Unix:h.stddef
specfunc.o.log:	Unix:h.stdarg
specfunc.o.log:	Unix:h.getopt
specfunc.o.log:	Unix:h.ctype
specfunc.o.log:	Unix:h.errno
specfunc.o.log:	^.gsl.h.gsl_types
specfunc.o.log:	^.gsl.h.gsl_sf_log
specfunc.o.log:	^.gsl.h.gsl_sf_result
specfunc.o.log:	specfunc.h.error
specfunc.o.log:	specfunc.h.chebyshev
specfunc.o.log:	specfunc.c.cheb_eval
specfunc.o.log:	specfunc.h.eval
specfunc.o.psi:	specfunc.c.psi
specfunc.o.psi:	@.h.config
specfunc.o.psi:	^.gsl.h.gsl_math
specfunc.o.psi:	Unix:h.math
specfunc.o.psi:	Unix:unixlib.h.features
specfunc.o.psi:	Unix:machine.h.huge_val
specfunc.o.psi:	Unix:machine.h.nan
specfunc.o.psi:	Unix:unixlib.h.types
specfunc.o.psi:	Unix:unixlib.h.features
specfunc.o.psi:	Unix:h.stddef
specfunc.o.psi:	^.gsl.h.gsl_sys
specfunc.o.psi:	^.gsl.h.gsl_machine
specfunc.o.psi:	Unix:h.limits
specfunc.o.psi:	Unix:h.float
specfunc.o.psi:	^.gsl.h.gsl_precision
specfunc.o.psi:	^.gsl.h.gsl_types
specfunc.o.psi:	^.gsl.h.gsl_nan
specfunc.o.psi:	^.gsl.h.gsl_pow_int
specfunc.o.psi:	^.gsl.h.gsl_errno
specfunc.o.psi:	Unix:h.stdio
specfunc.o.psi:	Unix:unixlib.h.features
specfunc.o.psi:	Unix:unixlib.h.types
specfunc.o.psi:	Unix:h.stddef
specfunc.o.psi:	Unix:h.stdarg
specfunc.o.psi:	Unix:h.getopt
specfunc.o.psi:	Unix:h.ctype
specfunc.o.psi:	Unix:h.errno
specfunc.o.psi:	^.gsl.h.gsl_types
specfunc.o.psi:	^.gsl.h.gsl_sf_exp
specfunc.o.psi:	^.gsl.h.gsl_sf_result
specfunc.o.psi:	^.gsl.h.gsl_precision
specfunc.o.psi:	^.gsl.h.gsl_sf_gamma
specfunc.o.psi:	^.gsl.h.gsl_sf_result
specfunc.o.psi:	^.gsl.h.gsl_sf_zeta
specfunc.o.psi:	^.gsl.h.gsl_sf_result
specfunc.o.psi:	^.gsl.h.gsl_sf_psi
specfunc.o.psi:	^.gsl.h.gsl_sf_result
specfunc.o.psi:	specfunc.h.error
specfunc.o.psi:	specfunc.h.chebyshev
specfunc.o.psi:	specfunc.c.cheb_eval
specfunc.o.psi:	specfunc.h.eval
specfunc.o.trig:	specfunc.c.trig
specfunc.o.trig:	@.h.config
specfunc.o.trig:	^.gsl.h.gsl_math
specfunc.o.trig:	Unix:h.math
specfunc.o.trig:	Unix:unixlib.h.features
specfunc.o.trig:	Unix:machine.h.huge_val
specfunc.o.trig:	Unix:machine.h.nan
specfunc.o.trig:	Unix:unixlib.h.types
specfunc.o.trig:	Unix:unixlib.h.features
specfunc.o.trig:	Unix:h.stddef
specfunc.o.trig:	^.gsl.h.gsl_sys
specfunc.o.trig:	^.gsl.h.gsl_machine
specfunc.o.trig:	Unix:h.limits
specfunc.o.trig:	Unix:h.float
specfunc.o.trig:	^.gsl.h.gsl_precision
specfunc.o.trig:	^.gsl.h.gsl_types
specfunc.o.trig:	^.gsl.h.gsl_nan
specfunc.o.trig:	^.gsl.h.gsl_pow_int
specfunc.o.trig:	^.gsl.h.gsl_errno
specfunc.o.trig:	Unix:h.stdio
specfunc.o.trig:	Unix:unixlib.h.features
specfunc.o.trig:	Unix:unixlib.h.types
specfunc.o.trig:	Unix:h.stddef
specfunc.o.trig:	Unix:h.stdarg
specfunc.o.trig:	Unix:h.getopt
specfunc.o.trig:	Unix:h.ctype
specfunc.o.trig:	Unix:h.errno
specfunc.o.trig:	^.gsl.h.gsl_types
specfunc.o.trig:	^.gsl.h.gsl_sf_log
specfunc.o.trig:	^.gsl.h.gsl_sf_result
specfunc.o.trig:	^.gsl.h.gsl_sf_trig
specfunc.o.trig:	^.gsl.h.gsl_sf_result
specfunc.o.trig:	specfunc.h.error
specfunc.o.trig:	specfunc.h.chebyshev
specfunc.o.trig:	specfunc.c.cheb_eval
specfunc.o.trig:	specfunc.h.eval
sys.o.coerce:	sys.c.coerce
sys.o.coerce:	@.h.config
sys.o.coerce:	Unix:h.math
sys.o.coerce:	Unix:unixlib.h.features
sys.o.coerce:	Unix:machine.h.huge_val
sys.o.coerce:	Unix:machine.h.nan
sys.o.coerce:	Unix:unixlib.h.types
sys.o.coerce:	Unix:unixlib.h.features
sys.o.coerce:	Unix:h.stddef
cdf.o.gamma:	cdf.c.gamma
cdf.o.gamma:	@.h.config
cdf.o.gamma:	Unix:h.math
cdf.o.gamma:	Unix:unixlib.h.features
cdf.o.gamma:	Unix:machine.h.huge_val
cdf.o.gamma:	Unix:machine.h.nan
cdf.o.gamma:	Unix:unixlib.h.types
cdf.o.gamma:	Unix:unixlib.h.features
cdf.o.gamma:	Unix:h.stddef
cdf.o.gamma:	^.gsl.h.gsl_cdf
cdf.o.gamma:	^.gsl.h.gsl_math
cdf.o.gamma:	Unix:h.math
cdf.o.gamma:	^.gsl.h.gsl_sys
cdf.o.gamma:	^.gsl.h.gsl_machine
cdf.o.gamma:	Unix:h.limits
cdf.o.gamma:	Unix:h.float
cdf.o.gamma:	^.gsl.h.gsl_precision
cdf.o.gamma:	^.gsl.h.gsl_types
cdf.o.gamma:	^.gsl.h.gsl_nan
cdf.o.gamma:	^.gsl.h.gsl_pow_int
cdf.o.gamma:	^.gsl.h.gsl_sf_gamma
cdf.o.gamma:	^.gsl.h.gsl_sf_result
cdf.o.gamma:	cdf.^.specfunc.c.gamma
cdf.o.gamma:	@.h.config
cdf.o.gamma:	^.gsl.h.gsl_math
cdf.o.gamma:	^.gsl.h.gsl_errno
cdf.o.gamma:	Unix:h.stdio
cdf.o.gamma:	Unix:unixlib.h.features
cdf.o.gamma:	Unix:unixlib.h.types
cdf.o.gamma:	Unix:h.stddef
cdf.o.gamma:	Unix:h.stdarg
cdf.o.gamma:	Unix:h.getopt
cdf.o.gamma:	Unix:h.ctype
cdf.o.gamma:	Unix:h.errno
cdf.o.gamma:	^.gsl.h.gsl_types
cdf.o.gamma:	^.gsl.h.gsl_sf_exp
cdf.o.gamma:	^.gsl.h.gsl_sf_result
cdf.o.gamma:	^.gsl.h.gsl_precision
cdf.o.gamma:	^.gsl.h.gsl_sf_log
cdf.o.gamma:	^.gsl.h.gsl_sf_result
cdf.o.gamma:	^.gsl.h.gsl_sf_psi
cdf.o.gamma:	^.gsl.h.gsl_sf_result
cdf.o.gamma:	^.gsl.h.gsl_sf_trig
cdf.o.gamma:	^.gsl.h.gsl_sf_result
cdf.o.gamma:	^.gsl.h.gsl_sf_gamma
cdf.o.gamma:	cdf.^.specfunc.h.error
cdf.o.gamma:	cdf.^.specfunc.h.check
cdf.o.gamma:	cdf.^.specfunc.h.chebyshev
cdf.o.gamma:	cdf.^.specfunc.c.cheb_eval
cdf.o.gamma:	cdf.^.specfunc.h.eval
specfunc.o.elementary:	specfunc.c.elementary
specfunc.o.elementary:	@.h.config
specfunc.o.elementary:	^.gsl.h.gsl_math
specfunc.o.elementary:	Unix:h.math
specfunc.o.elementary:	Unix:unixlib.h.features
specfunc.o.elementary:	Unix:machine.h.huge_val
specfunc.o.elementary:	Unix:machine.h.nan
specfunc.o.elementary:	Unix:unixlib.h.types
specfunc.o.elementary:	Unix:unixlib.h.features
specfunc.o.elementary:	Unix:h.stddef
specfunc.o.elementary:	^.gsl.h.gsl_sys
specfunc.o.elementary:	^.gsl.h.gsl_machine
specfunc.o.elementary:	Unix:h.limits
specfunc.o.elementary:	Unix:h.float
specfunc.o.elementary:	^.gsl.h.gsl_precision
specfunc.o.elementary:	^.gsl.h.gsl_types
specfunc.o.elementary:	^.gsl.h.gsl_nan
specfunc.o.elementary:	^.gsl.h.gsl_pow_int
specfunc.o.elementary:	^.gsl.h.gsl_errno
specfunc.o.elementary:	Unix:h.stdio
specfunc.o.elementary:	Unix:unixlib.h.features
specfunc.o.elementary:	Unix:unixlib.h.types
specfunc.o.elementary:	Unix:h.stddef
specfunc.o.elementary:	Unix:h.stdarg
specfunc.o.elementary:	Unix:h.getopt
specfunc.o.elementary:	Unix:h.ctype
specfunc.o.elementary:	Unix:h.errno
specfunc.o.elementary:	^.gsl.h.gsl_types
specfunc.o.elementary:	^.gsl.h.gsl_sf_elementary
specfunc.o.elementary:	^.gsl.h.gsl_sf_result
specfunc.o.elementary:	specfunc.h.error
specfunc.o.elementary:	specfunc.h.check
specfunc.o.elementary:	specfunc.h.eval
specfunc.o.erfc:	specfunc.c.erfc
specfunc.o.erfc:	@.h.config
specfunc.o.erfc:	^.gsl.h.gsl_math
specfunc.o.erfc:	Unix:h.math
specfunc.o.erfc:	Unix:unixlib.h.features
specfunc.o.erfc:	Unix:machine.h.huge_val
specfunc.o.erfc:	Unix:machine.h.nan
specfunc.o.erfc:	Unix:unixlib.h.types
specfunc.o.erfc:	Unix:unixlib.h.features
specfunc.o.erfc:	Unix:h.stddef
specfunc.o.erfc:	^.gsl.h.gsl_sys
specfunc.o.erfc:	^.gsl.h.gsl_machine
specfunc.o.erfc:	Unix:h.limits
specfunc.o.erfc:	Unix:h.float
specfunc.o.erfc:	^.gsl.h.gsl_precision
specfunc.o.erfc:	^.gsl.h.gsl_types
specfunc.o.erfc:	^.gsl.h.gsl_nan
specfunc.o.erfc:	^.gsl.h.gsl_pow_int
specfunc.o.erfc:	^.gsl.h.gsl_errno
specfunc.o.erfc:	Unix:h.stdio
specfunc.o.erfc:	Unix:unixlib.h.features
specfunc.o.erfc:	Unix:unixlib.h.types
specfunc.o.erfc:	Unix:h.stddef
specfunc.o.erfc:	Unix:h.stdarg
specfunc.o.erfc:	Unix:h.getopt
specfunc.o.erfc:	Unix:h.ctype
specfunc.o.erfc:	Unix:h.errno
specfunc.o.erfc:	^.gsl.h.gsl_types
specfunc.o.erfc:	^.gsl.h.gsl_sf_exp
specfunc.o.erfc:	^.gsl.h.gsl_sf_result
specfunc.o.erfc:	^.gsl.h.gsl_precision
specfunc.o.erfc:	^.gsl.h.gsl_sf_erf
specfunc.o.erfc:	^.gsl.h.gsl_sf_result
specfunc.o.erfc:	specfunc.h.check
specfunc.o.erfc:	specfunc.h.chebyshev
specfunc.o.erfc:	specfunc.c.cheb_eval
specfunc.o.erfc:	specfunc.h.eval
specfunc.o.expint:	specfunc.c.expint
specfunc.o.expint:	@.h.config
specfunc.o.expint:	^.gsl.h.gsl_math
specfunc.o.expint:	Unix:h.math
specfunc.o.expint:	Unix:unixlib.h.features
specfunc.o.expint:	Unix:machine.h.huge_val
specfunc.o.expint:	Unix:machine.h.nan
specfunc.o.expint:	Unix:unixlib.h.types
specfunc.o.expint:	Unix:unixlib.h.features
specfunc.o.expint:	Unix:h.stddef
specfunc.o.expint:	^.gsl.h.gsl_sys
specfunc.o.expint:	^.gsl.h.gsl_machine
specfunc.o.expint:	Unix:h.limits
specfunc.o.expint:	Unix:h.float
specfunc.o.expint:	^.gsl.h.gsl_precision
specfunc.o.expint:	^.gsl.h.gsl_types
specfunc.o.expint:	^.gsl.h.gsl_nan
specfunc.o.expint:	^.gsl.h.gsl_pow_int
specfunc.o.expint:	^.gsl.h.gsl_errno
specfunc.o.expint:	Unix:h.stdio
specfunc.o.expint:	Unix:unixlib.h.features
specfunc.o.expint:	Unix:unixlib.h.types
specfunc.o.expint:	Unix:h.stddef
specfunc.o.expint:	Unix:h.stdarg
specfunc.o.expint:	Unix:h.getopt
specfunc.o.expint:	Unix:h.ctype
specfunc.o.expint:	Unix:h.errno
specfunc.o.expint:	^.gsl.h.gsl_types
specfunc.o.expint:	^.gsl.h.gsl_sf_expint
specfunc.o.expint:	^.gsl.h.gsl_sf_result
specfunc.o.expint:	specfunc.h.error
specfunc.o.expint:	specfunc.h.chebyshev
specfunc.o.expint:	specfunc.c.cheb_eval
specfunc.o.expint:	specfunc.h.eval
