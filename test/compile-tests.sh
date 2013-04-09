BASEDIR=$(dirname $0)

   pawncc -\;+ -\(+ -O0 -d0 -v0 -D$BASEDIR -oserver/filterscripts/et-00.amx exceptions-test.pwn \
&& pawncc -\;+ -\(+ -O0 -d1 -v0 -D$BASEDIR -oserver/filterscripts/et-01.amx exceptions-test.pwn \
&& pawncc -\;+ -\(+ -O0 -d2 -v0 -D$BASEDIR -oserver/filterscripts/et-02.amx exceptions-test.pwn \
&& pawncc -\;+ -\(+ -O1 -d0 -v0 -D$BASEDIR -oserver/filterscripts/et-10.amx exceptions-test.pwn \
&& pawncc -\;+ -\(+ -O1 -d1 -v0 -D$BASEDIR -oserver/filterscripts/et-11.amx exceptions-test.pwn \
&& pawncc -\;+ -\(+ -O1 -d2 -v0 -D$BASEDIR -oserver/filterscripts/et-12.amx exceptions-test.pwn
