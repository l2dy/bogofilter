#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cdflib.h"

/*
 * A comment about ints and longs - whether ints or longs are used should
 * make no difference, but where double r-values are assigned to ints the
 * r-value is cast converted to a long, which is then assigned to the int
 * to be compatible with the operation of fifidint.
 */
/*
-----------------------------------------------------------------------
 
     COMPUTATION OF LN(GAMMA(B)/GAMMA(A+B)) WHEN B .GE. 8
 
                         --------
 
     IN THIS ALGORITHM, DEL(X) IS THE FUNCTION DEFINED BY
     LN(GAMMA(X)) = (X - 0.5)*LN(X) - X + 0.5*LN(2*PI) + DEL(X).
 
-----------------------------------------------------------------------
*/

#include <float.h> /* has DBL_EPSILON */
#define EPS	(100.0 * DBL_EPSILON) /* equality cutoff */
#define	EQ(a,b)	(fabs((a) -(b)) < EPS)
#define	NE(a,b)	(fabs((a) -(b)) > EPS)

void cdfchi(int *which,double *p,double *q,double *x,double *df,
	    int *status,double *bound)
/**********************************************************************

      void cdfchi(int *which,double *p,double *q,double *x,double *df,
            int *status,double *bound)

               Cumulative Distribution Function
               CHI-Square distribution


                              Function


     Calculates any one parameter of the chi-square
     distribution given values for the others.


                              Arguments


     WHICH --> Integer indicating which of the next three argument
               values is to be calculated from the others.
               Legal range: 1..3
               iwhich = 1 : Calculate P and Q from X and DF
               iwhich = 2 : Calculate X from P,Q and DF
               iwhich = 3 : Calculate DF from P,Q and X

     P <--> The integral from 0 to X of the chi-square
            distribution.
            Input range: [0, 1].

     Q <--> 1-P.
            Input range: (0, 1].
            P + Q = 1.0.

     X <--> Upper limit of integration of the non-central
            chi-square distribution.
            Input range: [0, +infinity).
            Search range: [0,1E100]

     DF <--> Degrees of freedom of the
             chi-square distribution.
             Input range: (0, +infinity).
             Search range: [ 1E-100, 1E100]

     STATUS <-- 0 if calculation completed correctly
               -I if input parameter number I is out of range
                1 if answer appears to be lower than lowest
                  search bound
                2 if answer appears to be higher than greatest
                  search bound
                3 if P + Q .ne. 1
               10 indicates error returned from cumgam.  See
                  references in cdfgam

     BOUND <-- Undefined if STATUS is 0

               Bound exceeded by parameter number I if STATUS
               is negative.

               Lower search bound if STATUS is 1.

               Upper search bound if STATUS is 2.


                              Method


     Formula    26.4.19   of Abramowitz  and     Stegun, Handbook  of
     Mathematical Functions   (1966) is used   to reduce the chisqure
     distribution to the incomplete distribution.

     Computation of other parameters involve a seach for a value that
     produces  the desired  value  of P.   The search relies  on  the
     monotinicity of P with the other parameter.

**********************************************************************/
{
#define tol 1.0e-8
#define atol 1.0e-50
#define zero 1.0e-100
#define inf 1.0e100
static int K1 = 1;
static double K2 = 0.0e0;
static double K4 = 0.5e0;
static double K5 = 5.0e0;
static double fx,cum,ccum,pq,porq;
static unsigned long qhi,qleft,qporq;
static double T3,T6,T7,T8,T9,T10,T11;
/*
     ..
     .. Executable Statements ..
*/
/*
     Check arguments
*/
    if(!(*which < 1 || *which > 3)) goto S30;
    if(!(*which < 1)) goto S10;
    *bound = 1.0e0;
    goto S20;
S10:
    *bound = 3.0e0;
S20:
    *status = -1;
    return;
S30:
    if(*which == 1) goto S70;
/*
     P
*/
    if(!(*p < 0.0e0 || *p > 1.0e0)) goto S60;
    if(!(*p < 0.0e0)) goto S40;
    *bound = 0.0e0;
    goto S50;
S40:
    *bound = 1.0e0;
S50:
    *status = -2;
    return;
S70:
S60:
    if(*which == 1) goto S110;
/*
     Q
*/
    if(!(*q <= 0.0e0 || *q > 1.0e0)) goto S100;
    if(!(*q <= 0.0e0)) goto S80;
    *bound = 0.0e0;
    goto S90;
S80:
    *bound = 1.0e0;
S90:
    *status = -3;
    return;
S110:
S100:
    if(*which == 2) goto S130;
/*
     X
*/
    if(!(*x < 0.0e0)) goto S120;
    *bound = 0.0e0;
    *status = -4;
    return;
S130:
S120:
    if(*which == 3) goto S150;
/*
     DF
*/
    if(!(*df <= 0.0e0)) goto S140;
    *bound = 0.0e0;
    *status = -5;
    return;
S150:
S140:
    if(*which == 1) goto S190;
/*
     P + Q
*/
    pq = *p+*q;
    if(!(fabs(pq-0.5e0-0.5e0) > 3.0e0*spmpar(&K1))) goto S180;
    if(!(pq < 0.0e0)) goto S160;
    *bound = 0.0e0;
    goto S170;
S160:
    *bound = 1.0e0;
S170:
    *status = 3;
    return;
S190:
S180:
    if(*which == 1) goto S220;
/*
     Select the minimum of P or Q
*/
    qporq = *p <= *q;
    if(!qporq) goto S200;
    porq = *p;
    goto S210;
S200:
    porq = *q;
S220:
S210:
/*
     Calculate ANSWERS
*/
    if(1 == *which) {
/*
     Calculating P and Q
*/
        *status = 0;
        cumchi(x,df,p,q);
        if(porq > 1.5e0) {
            *status = 10;
            return;
        }
    }
    else if(2 == *which) {
/*
     Calculating X
*/
        *x = 5.0e0;
        T3 = inf;
        T6 = atol;
        T7 = tol;
        dstinv(&K2,&T3,&K4,&K4,&K5,&T6,&T7);
        *status = 0;
        dinvr(status,x,&fx,&qleft,&qhi);
S230:
        if(!(*status == 1)) goto S270;
        cumchi(x,df,&cum,&ccum);
        if(!qporq) goto S240;
        fx = cum-*p;
        goto S250;
S240:
        fx = ccum-*q;
S250:
        if(!(fx+porq > 1.5e0)) goto S260;
        *status = 10;
        return;
S260:
        dinvr(status,x,&fx,&qleft,&qhi);
        goto S230;
S270:
        if(!(*status == -1)) goto S300;
        if(!qleft) goto S280;
        *status = 1;
        *bound = 0.0e0;
        goto S290;
S280:
        *status = 2;
        *bound = inf;
S300:
S290:
        ;
    }
    else if(3 == *which) {
/*
     Calculating DF
*/
        *df = 5.0e0;
        T8 = zero;
        T9 = inf;
        T10 = atol;
        T11 = tol;
        dstinv(&T8,&T9,&K4,&K4,&K5,&T10,&T11);
        *status = 0;
        dinvr(status,df,&fx,&qleft,&qhi);
S310:
        if(!(*status == 1)) goto S350;
        cumchi(x,df,&cum,&ccum);
        if(!qporq) goto S320;
        fx = cum-*p;
        goto S330;
S320:
        fx = ccum-*q;
S330:
        if(!(fx+porq > 1.5e0)) goto S340;
        *status = 10;
        return;
S340:
        dinvr(status,df,&fx,&qleft,&qhi);
        goto S310;
S350:
        if(!(*status == -1)) goto S380;
        if(!qleft) goto S360;
        *status = 1;
        *bound = zero;
        goto S370;
S360:
        *status = 2;
        *bound = inf;
S370:
        ;
    }
S380:
    return;
#undef tol
#undef atol
#undef zero
#undef inf
}

void cumchi(double *x,double *df,double *cum,double *ccum)
/*
**********************************************************************
 
     void cumchi(double *x,double *df,double *cum,double *ccum)
             CUMulative of the CHi-square distribution
 
 
                              Function
 
 
     Calculates the cumulative chi-square distribution.
 
 
                              Arguments
 
 
     X       --> Upper limit of integration of the
                 chi-square distribution.
                                                 X is DOUBLE PRECISION
 
     DF      --> Degrees of freedom of the
                 chi-square distribution.
                                                 DF is DOUBLE PRECISION
 
     CUM <-- Cumulative chi-square distribution.
                                                 CUM is DOUBLE PRECISIO
 
     CCUM <-- Compliment of Cumulative chi-square distribution.
                                                 CCUM is DOUBLE PRECISI
 
 
                              Method
 
 
     Calls incomplete gamma function (CUMGAM)
 
**********************************************************************
*/
{
static double a,xx;
/*
     ..
     .. Executable Statements ..
*/
    a = *df*0.5e0;
    xx = *x*0.5e0;
    cumgam(&xx,&a,cum,ccum);
    return;
}

void cumgam(double *x,double *a,double *cum,double *ccum)
/*
**********************************************************************
 
     void cumgam(double *x,double *a,double *cum,double *ccum)
           Double precision cUMulative incomplete GAMma distribution
 
 
                              Function
 
 
     Computes   the  cumulative        of    the     incomplete   gamma
     distribution, i.e., the integral from 0 to X of
          (1/GAM(A))*EXP(-T)*T**(A-1) DT
     where GAM(A) is the complete gamma function of A, i.e.,
          GAM(A) = integral from 0 to infinity of
                    EXP(-T)*T**(A-1) DT
 
 
                              Arguments
 
 
     X --> The upper limit of integration of the incomplete gamma.
                                                X is DOUBLE PRECISION
 
     A --> The shape parameter of the incomplete gamma.
                                                A is DOUBLE PRECISION
 
     CUM <-- Cumulative incomplete gamma distribution.
                                        CUM is DOUBLE PRECISION
 
     CCUM <-- Compliment of Cumulative incomplete gamma distribution.
                                                CCUM is DOUBLE PRECISIO
 
 
                              Method
 
 
     Calls the routine GRATIO.
 
**********************************************************************
*/
{
static int K1 = 0;
/*
     ..
     .. Executable Statements ..
*/
    if(!(*x <= 0.0e0)) goto S10;
    *cum = 0.0e0;
    *ccum = 1.0e0;
    return;
S10:
    gratio(a,x,cum,ccum,&K1);
/*
     Call gratio routine
*/
    return;
}

/* DEFINE DINVR */
static void E0000(int IENTRY,int *status,double *x,double *fx,
		  unsigned long *qleft,unsigned long *qhi,double *zabsst,
		  double *zabsto,double *zbig,double *zrelst,
		  double *zrelto,double *zsmall,double *zstpmu)
{
#define qxmon(zx,zy,zz) (int)((zx) <= (zy) && (zy) <= (zz))
static double absstp,abstol,big,fbig,fsmall,relstp,reltol,small,step,stpmul,xhi,
    xlb,xlo,xsave,xub,yy;
static int i99999;
static unsigned long qbdd,qcond,qdum1,qdum2,qincr,qlim,qok,qup;
    switch(IENTRY){case 0: goto DINVR; case 1: goto DSTINV;}
DINVR:
    if(*status > 0) goto S310;
    qcond = !qxmon(small,*x,big);
    if(qcond) ftnstop(" SMALL, X, BIG not monotone in INVR");
    xsave = *x;
/*
     See that SMALL and BIG bound the zero and set QINCR
*/
    *x = small;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 1;
    goto S300;
S10:
    fsmall = *fx;
    *x = big;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 2;
    goto S300;
S20:
    fbig = *fx;
    qincr = fbig > fsmall;
    if(!qincr) goto S50;
    if(fsmall <= 0.0e0) goto S30;
    *status = -1;
    *qleft = *qhi = 1;
    return;
S30:
    if(fbig >= 0.0e0) goto S40;
    *status = -1;
    *qleft = *qhi = 0;
    return;
S40:
    goto S80;
S50:
    if(fsmall >= 0.0e0) goto S60;
    *status = -1;
    *qleft = 1;
    *qhi = 0;
    return;
S60:
    if(fbig <= 0.0e0) goto S70;
    *status = -1;
    *qleft = 0;
    *qhi = 1;
    return;
S80:
S70:
    *x = xsave;
    step = fifdmax1(absstp,relstp*fabs(*x));
/*
      YY = F(X) - Y
     GET-FUNCTION-VALUE
*/
    i99999 = 3;
    goto S300;
S90:
    yy = *fx;
    if(!(EQ(yy, 0.0e0))) goto S100;
    *status = 0;
    qok = 1;
    return;
S100:
    qup = (qincr && yy < 0.0e0) || (!qincr && yy > 0.0e0);
/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     HANDLE CASE IN WHICH WE MUST STEP HIGHER
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
    if(!qup) goto S170;
    xlb = xsave;
    xub = fifdmin1(xlb+step,big);
    goto S120;
S110:
    if(qcond) goto S150;
S120:
/*
      YY = F(XUB) - Y
*/
    *x = xub;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 4;
    goto S300;
S130:
    yy = *fx;
    qbdd = (qincr && yy >= 0.0e0) || (!qincr && yy <= 0.0e0);
    qlim = xub >= big;
    qcond = qbdd || qlim;
    if(qcond) goto S140;
    step = stpmul*step;
    xlb = xub;
    xub = fifdmin1(xlb+step,big);
S140:
    goto S110;
S150:
    if(!(qlim && !qbdd)) goto S160;
    *status = -1;
    *qleft = 0;
    *qhi = !qincr;
    *x = big;
    return;
S160:
    goto S240;
S170:
/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     HANDLE CASE IN WHICH WE MUST STEP LOWER
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
    xub = xsave;
    xlb = fifdmax1(xub-step,small);
    goto S190;
S180:
    if(qcond) goto S220;
S190:
/*
      YY = F(XLB) - Y
*/
    *x = xlb;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 5;
    goto S300;
S200:
    yy = *fx;
    qbdd = (qincr && yy <= 0.0e0) || (!qincr && yy >= 0.0e0);
    qlim = xlb <= small;
    qcond = qbdd || qlim;
    if(qcond) goto S210;
    step = stpmul*step;
    xub = xlb;
    xlb = fifdmax1(xub-step,small);
S210:
    goto S180;
S220:
    if(!(qlim && !qbdd)) goto S230;
    *status = -1;
    *qleft = 1;
    *qhi = qincr;
    *x = small;
    return;
S240:
S230:
    dstzr(&xlb,&xub,&abstol,&reltol);
/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     IF WE REACH HERE, XLB AND XUB BOUND THE ZERO OF F.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
    *status = 0;
    goto S260;
S250:
    if(!(*status == 1)) goto S290;
S260:
    dzror(status,x,fx,&xlo,&xhi,&qdum1,&qdum2);
    if(!(*status == 1)) goto S280;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 6;
    goto S300;
S280:
S270:
    goto S250;
S290:
    *x = xlo;
    *status = 0;
    return;
DSTINV:
    small = *zsmall;
    big = *zbig;
    absstp = *zabsst;
    relstp = *zrelst;
    stpmul = *zstpmu;
    abstol = *zabsto;
    reltol = *zrelto;
    return;
S300:
/*
     TO GET-FUNCTION-VALUE
*/
    *status = 1;
    return;
S310:
    switch((int)i99999){case 1: goto S10;case 2: goto S20;case 3: goto S90;case 
      4: goto S130;case 5: goto S200;case 6: goto S270;default: break;}
#undef qxmon
}

void dinvr(int *status,double *x,double *fx,
	   unsigned long *qleft,unsigned long *qhi)
/*
**********************************************************************
 
     void dinvr(int *status,double *x,double *fx,
           unsigned long *qleft,unsigned long *qhi)

          Double precision
          bounds the zero of the function and invokes zror
                    Reverse Communication
 
 
                              Function
 
 
     Bounds the    function  and  invokes  ZROR   to perform the   zero
     finding.  STINVR  must  have   been  called  before this   routine
     in order to set its parameters.
 
 
                              Arguments
 
 
     STATUS <--> At the beginning of a zero finding problem, STATUS
                 should be set to 0 and INVR invoked.  (The value
                 of parameters other than X will be ignored on this cal
 
                 When INVR needs the function evaluated, it will set
                 STATUS to 1 and return.  The value of the function
                 should be set in FX and INVR again called without
                 changing any of its other parameters.
 
                 When INVR has finished without error, it will return
                 with STATUS 0.  In that case X is approximately a root
                 of F(X).
 
                 If INVR cannot bound the function, it returns status
                 -1 and sets QLEFT and QHI.
                         INTEGER STATUS
 
     X <-- The value of X at which F(X) is to be evaluated.
                         DOUBLE PRECISION X
 
     FX --> The value of F(X) calculated when INVR returns with
            STATUS = 1.
                         DOUBLE PRECISION FX
 
     QLEFT <-- Defined only if QMFINV returns .FALSE.  In that
          case it is .TRUE. If the stepping search terminated
          unsucessfully at SMALL.  If it is .FALSE. the search
          terminated unsucessfully at BIG.
                    QLEFT is LOGICAL
 
     QHI <-- Defined only if QMFINV returns .FALSE.  In that
          case it is .TRUE. if F(X) .GT. Y at the termination
          of the search and .FALSE. if F(X) .LT. Y at the
          termination of the search.
                    QHI is LOGICAL
 
**********************************************************************
*/
{
    E0000(0,status,x,fx,qleft,qhi,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
}

void dstinv(double *zsmall,double *zbig,double *zabsst,
	    double *zrelst,double *zstpmu,double *zabsto,
	    double *zrelto)
/*
**********************************************************************
      void dstinv(double *zsmall,double *zbig,double *zabsst,
            double *zrelst,double *zstpmu,double *zabsto,
            double *zrelto)

      Double Precision - SeT INverse finder - Reverse Communication
                              Function
     Concise Description - Given a monotone function F finds X
     such that F(X) = Y.  Uses Reverse communication -- see invr.
     This routine sets quantities needed by INVR.
          More Precise Description of INVR -
     F must be a monotone function, the results of QMFINV are
     otherwise undefined.  QINCR must be .TRUE. if F is non-
     decreasing and .FALSE. if F is non-increasing.
     QMFINV will return .TRUE. if and only if F(SMALL) and
     F(BIG) bracket Y, i. e.,
          QINCR is .TRUE. and F(SMALL).LE.Y.LE.F(BIG) or
          QINCR is .FALSE. and F(BIG).LE.Y.LE.F(SMALL)
     if QMFINV returns .TRUE., then the X returned satisfies
     the following condition.  let
               TOL(X) = MAX(ABSTOL,RELTOL*ABS(X))
     then if QINCR is .TRUE.,
          F(X-TOL(X)) .LE. Y .LE. F(X+TOL(X))
     and if QINCR is .FALSE.
          F(X-TOL(X)) .GE. Y .GE. F(X+TOL(X))
                              Arguments
     SMALL --> The left endpoint of the interval to be
          searched for a solution.
                    SMALL is DOUBLE PRECISION
     BIG --> The right endpoint of the interval to be
          searched for a solution.
                    BIG is DOUBLE PRECISION
     ABSSTP, RELSTP --> The initial step size in the search
          is MAX(ABSSTP,RELSTP*ABS(X)). See algorithm.
                    ABSSTP is DOUBLE PRECISION
                    RELSTP is DOUBLE PRECISION
     STPMUL --> When a step doesn't bound the zero, the step
                size is multiplied by STPMUL and another step
                taken.  A popular value is 2.0
                    DOUBLE PRECISION STPMUL
     ABSTOL, RELTOL --> Two numbers that determine the accuracy
          of the solution.  See function for a precise definition.
                    ABSTOL is DOUBLE PRECISION
                    RELTOL is DOUBLE PRECISION
                              Method
     Compares F(X) with Y for the input value of X then uses QINCR
     to determine whether to step left or right to bound the
     desired x.  the initial step size is
          MAX(ABSSTP,RELSTP*ABS(S)) for the input value of X.
     Iteratively steps right or left until it bounds X.
     At each step which doesn't bound X, the step size is doubled.
     The routine is careful never to step beyond SMALL or BIG.  If
     it hasn't bounded X at SMALL or BIG, QMFINV returns .FALSE.
     after setting QLEFT and QHI.
     If X is successfully bounded then Algorithm R of the paper
     'Two Efficient Algorithms with Guaranteed Convergence for
     Finding a Zero of a Function' by J. C. P. Bus and
     T. J. Dekker in ACM Transactions on Mathematical
     Software, Volume 1, No. 4 page 330 (DEC. '75) is employed
     to find the zero of the function F(X)-Y. This is routine
     QRZERO.
**********************************************************************
*/
{
    E0000(1,NULL,NULL,NULL,NULL,NULL,zabsst,zabsto,zbig,zrelst,zrelto,zsmall,
    zstpmu);
}

/* DEFINE DZROR */
static void E0001(int IENTRY,int *status,double *x,double *fx,
		  double *xlo,double *xhi,unsigned long *qleft,
		  unsigned long *qhi,double *zabstl,double *zreltl,
		  double *zxhi,double *zxlo)
{
#define ftol(zx) (0.5e0*fifdmax1(abstol,reltol*fabs((zx))))
static double a,abstol,b,c,d,fa,fb,fc,fd,fda,fdb,m,mb,p,q,reltol,tol,w,xxhi,xxlo;
static int ext,i99999;
static unsigned long first,qrzero;
    switch(IENTRY){case 0: goto DZROR; case 1: goto DSTZR;}
DZROR:
    if(*status > 0) goto S280;
    *xlo = xxlo;
    *xhi = xxhi;
    b = *x = *xlo;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 1;
    goto S270;
S10:
    fb = *fx;
    *xlo = *xhi;
    a = *x = *xlo;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 2;
    goto S270;
S20:
/*
     Check that F(ZXLO) < 0 < F(ZXHI)  or
                F(ZXLO) > 0 > F(ZXHI)
*/
    if(!(fb < 0.0e0)) goto S40;
    if(!(*fx < 0.0e0)) goto S30;
    *status = -1;
    *qleft = *fx < fb;
    *qhi = 0;
    return;
S40:
S30:
    if(!(fb > 0.0e0)) goto S60;
    if(!(*fx > 0.0e0)) goto S50;
    *status = -1;
    *qleft = *fx > fb;
    *qhi = 1;
    return;
S60:
S50:
    fa = *fx;
    first = 1;
S70:
    c = a;
    fc = fa;
    ext = 0;
S80:
    if(!(fabs(fc) < fabs(fb))) goto S100;
    if(!NE(c, a)) goto S90;
    d = a;
    fd = fa;
S90:
    a = b;
    fa = fb;
    *xlo = c;
    b = *xlo;
    fb = fc;
    c = a;
    fc = fa;
S100:
    tol = ftol(*xlo);
    m = (c+b)*.5e0;
    mb = m-b;
    if(!(fabs(mb) > tol)) goto S240;
    if(!(ext > 3)) goto S110;
    w = mb;
    goto S190;
S110:
    tol = fifdsign(tol,mb);
    p = (b-a)*fb;
    if(!first) goto S120;
    q = fa-fb;
    first = 0;
    goto S130;
S120:
    fdb = (fd-fb)/(d-b);
    fda = (fd-fa)/(d-a);
    p = fda*p;
    q = fdb*fa-fda*fb;
S130:
    if(!(p < 0.0e0)) goto S140;
    p = -p;
    q = -q;
S140:
    if(ext == 3) p *= 2.0e0;
    if(!(EQ(p*1.0e0, 0.0e0) || p <= q*tol)) goto S150;
    w = tol;
    goto S180;
S150:
    if(!(p < mb*q)) goto S160;
    w = p/q;
    goto S170;
S160:
    w = mb;
S190:
S180:
S170:
    d = a;
    fd = fa;
    a = b;
    fa = fb;
    b += w;
    *xlo = b;
    *x = *xlo;
/*
     GET-FUNCTION-VALUE
*/
    i99999 = 3;
    goto S270;
S200:
    fb = *fx;
    if(!(fc*fb >= 0.0e0)) goto S210;
    goto S70;
S210:
    if(!EQ(w, mb)) goto S220;
    ext = 0;
    goto S230;
S220:
    ext += 1;
S230:
    goto S80;
S240:
    *xhi = c;
    qrzero = (fc >= 0.0e0 && fb <= 0.0e0) || (fc < 0.0e0 && fb >= 0.0e0);
    if(!qrzero) goto S250;
    *status = 0;
    goto S260;
S250:
    *status = -1;
S260:
    return;
DSTZR:
    xxlo = *zxlo;
    xxhi = *zxhi;
    abstol = *zabstl;
    reltol = *zreltl;
    return;
S270:
/*
     TO GET-FUNCTION-VALUE
*/
    *status = 1;
    return;
S280:
    switch((int)i99999){case 1: goto S10;case 2: goto S20;case 3: goto S200;
      default: break;}
#undef ftol
}

void dzror(int *status,double *x,double *fx,double *xlo,
	   double *xhi,unsigned long *qleft,unsigned long *qhi)
/*
**********************************************************************
 
     void dzror(int *status,double *x,double *fx,double *xlo,
           double *xhi,unsigned long *qleft,unsigned long *qhi)

     Double precision ZeRo of a function -- Reverse Communication
 
 
                              Function
 
 
     Performs the zero finding.  STZROR must have been called before
     this routine in order to set its parameters.
 
 
                              Arguments
 
 
     STATUS <--> At the beginning of a zero finding problem, STATUS
                 should be set to 0 and ZROR invoked.  (The value
                 of other parameters will be ignored on this call.)
 
                 When ZROR needs the function evaluated, it will set
                 STATUS to 1 and return.  The value of the function
                 should be set in FX and ZROR again called without
                 changing any of its other parameters.
 
                 When ZROR has finished without error, it will return
                 with STATUS 0.  In that case (XLO,XHI) bound the answe
 
                 If ZROR finds an error (which implies that F(XLO)-Y an
                 F(XHI)-Y have the same sign, it returns STATUS -1.  In
                 this case, XLO and XHI are undefined.
                         INTEGER STATUS
 
     X <-- The value of X at which F(X) is to be evaluated.
                         DOUBLE PRECISION X
 
     FX --> The value of F(X) calculated when ZROR returns with
            STATUS = 1.
                         DOUBLE PRECISION FX
 
     XLO <-- When ZROR returns with STATUS = 0, XLO bounds the
             inverval in X containing the solution below.
                         DOUBLE PRECISION XLO
 
     XHI <-- When ZROR returns with STATUS = 0, XHI bounds the
             inverval in X containing the solution above.
                         DOUBLE PRECISION XHI
 
     QLEFT <-- .TRUE. if the stepping search terminated unsucessfully
                at XLO.  If it is .FALSE. the search terminated
                unsucessfully at XHI.
                    QLEFT is LOGICAL
 
     QHI <-- .TRUE. if F(X) .GT. Y at the termination of the
              search and .FALSE. if F(X) .LT. Y at the
              termination of the search.
                    QHI is LOGICAL
 
**********************************************************************
*/
{
    E0001(0,status,x,fx,xlo,xhi,qleft,qhi,NULL,NULL,NULL,NULL);
}

void dstzr(double *zxlo,double *zxhi,double *zabstl,double *zreltl)
/*
**********************************************************************
     void dstzr(double *zxlo,double *zxhi,double *zabstl,double *zreltl)
     Double precision SeT ZeRo finder - Reverse communication version
                              Function
     Sets quantities needed by ZROR.  The function of ZROR
     and the quantities set is given here.
     Concise Description - Given a function F
     find XLO such that F(XLO) = 0.
          More Precise Description -
     Input condition. F is a double precision function of a single
     double precision argument and XLO and XHI are such that
          F(XLO)*F(XHI)  .LE.  0.0
     If the input condition is met, QRZERO returns .TRUE.
     and output values of XLO and XHI satisfy the following
          F(XLO)*F(XHI)  .LE. 0.
          ABS(F(XLO)  .LE. ABS(F(XHI)
          ABS(XLO-XHI)  .LE. TOL(X)
     where
          TOL(X) = MAX(ABSTOL,RELTOL*ABS(X))
     If this algorithm does not find XLO and XHI satisfying
     these conditions then QRZERO returns .FALSE.  This
     implies that the input condition was not met.
                              Arguments
     XLO --> The left endpoint of the interval to be
           searched for a solution.
                    XLO is DOUBLE PRECISION
     XHI --> The right endpoint of the interval to be
           for a solution.
                    XHI is DOUBLE PRECISION
     ABSTOL, RELTOL --> Two numbers that determine the accuracy
                      of the solution.  See function for a
                      precise definition.
                    ABSTOL is DOUBLE PRECISION
                    RELTOL is DOUBLE PRECISION
                              Method
     Algorithm R of the paper 'Two Efficient Algorithms with
     Guaranteed Convergence for Finding a Zero of a Function'
     by J. C. P. Bus and T. J. Dekker in ACM Transactions on
     Mathematical Software, Volume 1, no. 4 page 330
     (Dec. '75) is employed to find the zero of F(X)-Y.
**********************************************************************
*/
{
    E0001(1,NULL,NULL,NULL,NULL,NULL,NULL,NULL,zabstl,zreltl,zxhi,zxlo);
}

double erf1(double *x)
/*
-----------------------------------------------------------------------
             EVALUATION OF THE REAL ERROR FUNCTION
-----------------------------------------------------------------------
*/
{
static double c = .564189583547756e0;
static double a[5] = {
    .771058495001320e-04,-.133733772997339e-02,.323076579225834e-01,
    .479137145607681e-01,.128379167095513e+00
};
static double b[3] = {
    .301048631703895e-02,.538971687740286e-01,.375795757275549e+00
};
static double p[8] = {
    -1.36864857382717e-07,5.64195517478974e-01,7.21175825088309e+00,
    4.31622272220567e+01,1.52989285046940e+02,3.39320816734344e+02,
    4.51918953711873e+02,3.00459261020162e+02
};
static double q[8] = {
    1.00000000000000e+00,1.27827273196294e+01,7.70001529352295e+01,
    2.77585444743988e+02,6.38980264465631e+02,9.31354094850610e+02,
    7.90950925327898e+02,3.00459260956983e+02
};
static double r[5] = {
    2.10144126479064e+00,2.62370141675169e+01,2.13688200555087e+01,
    4.65807828718470e+00,2.82094791773523e-01
};
static double s[4] = {
    9.41537750555460e+01,1.87114811799590e+02,9.90191814623914e+01,
    1.80124575948747e+01
};
static double ax,bot,t,top,x2;
/*
     ..
     .. Executable Statements ..
*/
    ax = fabs(*x);
    if(ax > 0.5e0) goto S10;
    t = *x**x;
    top = (((a[0]*t+a[1])*t+a[2])*t+a[3])*t+a[4]+1.0e0;
    bot = ((b[0]*t+b[1])*t+b[2])*t+1.0e0;
    return *x*(top/bot);
S10:
    if(ax > 4.0e0) goto S20;
    top = ((((((p[0]*ax+p[1])*ax+p[2])*ax+p[3])*ax+p[4])*ax+p[5])*ax+p[6])*ax+p[
      7];
    bot = ((((((q[0]*ax+q[1])*ax+q[2])*ax+q[3])*ax+q[4])*ax+q[5])*ax+q[6])*ax+q[
      7];
    if(*x < 0.0e0) return -(0.5e0+(0.5e0-exp(-(*x**x))*top/bot));
    else return 0.5e0+(0.5e0-exp(-(*x**x))*top/bot);
S20:
    if(ax >= 5.8e0) goto S30;
    x2 = *x**x;
    t = 1.0e0/x2;
    top = (((r[0]*t+r[1])*t+r[2])*t+r[3])*t+r[4];
    bot = (((s[0]*t+s[1])*t+s[2])*t+s[3])*t+1.0e0;
    if(*x < 0.0e0) return -(0.5e0+(0.5e0-exp(-x2)*((c-top/(x2*bot))/ax)));
    else return 0.5e0+(0.5e0-exp(-x2)*((c-top/(x2*bot))/ax));
S30:
    return fifdsign(1.0e0,*x);
}

double erfc1(int *ind,double *x)
/*
-----------------------------------------------------------------------
         EVALUATION OF THE COMPLEMENTARY ERROR FUNCTION
 
          ERFC1(IND,X) = ERFC(X)            IF IND = 0
          ERFC1(IND,X) = EXP(X*X)*ERFC(X)   OTHERWISE
-----------------------------------------------------------------------
*/
{
static double c = .564189583547756e0;
static double a[5] = {
    .771058495001320e-04,-.133733772997339e-02,.323076579225834e-01,
    .479137145607681e-01,.128379167095513e+00
};
static double b[3] = {
    .301048631703895e-02,.538971687740286e-01,.375795757275549e+00
};
static double p[8] = {
    -1.36864857382717e-07,5.64195517478974e-01,7.21175825088309e+00,
    4.31622272220567e+01,1.52989285046940e+02,3.39320816734344e+02,
    4.51918953711873e+02,3.00459261020162e+02
};
static double q[8] = {
    1.00000000000000e+00,1.27827273196294e+01,7.70001529352295e+01,
    2.77585444743988e+02,6.38980264465631e+02,9.31354094850610e+02,
    7.90950925327898e+02,3.00459260956983e+02
};
static double r[5] = {
    2.10144126479064e+00,2.62370141675169e+01,2.13688200555087e+01,
    4.65807828718470e+00,2.82094791773523e-01
};
static double s[4] = {
    9.41537750555460e+01,1.87114811799590e+02,9.90191814623914e+01,
    1.80124575948747e+01
};
static int K1 = 1;
static double erfc1,ax,bot,e,t,top,w;
/*
     ..
     .. Executable Statements ..
*/
/*
                     ABS(X) .LE. 0.5
*/
    ax = fabs(*x);
    if(ax > 0.5e0) goto S10;
    t = *x**x;
    top = (((a[0]*t+a[1])*t+a[2])*t+a[3])*t+a[4]+1.0e0;
    bot = ((b[0]*t+b[1])*t+b[2])*t+1.0e0;
    erfc1 = 0.5e0+(0.5e0-*x*(top/bot));
    if(*ind != 0) erfc1 = exp(t)*erfc1;
    return erfc1;
S10:
/*
                  0.5 .LT. ABS(X) .LE. 4
*/
    if(ax > 4.0e0) goto S20;
    top = ((((((p[0]*ax+p[1])*ax+p[2])*ax+p[3])*ax+p[4])*ax+p[5])*ax+p[6])*ax+p[
      7];
    bot = ((((((q[0]*ax+q[1])*ax+q[2])*ax+q[3])*ax+q[4])*ax+q[5])*ax+q[6])*ax+q[
      7];
    erfc1 = top/bot;
    goto S40;
S20:
/*
                      ABS(X) .GT. 4
*/
    if(*x <= -5.6e0) goto S60;
    if(*ind != 0) goto S30;
    if(*x > 100.0e0) goto S70;
    if(*x**x > -exparg(&K1)) goto S70;
S30:
    t = pow(1.0e0/ *x,2.0);
    top = (((r[0]*t+r[1])*t+r[2])*t+r[3])*t+r[4];
    bot = (((s[0]*t+s[1])*t+s[2])*t+s[3])*t+1.0e0;
    erfc1 = (c-t*top/bot)/ax;
S40:
/*
                      FINAL ASSEMBLY
*/
    if(*ind == 0) goto S50;
    if(*x < 0.0e0) erfc1 = 2.0e0*exp(*x**x)-erfc1;
    return erfc1;
S50:
    w = *x**x;
    t = w;
    e = w-t;
    erfc1 = (0.5e0+(0.5e0-e))*exp(-t)*erfc1;
    if(*x < 0.0e0) erfc1 = 2.0e0-erfc1;
    return erfc1;
S60:
/*
             LIMIT VALUE FOR LARGE NEGATIVE X
*/
    erfc1 = 2.0e0;
    if(*ind != 0) erfc1 = 2.0e0*exp(*x**x);
    return erfc1;
S70:
/*
             LIMIT VALUE FOR LARGE POSITIVE X
                       WHEN IND = 0
*/
    erfc1 = 0.0e0;
    return erfc1;
}

double exparg(int *l)
/*
--------------------------------------------------------------------
     IF L = 0 THEN  EXPARG(L) = THE LARGEST POSITIVE W FOR WHICH
     EXP(W) CAN BE COMPUTED.
 
     IF L IS NONZERO THEN  EXPARG(L) = THE LARGEST NEGATIVE W FOR
     WHICH THE COMPUTED VALUE OF EXP(W) IS NONZERO.
 
     NOTE... ONLY AN APPROXIMATE VALUE FOR EXPARG(L) IS NEEDED.
--------------------------------------------------------------------
*/
{
static int K1 = 4;
static int K2 = 9;
static int K3 = 10;
static double lnb;
static int b,m;
/*
     ..
     .. Executable Statements ..
*/
    b = ipmpar(&K1);
    if(b != 2) goto S10;
    lnb = .69314718055995e0;
    goto S40;
S10:
    if(b != 8) goto S20;
    lnb = 2.0794415416798e0;
    goto S40;
S20:
    if(b != 16) goto S30;
    lnb = 2.7725887222398e0;
    goto S40;
S30:
    lnb = log((double)b);
S40:
    if(*l == 0) goto S50;
    m = ipmpar(&K2)-1;
    return 0.99999e0*((double)m*lnb);
S50:
    m = ipmpar(&K3);
    return 0.99999e0*((double)m*lnb);
}

double gam1(double *a)
/*
     ------------------------------------------------------------------
     COMPUTATION OF 1/GAMMA(A+1) - 1  FOR -0.5 .LE. A .LE. 1.5
     ------------------------------------------------------------------
*/
{
static double s1 = .273076135303957e+00;
static double s2 = .559398236957378e-01;
static double p[7] = {
    .577215664901533e+00,-.409078193005776e+00,-.230975380857675e+00,
    .597275330452234e-01,.766968181649490e-02,-.514889771323592e-02,
    .589597428611429e-03
};
static double q[5] = {
    .100000000000000e+01,.427569613095214e+00,.158451672430138e+00,
    .261132021441447e-01,.423244297896961e-02
};
static double r[9] = {
    -.422784335098468e+00,-.771330383816272e+00,-.244757765222226e+00,
    .118378989872749e+00,.930357293360349e-03,-.118290993445146e-01,
    .223047661158249e-02,.266505979058923e-03,-.132674909766242e-03
};
static double bot,d,t,top,w,T1;
/*
     ..
     .. Executable Statements ..
*/
    t = *a;
    d = *a-0.5e0;
    if(d > 0.0e0) t = d-0.5e0;
    T1 = t;
    if(T1 < 0) goto S40;
    else if(EQ(T1, 0)) goto S10;
    else  goto S20;
S10:
    return 0.0e0;
S20:
    top = (((((p[6]*t+p[5])*t+p[4])*t+p[3])*t+p[2])*t+p[1])*t+p[0];
    bot = (((q[4]*t+q[3])*t+q[2])*t+q[1])*t+1.0e0;
    w = top/bot;
    if(d > 0.0e0) goto S30;
    return *a*w;
S30:
    return t/ *a*(w-0.5e0-0.5e0);
S40:
    top = (((((((r[8]*t+r[7])*t+r[6])*t+r[5])*t+r[4])*t+r[3])*t+r[2])*t+r[1])*t+
      r[0];
    bot = (s2*t+s1)*t+1.0e0;
    w = top/bot;
    if(d > 0.0e0) goto S50;
    return *a*(w+0.5e0+0.5e0);
S50:
    return t*w/ *a;
}

double Xgamm(double *a)
/*
-----------------------------------------------------------------------
 
         EVALUATION OF THE GAMMA FUNCTION FOR REAL ARGUMENTS
 
                           -----------
 
     GAMMA(A) IS ASSIGNED THE VALUE 0 WHEN THE GAMMA FUNCTION CANNOT
     BE COMPUTED.
 
-----------------------------------------------------------------------
     WRITTEN BY ALFRED H. MORRIS, JR.
          NAVAL SURFACE WEAPONS CENTER
          DAHLGREN, VIRGINIA
-----------------------------------------------------------------------
*/
{
static double d = .41893853320467274178e0;
static double pi = 3.1415926535898e0;
static double r1 = .820756370353826e-03;
static double r2 = -.595156336428591e-03;
static double r3 = .793650663183693e-03;
static double r4 = -.277777777770481e-02;
static double r5 = .833333333333333e-01;
static double p[7] = {
    .539637273585445e-03,.261939260042690e-02,.204493667594920e-01,
    .730981088720487e-01,.279648642639792e+00,.553413866010467e+00,1.0e0
};
static double q[7] = {
    -.832979206704073e-03,.470059485860584e-02,.225211131035340e-01,
    -.170458969313360e+00,-.567902761974940e-01,.113062953091122e+01,1.0e0
};
static int K2 = 3;
static int K3 = 0;
static double Xgamm,bot,g,lnx,s,t,top,w,x,z;
static int i,j,m,n,T1;
/*
     ..
     .. Executable Statements ..
*/
    Xgamm = 0.0e0;
    x = *a;
    if(fabs(*a) >= 15.0e0) goto S110;
/*
-----------------------------------------------------------------------
            EVALUATION OF GAMMA(A) FOR ABS(A) .LT. 15
-----------------------------------------------------------------------
*/
    t = 1.0e0;
    m = fifidint(*a)-1;
/*
     LET T BE THE PRODUCT OF A-J WHEN A .GE. 2
*/
    T1 = m;
    if(T1 < 0) goto S40;
    else if(T1 == 0) goto S30;
    else  goto S10;
S10:
    for(j=1; j<=m; j++) {
        x -= 1.0e0;
        t = x*t;
    }
S30:
    x -= 1.0e0;
    goto S80;
S40:
/*
     LET T BE THE PRODUCT OF A+J WHEN A .LT. 1
*/
    t = *a;
    if(*a > 0.0e0) goto S70;
    m = -m-1;
    if(m == 0) goto S60;
    for(j=1; j<=m; j++) {
        x += 1.0e0;
        t = x*t;
    }
S60:
    x += (0.5e0+0.5e0);
    t = x*t;
    if(EQ(t, 0.0e0)) return Xgamm;
S70:
/*
     THE FOLLOWING CODE CHECKS IF 1/T CAN OVERFLOW. THIS
     CODE MAY BE OMITTED IF DESIRED.
*/
    if(fabs(t) >= 1.e-30) goto S80;
    if(fabs(t)*spmpar(&K2) <= 1.0001e0) return Xgamm;
    Xgamm = 1.0e0/t;
    return Xgamm;
S80:
/*
     COMPUTE GAMMA(1 + X) FOR  0 .LE. X .LT. 1
*/
    top = p[0];
    bot = q[0];
    for(i=1; i<7; i++) {
        top = p[i]+x*top;
        bot = q[i]+x*bot;
    }
    Xgamm = top/bot;
/*
     TERMINATION
*/
    if(*a < 1.0e0) goto S100;
    Xgamm *= t;
    return Xgamm;
S100:
    Xgamm /= t;
    return Xgamm;
S110:
/*
-----------------------------------------------------------------------
            EVALUATION OF GAMMA(A) FOR ABS(A) .GE. 15
-----------------------------------------------------------------------
*/
    if(fabs(*a) >= 1.e3) return Xgamm;
    if(*a > 0.0e0) goto S120;
    x = -*a;
    n = (long)(x);
    t = x-(double)n;
    if(t > 0.9e0) t = 1.0e0-t;
    s = sin(pi*t)/pi;
    if(fifmod(n,2) == 0) s = -s;
    if(EQ(s, 0.0e0)) return Xgamm;
S120:
/*
     COMPUTE THE MODIFIED ASYMPTOTIC SUM
*/
    t = 1.0e0/(x*x);
    g = ((((r1*t+r2)*t+r3)*t+r4)*t+r5)/x;
/*
     ONE MAY REPLACE THE NEXT STATEMENT WITH  LNX = ALOG(X)
     BUT LESS ACCURACY WILL NORMALLY BE OBTAINED.
*/
    lnx = log(x);
/*
     FINAL ASSEMBLY
*/
    z = x;
    g = d+g+(z-0.5e0)*(lnx-1.e0);
    w = g;
    t = g-w;
    if(w > 0.99999e0*exparg(&K3)) return Xgamm;
    Xgamm = exp(w)*(1.0e0+t);
    if(*a < 0.0e0) Xgamm = 1.0e0/(Xgamm*s)/x;
    return Xgamm;
}

void gratio(double *a,double *x,double *ans,double *qans,int *ind)
/*
 ----------------------------------------------------------------------
        EVALUATION OF THE INCOMPLETE GAMMA RATIO FUNCTIONS
                      P(A,X) AND Q(A,X)
 
                        ----------
 
     IT IS ASSUMED THAT A AND X ARE NONNEGATIVE, WHERE A AND X
     ARE NOT BOTH 0.
 
     ANS AND QANS ARE VARIABLES. GRATIO ASSIGNS ANS THE VALUE
     P(A,X) AND QANS THE VALUE Q(A,X). IND MAY BE ANY INTEGER.
     IF IND = 0 THEN THE USER IS REQUESTING AS MUCH ACCURACY AS
     POSSIBLE (UP TO 14 SIGNIFICANT DIGITS). OTHERWISE, IF
     IND = 1 THEN ACCURACY IS REQUESTED TO WITHIN 1 UNIT OF THE
     6-TH SIGNIFICANT DIGIT, AND IF IND .NE. 0,1 THEN ACCURACY
     IS REQUESTED TO WITHIN 1 UNIT OF THE 3RD SIGNIFICANT DIGIT.
 
     ERROR RETURN ...
        ANS IS ASSIGNED THE VALUE 2 WHEN A OR X IS NEGATIVE,
     WHEN A*X = 0, OR WHEN P(A,X) AND Q(A,X) ARE INDETERMINANT.
     P(A,X) AND Q(A,X) ARE COMPUTATIONALLY INDETERMINANT WHEN
     X IS EXCEEDINGLY CLOSE TO A AND A IS EXTREMELY LARGE.
 ----------------------------------------------------------------------
     WRITTEN BY ALFRED H. MORRIS, JR.
        NAVAL SURFACE WEAPONS CENTER
        DAHLGREN, VIRGINIA
     --------------------
*/
{
static double alog10 = 2.30258509299405e0;
static double d10 = -.185185185185185e-02;
static double d20 = .413359788359788e-02;
static double d30 = .649434156378601e-03;
static double d40 = -.861888290916712e-03;
static double d50 = -.336798553366358e-03;
static double d60 = .531307936463992e-03;
static double d70 = .344367606892378e-03;
static double rt2pin = .398942280401433e0;
static double rtpi = 1.77245385090552e0;
static double third = .333333333333333e0;
static double acc0[3] = {
    5.e-15,5.e-7,5.e-4
};
static double big[3] = {
    20.0e0,14.0e0,10.0e0
};
static double d0[13] = {
    .833333333333333e-01,-.148148148148148e-01,.115740740740741e-02,
    .352733686067019e-03,-.178755144032922e-03,.391926317852244e-04,
    -.218544851067999e-05,-.185406221071516e-05,.829671134095309e-06,
    -.176659527368261e-06,.670785354340150e-08,.102618097842403e-07,
    -.438203601845335e-08
};
static double d1[12] = {
    -.347222222222222e-02,.264550264550265e-02,-.990226337448560e-03,
    .205761316872428e-03,-.401877572016461e-06,-.180985503344900e-04,
    .764916091608111e-05,-.161209008945634e-05,.464712780280743e-08,
    .137863344691572e-06,-.575254560351770e-07,.119516285997781e-07
};
static double d2[10] = {
    -.268132716049383e-02,.771604938271605e-03,.200938786008230e-05,
    -.107366532263652e-03,.529234488291201e-04,-.127606351886187e-04,
    .342357873409614e-07,.137219573090629e-05,-.629899213838006e-06,
    .142806142060642e-06
};
static double d3[8] = {
    .229472093621399e-03,-.469189494395256e-03,.267720632062839e-03,
    -.756180167188398e-04,-.239650511386730e-06,.110826541153473e-04,
    -.567495282699160e-05,.142309007324359e-05
};
static double d4[6] = {
    .784039221720067e-03,-.299072480303190e-03,-.146384525788434e-05,
    .664149821546512e-04,-.396836504717943e-04,.113757269706784e-04
};
static double d5[4] = {
    -.697281375836586e-04,.277275324495939e-03,-.199325705161888e-03,
    .679778047793721e-04
};
static double d6[2] = {
    -.592166437353694e-03,.270878209671804e-03
};
static double e00[3] = {
    .25e-3,.25e-1,.14e0
};
static double x00[3] = {
    31.0e0,17.0e0,9.7e0
};
static int K1 = 1;
static int K2 = 0;
static double a2n,a2nm1,acc,am0,amn,an,an0,apn,b2n,b2nm1,c,c0,c1,c2,c3,c4,c5,c6,
    cma,e,e0,g,h,j,l,r,rta,rtx,s,sum,t,t1,tol,twoa,u,w,x0,y,z;
static int i,iop,m,max,n;
static double wk[20],T3;
static int T4,T5;
static double T6,T7;
/*
     ..
     .. Executable Statements ..
*/
/*
     --------------------
     ****** E IS A MACHINE DEPENDENT CONSTANT. E IS THE SMALLEST
            FLOATING POINT NUMBER FOR WHICH 1.0 + E .GT. 1.0 .
*/
    e = spmpar(&K1);
    if(*a < 0.0e0 || *x < 0.0e0) goto S430;
    if(EQ(*a, 0.0e0) && EQ(*x, 0.0e0)) goto S430;
    if(EQ(*a**x, 0.0e0)) goto S420;
    iop = *ind+1;
    if(iop != 1 && iop != 2) iop = 3;
    acc = fifdmax1(acc0[iop-1],e);
    e0 = e00[iop-1];
    x0 = x00[iop-1];
/*
            SELECT THE APPROPRIATE ALGORITHM
*/
    if(*a >= 1.0e0) goto S10;
    if(EQ(*a, 0.5e0)) goto S390;
    if(*x < 1.1e0) goto S160;
    t1 = *a*log(*x)-*x;
    u = *a*exp(t1);
    if(EQ(u, 0.0e0)) goto S380;
    r = u*(1.0e0+gam1(a));
    goto S250;
S10:
    if(*a >= big[iop-1]) goto S30;
    if(*a > *x || *x >= x0) goto S20;
    twoa = *a+*a;
    m = fifidint(twoa);
    if(NE(twoa, (double)m)) goto S20;
    i = m/2;
    if(EQ(*a, (double)i)) goto S210;
    goto S220;
S20:
    t1 = *a*log(*x)-*x;
    r = exp(t1)/Xgamm(a);
    goto S40;
S30:
    l = *x/ *a;
    if(EQ(l, 0.0e0)) goto S370;
    s = 0.5e0+(0.5e0-l);
    z = rlog(&l);
    if(z >= 700.0e0/ *a) goto S410;
    y = *a*z;
    rta = sqrt(*a);
    if(fabs(s) <= e0/rta) goto S330;
    if(fabs(s) <= 0.4e0) goto S270;
    t = pow(1.0e0/ *a,2.0);
    t1 = (((0.75e0*t-1.0e0)*t+3.5e0)*t-105.0e0)/(*a*1260.0e0);
    t1 -= y;
    r = rt2pin*rta*exp(t1);
S40:
    if(EQ(r, 0.0e0)) goto S420;
    if(*x <= fifdmax1(*a,alog10)) goto S50;
    if(*x < x0) goto S250;
    goto S100;
S50:
/*
                 TAYLOR SERIES FOR P/R
*/
    apn = *a+1.0e0;
    t = *x/apn;
    wk[0] = t;
    for(n=2; n<=20; n++) {
        apn += 1.0e0;
        t *= (*x/apn);
        if(t <= 1.e-3) goto S70;
        wk[n-1] = t;
    }
    n = 20;
S70:
    sum = t;
    tol = 0.5e0*acc;
S80:
    apn += 1.0e0;
    t *= (*x/apn);
    sum += t;
    if(t > tol) goto S80;
    max = n-1;
    for(m=1; m<=max; m++) {
        n -= 1;
        sum += wk[n-1];
    }
    *ans = r/ *a*(1.0e0+sum);
    *qans = 0.5e0+(0.5e0-*ans);
    return;
S100:
/*
                 ASYMPTOTIC EXPANSION
*/
    amn = *a-1.0e0;
    t = amn/ *x;
    wk[0] = t;
    for(n=2; n<=20; n++) {
        amn -= 1.0e0;
        t *= (amn/ *x);
        if(fabs(t) <= 1.e-3) goto S120;
        wk[n-1] = t;
    }
    n = 20;
S120:
    sum = t;
S130:
    if(fabs(t) <= acc) goto S140;
    amn -= 1.0e0;
    t *= (amn/ *x);
    sum += t;
    goto S130;
S140:
    max = n-1;
    for(m=1; m<=max; m++) {
        n -= 1;
        sum += wk[n-1];
    }
    *qans = r/ *x*(1.0e0+sum);
    *ans = 0.5e0+(0.5e0-*qans);
    return;
S160:
/*
             TAYLOR SERIES FOR P(A,X)/X**A
*/
    an = 3.0e0;
    c = *x;
    sum = *x/(*a+3.0e0);
    tol = 3.0e0*acc/(*a+1.0e0);
S170:
    an += 1.0e0;
    c = -(c*(*x/an));
    t = c/(*a+an);
    sum += t;
    if(fabs(t) > tol) goto S170;
    j = *a**x*((sum/6.0e0-0.5e0/(*a+2.0e0))**x+1.0e0/(*a+1.0e0));
    z = *a*log(*x);
    h = gam1(a);
    g = 1.0e0+h;
    if(*x < 0.25e0) goto S180;
    if(*a < *x/2.59e0) goto S200;
    goto S190;
S180:
    if(z > -.13394e0) goto S200;
S190:
    w = exp(z);
    *ans = w*g*(0.5e0+(0.5e0-j));
    *qans = 0.5e0+(0.5e0-*ans);
    return;
S200:
    l = rexp(&z);
    w = 0.5e0+(0.5e0+l);
    *qans = (w*j-l)*g-h;
    if(*qans < 0.0e0) goto S380;
    *ans = 0.5e0+(0.5e0-*qans);
    return;
S210:
/*
             FINITE SUMS FOR Q WHEN A .GE. 1
                 AND 2*A IS AN INTEGER
*/
    sum = exp(-*x);
    t = sum;
    n = 1;
    c = 0.0e0;
    goto S230;
S220:
    rtx = sqrt(*x);
    sum = erfc1(&K2,&rtx);
    t = exp(-*x)/(rtpi*rtx);
    n = 0;
    c = -0.5e0;
S230:
    if(n == i) goto S240;
    n += 1;
    c += 1.0e0;
    t = *x*t/c;
    sum += t;
    goto S230;
S240:
    *qans = sum;
    *ans = 0.5e0+(0.5e0-*qans);
    return;
S250:
/*
              CONTINUED FRACTION EXPANSION
*/
    tol = fifdmax1(5.0e0*e,acc);
    a2nm1 = a2n = 1.0e0;
    b2nm1 = *x;
    b2n = *x+(1.0e0-*a);
    c = 1.0e0;
S260:
    a2nm1 = *x*a2n+c*a2nm1;
    b2nm1 = *x*b2n+c*b2nm1;
    am0 = a2nm1/b2nm1;
    c += 1.0e0;
    cma = c-*a;
    a2n = a2nm1+cma*a2n;
    b2n = b2nm1+cma*b2n;
    an0 = a2n/b2n;
    if(fabs(an0-am0) >= tol*an0) goto S260;
    *qans = r*an0;
    *ans = 0.5e0+(0.5e0-*qans);
    return;
S270:
/*
                GENERAL TEMME EXPANSION
*/
    if(fabs(s) <= 2.0e0*e && *a*e*e > 3.28e-3) goto S430;
    c = exp(-y);
    T3 = sqrt(y);
    w = 0.5e0*erfc1(&K1,&T3);
    u = 1.0e0/ *a;
    z = sqrt(z+z);
    if(l < 1.0e0) z = -z;
    T4 = iop-2;
    if(T4 < 0) goto S280;
    else if(T4 == 0) goto S290;
    else  goto S300;
S280:
    if(fabs(s) <= 1.e-3) goto S340;
    c0 = ((((((((((((d0[12]*z+d0[11])*z+d0[10])*z+d0[9])*z+d0[8])*z+d0[7])*z+d0[
      6])*z+d0[5])*z+d0[4])*z+d0[3])*z+d0[2])*z+d0[1])*z+d0[0])*z-third;
    c1 = (((((((((((d1[11]*z+d1[10])*z+d1[9])*z+d1[8])*z+d1[7])*z+d1[6])*z+d1[5]
      )*z+d1[4])*z+d1[3])*z+d1[2])*z+d1[1])*z+d1[0])*z+d10;
    c2 = (((((((((d2[9]*z+d2[8])*z+d2[7])*z+d2[6])*z+d2[5])*z+d2[4])*z+d2[3])*z+
      d2[2])*z+d2[1])*z+d2[0])*z+d20;
    c3 = (((((((d3[7]*z+d3[6])*z+d3[5])*z+d3[4])*z+d3[3])*z+d3[2])*z+d3[1])*z+
      d3[0])*z+d30;
    c4 = (((((d4[5]*z+d4[4])*z+d4[3])*z+d4[2])*z+d4[1])*z+d4[0])*z+d40;
    c5 = (((d5[3]*z+d5[2])*z+d5[1])*z+d5[0])*z+d50;
    c6 = (d6[1]*z+d6[0])*z+d60;
    t = ((((((d70*u+c6)*u+c5)*u+c4)*u+c3)*u+c2)*u+c1)*u+c0;
    goto S310;
S290:
    c0 = (((((d0[5]*z+d0[4])*z+d0[3])*z+d0[2])*z+d0[1])*z+d0[0])*z-third;
    c1 = (((d1[3]*z+d1[2])*z+d1[1])*z+d1[0])*z+d10;
    c2 = d2[0]*z+d20;
    t = (c2*u+c1)*u+c0;
    goto S310;
S300:
    t = ((d0[2]*z+d0[1])*z+d0[0])*z-third;
S310:
    if(l < 1.0e0) goto S320;
    *qans = c*(w+rt2pin*t/rta);
    *ans = 0.5e0+(0.5e0-*qans);
    return;
S320:
    *ans = c*(w-rt2pin*t/rta);
    *qans = 0.5e0+(0.5e0-*ans);
    return;
S330:
/*
               TEMME EXPANSION FOR L = 1
*/
    if(*a*e*e > 3.28e-3) goto S430;
    c = 0.5e0+(0.5e0-y);
    w = (0.5e0-sqrt(y)*(0.5e0+(0.5e0-y/3.0e0))/rtpi)/c;
    u = 1.0e0/ *a;
    z = sqrt(z+z);
    if(l < 1.0e0) z = -z;
    T5 = iop-2;
    if(T5 < 0) goto S340;
    else if(T5 == 0) goto S350;
    else  goto S360;
S340:
    c0 = ((((((d0[6]*z+d0[5])*z+d0[4])*z+d0[3])*z+d0[2])*z+d0[1])*z+d0[0])*z-
      third;
    c1 = (((((d1[5]*z+d1[4])*z+d1[3])*z+d1[2])*z+d1[1])*z+d1[0])*z+d10;
    c2 = ((((d2[4]*z+d2[3])*z+d2[2])*z+d2[1])*z+d2[0])*z+d20;
    c3 = (((d3[3]*z+d3[2])*z+d3[1])*z+d3[0])*z+d30;
    c4 = (d4[1]*z+d4[0])*z+d40;
    c5 = (d5[1]*z+d5[0])*z+d50;
    c6 = d6[0]*z+d60;
    t = ((((((d70*u+c6)*u+c5)*u+c4)*u+c3)*u+c2)*u+c1)*u+c0;
    goto S310;
S350:
    c0 = (d0[1]*z+d0[0])*z-third;
    c1 = d1[0]*z+d10;
    t = (d20*u+c1)*u+c0;
    goto S310;
S360:
    t = d0[0]*z-third;
    goto S310;
S370:
/*
                     SPECIAL CASES
*/
    *ans = 0.0e0;
    *qans = 1.0e0;
    return;
S380:
    *ans = 1.0e0;
    *qans = 0.0e0;
    return;
S390:
    if(*x >= 0.25e0) goto S400;
    T6 = sqrt(*x);
    *ans = erf1(&T6);
    *qans = 0.5e0+(0.5e0-*ans);
    return;
S400:
    T7 = sqrt(*x);
    *qans = erfc1(&K2,&T7);
    *ans = 0.5e0+(0.5e0-*qans);
    return;
S410:
    if(fabs(s) <= 2.0e0*e) goto S430;
S420:
    if(*x <= *a) goto S370;
    goto S380;
S430:
/*
                     ERROR RETURN
*/
    *ans = 2.0e0;
    return;
}

double rexp(double *x)
/*
-----------------------------------------------------------------------
            EVALUATION OF THE FUNCTION EXP(X) - 1
-----------------------------------------------------------------------
*/
{
static double p1 = .914041914819518e-09;
static double p2 = .238082361044469e-01;
static double q1 = -.499999999085958e+00;
static double q2 = .107141568980644e+00;
static double q3 = -.119041179760821e-01;
static double q4 = .595130811860248e-03;
static double rexp,w;
/*
     ..
     .. Executable Statements ..
*/
    if(fabs(*x) > 0.15e0) goto S10;
    rexp = *x*(((p2**x+p1)**x+1.0e0)/((((q4**x+q3)**x+q2)**x+q1)**x+1.0e0));
    return rexp;
S10:
    w = exp(*x);
    if(*x > 0.0e0) goto S20;
    rexp = w-0.5e0-0.5e0;
    return rexp;
S20:
    rexp = w*(0.5e0+(0.5e0-1.0e0/w));
    return rexp;
}

double rlog(double *x)
/*
     -------------------
     COMPUTATION OF  X - 1 - LN(X)
     -------------------
*/
{
static double a = .566749439387324e-01;
static double b = .456512608815524e-01;
static double p0 = .333333333333333e+00;
static double p1 = -.224696413112536e+00;
static double p2 = .620886815375787e-02;
static double q1 = -.127408923933623e+01;
static double q2 = .354508718369557e+00;
static double r,t,u,w,w1;
/*
     ..
     .. Executable Statements ..
*/
    if(*x < 0.61e0 || *x > 1.57e0) goto S40;
    if(*x < 0.82e0) goto S10;
    if(*x > 1.18e0) goto S20;
/*
              ARGUMENT REDUCTION
*/
    u = *x-0.5e0-0.5e0;
    w1 = 0.0e0;
    goto S30;
S10:
    u = *x-0.7e0;
    u /= 0.7e0;
    w1 = a-u*0.3e0;
    goto S30;
S20:
    u = 0.75e0**x-1.e0;
    w1 = b+u/3.0e0;
S30:
/*
               SERIES EXPANSION
*/
    r = u/(u+2.0e0);
    t = r*r;
    w = ((p2*t+p1)*t+p0)/((q2*t+q1)*t+1.0e0);
    return 2.0e0*t*(1.0e0/(1.0e0-r)-r*w)+w1;
S40:
    r = *x-0.5e0-0.5e0;
    return r-log(*x);
}

double spmpar(int *i)
/*
-----------------------------------------------------------------------
 
     SPMPAR PROVIDES THE SINGLE PRECISION MACHINE CONSTANTS FOR
     THE COMPUTER BEING USED. IT IS ASSUMED THAT THE ARGUMENT
     I IS AN INTEGER HAVING ONE OF THE VALUES 1, 2, OR 3. IF THE
     SINGLE PRECISION ARITHMETIC BEING USED HAS M BASE B DIGITS AND
     ITS SMALLEST AND LARGEST EXPONENTS ARE EMIN AND EMAX, THEN
 
        SPMPAR(1) = B**(1 - M), THE MACHINE PRECISION,
 
        SPMPAR(2) = B**(EMIN - 1), THE SMALLEST MAGNITUDE,
 
        SPMPAR(3) = B**EMAX*(1 - B**(-M)), THE LARGEST MAGNITUDE.
 
-----------------------------------------------------------------------
     WRITTEN BY
        ALFRED H. MORRIS, JR.
        NAVAL SURFACE WARFARE CENTER
        DAHLGREN VIRGINIA
-----------------------------------------------------------------------
-----------------------------------------------------------------------
     MODIFIED BY BARRY W. BROWN TO RETURN DOUBLE PRECISION MACHINE
     CONSTANTS FOR THE COMPUTER BEING USED.  THIS MODIFICATION WAS
     MADE AS PART OF CONVERTING BRATIO TO DOUBLE PRECISION
-----------------------------------------------------------------------
*/
{
static int K1 = 4;
static int K2 = 8;
static int K3 = 9;
static int K4 = 10;
static double b,binv,bm1,one,w,z;
static int emax,emin,ibeta,m;
/*
     ..
     .. Executable Statements ..
*/
    if(*i > 1) goto S10;
    b = ipmpar(&K1);
    m = ipmpar(&K2);
    return pow(b,(double)(1-m));
S10:
    if(*i > 2) goto S20;
    b = ipmpar(&K1);
    emin = ipmpar(&K3);
    one = 1.0;
    binv = one/b;
    w = pow(b,(double)(emin+2));
    return w*binv*binv*binv;
S20:
    ibeta = ipmpar(&K1);
    m = ipmpar(&K2);
    emax = ipmpar(&K4);
    b = ibeta;
    bm1 = ibeta-1;
    one = 1.0;
    z = pow(b,(double)(m-1));
    w = ((z-one)*b+bm1)/(b*z);
    z = pow(b,(double)(emax-2));
    return w*z*b*b;
}

/************************************************************************
FIFDINT:
Truncates a double precision number to an integer and returns the
value in a double.
************************************************************************/


/************************************************************************
FIFDMAX1:
returns the maximum of two numbers a and b
************************************************************************/
double fifdmax1(double a,double b)
/* a     -      first number */
/* b     -      second number */
{
  if (a < b) return b;
  else return a;
}

/************************************************************************
FIFDMIN1:
returns the minimum of two numbers a and b
************************************************************************/
double fifdmin1(double a,double b)
/* a     -     first number */
/* b     -     second number */
{
  if (a < b) return a;
  else return b;
}

/************************************************************************
FIFDSIGN:
transfers the sign of the variable "sign" to the variable "mag"
************************************************************************/
double fifdsign(double mag,double sign)
/* mag     -     magnitude */
/* sign    -     sign to be transfered */
{
  if (mag < 0) mag = -mag;
  if (sign < 0) mag = -mag;
  return mag;

}

/************************************************************************
FIFIDINT:
Truncates a double precision number to a long integer
************************************************************************/
long fifidint(double a)
/* a - number to be truncated */
{
  return (long)(a);
}

/************************************************************************
FIFMOD:
returns the modulo of a and b
************************************************************************/
long fifmod(long a,long b)
/* a - numerator */
/* b - denominator */
{
  return a % b;
}

/************************************************************************
FTNSTOP:
Prints msg to standard error and then exits
************************************************************************/
void ftnstop(char* msg)
/* msg - error message */
{
  if (msg != NULL) fprintf(stderr,"%s\n",msg);
  exit(EXIT_FAILURE); /* EXIT_FAILURE from stdlib.h, or use an int */
}
