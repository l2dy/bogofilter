#! /bin/sh /usr/bin/setR
# This file is smindev.R, an R script to perform the data reduction for
# experiments in which min_dev and s are varied over a wide range of
# values; for each combination, a spam cutoff is first determined such
# that there are no more than some target number of false positives when
# nonspam files are pooled and evaluated, and with that cutoff, the number
# of false negatives is determined for each spam file.

#
# see http://www.bgl.nu/bogofilter/smindev3.html#appA for more information
#

#                                                              /**/
# For use in R, parms.tbl from runex is expected in bogolog/smindev.tbl
# by default; otherwise run ./smindev.R filename

graphics.off(); setwd("/proj/Rwork")
if(length(argv) > 0) fn <- argv[1] else fn <- "bogolog/smindev.tbl"
if(file.exists(fn) == FALSE) stop(paste("file", fn, "not found"))
if(length(argv) > 1) sub <- argv[2] else sub <- "--"

### First read the message counts:
read.table(fn, nrows=2) -> meta
msgcount <- sum(apply(meta[,2:length(meta)],1,mean))

### Now read the data 
parms <- read.table(fn, col.names=c("s", "md", "cutoff", "run", "fp", "fn"),
  skip=2)

### Get axis values and number of replicates
sval <- sort(unique(parms$s), decreasing=TRUE)
x <- -log10(sval)
y <- sort(unique(parms$md))
n <- length(unique(parms$run))

### Express error in percentage and perform an anova
parms$percent = (parms$fp + parms$fn) * 100 / msgcount
parms$s = factor(parms$s)
parms$md = factor(parms$md)
paov <- aov(percent ~ s + md + s*md, data=parms)
print(summary(paov))

### Now express results as mean percent correct
pcs <- array(parms$percent, dim=c(n, length(parms$percent)/n))
meanerr <- apply(pcs, 2, mean)
cutoffs <- array(parms$cutoff, dim=c(n, length(parms$cutoff)/n))[1,]

### Create a data frame with these results
parmres <- data.frame(rs=rep(sval, each=length(parms$s)/(n*length(sval))),
  md=rep(y,length(sval)), cutoff=cutoffs, percent=meanerr)

### calculate the z-axis values for percent correct and for cutoffs
z <- t(array(100 - parmres$percent, dim=c(length(parms$md)/(n * length(sval)),
  length(sval))))
co <- t(array(parmres$cutoff, dim=c(length(parms$md)/(n * length(sval)),
  length(sval))))

X11(width=4.5, height=4.5)

### produce a trial plot, making it easy to try other rotation values
pplot <- function(th,ph) {
  persp(x, y, z, ticktype="detailed", theta=th, phi=ph,
  main="Percent correct vs s and mindev", sub=sub,
  xlab="-log(10) s", ylab="mindev", zlim=c(90,100),
  zlab="percent correct", shade=0.6, border=4, r=sqrt(2), d=2.5)
}

pplot(70,15)

### another trial plot, this time for cutoffs
X11(width=4.5, height=4.5)

qplot <- function(th,ph) {
  persp(x, y, co, ticktype="detailed", theta=th, phi=ph,
  main="Cutoff vs s and mindev", sub=sub, xlab="-log(10) s",
  ylab="mindev", zlab="cutoff", shade=0.6, border=4, r=sqrt(2), d=2.5)
}

qplot(70,15)

### get the 15 best combinations of s and mindev
sortlist <- sort(parmres$percent, index.return=TRUE)
system("echo")
print(parmres[sortlist$ix,][1:15,])
