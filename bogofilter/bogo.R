# bogo.R - check calculations performed by bogofilter
#
# Before running this script,
# prepare file bogo.tbl as follows: run
#   bogofilter -R <mailbox >bogo.tbl
# where mailbox is a file with only one email message in it.

# Then run R and type
#   source("bogo.R")

# first read the data and initialize parameters s and x (ROBS and ROBX)
bogo <- read.table("bogo.tbl")
attach(bogo)
s <- 0.001
x <- 0.415

# next recalculate the fw values from the counts in the table
fw2 <- (s * x + pbad) / (s + pbad + pgood)

# display fw (calculated by bogofilter) and fw2 and compare
print.noquote("Bogofilter f(w):")
print(fw)
print.noquote("")
print.noquote("R f(w):")
print(fw2)
print.noquote("")
print.noquote("Difference (R - bogo):")
print(fw2 - fw)
# (Of course they will differ in the last value; the last fw is actually
# S, and the last fw2 is meaningless.)

# calculate S using fw2 but excluding the last value in the vector
P <- 1 - exp( (sum(log(1-fw2)) - log(1-fw2[197])) / 196 )
Q <- 1 - exp( (sum(log(fw2)) - log(fw2[197])) / 196 )
S <- ( 1 + (P-Q)/(P+Q) ) / 2

# display S as calculated by bogofilter and by R and compare
print.noquote("")
print.noquote("Bogofilter S:")
print(fw[length(fw)])
print.noquote("")
print.noquote("R S:")
print(S)
print.noquote("")
print.noquote("Difference (R - bogo):")
print(S - fw[length(fw)])
