#Make file for VMS backup program
#

##############################
# Choose line depending if you want to have remote tape support
#
REMOTE=
#REMOTE=-DREMOTE                        # -DREMOTE  use remote tape
##############################
# Set this if you want to use long option names
#
#LONGOPT=
LONGOPT=-DHAVE_GETOPTLONG
#
##############################
# Choose one of these two sets of lines depending on if you have
# the starlet library available.
#
# Choose this set if you do NOT have starlet available
#
CFLAGS=$(REMOTE) $(LONGOPT) -Wall -fdollars-in-identifiers -g
LDLIBS=
#
# Choose this set if you DO have starlet available
#
#STARLETDIR=/home/kevin/basic/starlet
#CFLAGS=$(REMOTE) $(LONGOPT) -fdollars-in-identifiers -I $(STARLETDIR) -DHAVE_STARLET -g -DDEBUG
#LDLIBS=$(STARLETDIR)/starlet.a
#
##############################
#
OWNER=tar                      # user for remote tape access
MODE=4755
BINDIR=/usr/bin
MANSEC=1
MANDIR=/usr/share/man/man$(MANSEC)
DISTFILES=README vmsbackup.1 Makefile vmsbackup.c match.c NEWS  build.com dclmain.c getoptmain.c vmsbackup.cld vmsbackup.h  sysdep.h

vmsbackup: vmsbackup.o match.o getoptmain.o

vmsbackup.o : vmsbackup.c
match.o : match.c
getoptmain.o : getoptmain.c

install:
	install -m $(MODE) -o $(OWNER) -s vmsbackup $(BINDIR)
	cp vmsbackup.1 $(MANDIR)/vmsbackup.$(MANSEC)

clean:
	rm -f vmsbackup *.o core

shar:
	shar -a $(DISTFILES) > vmsbackup.shar

dist:
	rm -rf vmsbackup-dist
	mkdir vmsbackup-dist
	for i in $(DISTFILES); do  ln $${i} vmsbackup-dist;  done
	tar chf vmsbackup.tar vmsbackup-dist

