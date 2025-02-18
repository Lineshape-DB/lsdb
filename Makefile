include Make.conf

.SUFFIXES : .c .vala
%.vala.o: %.vala
	$(VALAC) $(VFLAGS) -c $<

PROGS  = morphu$(EXE_EXT) lsdbu$(EXE_EXT) fit$(EXE_EXT)

MCSRCS = morph.c morphu.c
LCSRCS = lsdb.c interp.c lsdbu.c morph.c

CHDRS  = morph.h morphP.h lsdb.h

MCOBJS = ${MCSRCS:.c=.o}
LCOBJS = ${LCSRCS:.c=.o}

SRCS   = $(MCSRCS) $(LCSRCS)
COBJS  = $(MCOBJS) $(LCOBJS)

CFLAGS = $(DEBUG) $(LINT) $(OPTIMIZE) $(TCOVERAGE) $(PROFILING) -I .
LDFLAGS = $(DEBUG) 

all: $(PROGS)

morphu$(EXE_EXT): $(MCOBJS)
	$(CC) $(LDFLAGS) -o $@ $(MCOBJS) $(LIBS)

lsdbu$(EXE_EXT): $(LCOBJS)
	$(CC) $(LDFLAGS) -o $@ $(LCOBJS) $(LIBS)

fit$(EXE_EXT): lsdb.o grace.vala.o fit.vala.o
	$(CC) $(LDFLAGS) -o $@ lsdb.o morph.o interp.o fit.vala.o grace.vala.o $(GTKLIBS) $(LIBS)

include Make.dep

Make.dep: $(SRCS)
	@echo -n "Generating dependencies... "
	@echo "# Generated automatically by \`make depend'" > $@
	@$(CC) $(CFLAGS) -MM $(SRCS) >> $@
	@echo "done"

schema.i : schema.sql
	./sql2cstr.sh < $? > $@

grace.vapi : grace.vala
	$(VALAC) --pkg gee-0.8 --pkg gtk4 -X -Wno-incompatible-pointer-types \
	   --library grace -H grace.h -c $<

grace.vala.o : grace.vala
	$(VALAC) --pkg gee-0.8 --pkg gtk4 -X -Wno-incompatible-pointer-types \
	   -c $<

fit.vala.o : lsdb.vapi grace.vapi

clean:
	$(RM) $(PROGS) $(COBJS) \
	Make.dep tags ChangeLog *.bak \
	*.bb *.bbg *.da *.gcda *.gcno *.gcov

depend: Make.dep

tags: $(SRCS) $(CHDRS)
	ctags -f $@ $(SRCS) $(CHDRS)

count:
	@sloccount . | \
        egrep --color 'Total ((Estimated Cost)|(Physical Source Lines)).*'
