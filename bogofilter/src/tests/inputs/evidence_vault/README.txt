lexer-overflow:	    (added by Matthias Andree)
---------------

Provided by Julius Plenz, FU Berlin, Germany, 2010-07-21.

Reported 2010-07-21 by Julius Plenz, after a colleague of his fuzzed
bogofilter with 18 million messages the previous week-end. These eight
cause bogofilter to exit with status 3 after reporting "flex scanner
push-back overflow".

Obtained from:
http://userpage.fu-berlin.de/~plenz/bogofilter/lexer-overflows.tar.gz

Proposed action:
- identify the culprit in the lexer ensemble
- fix it
- integrate these into the self-test suite
