include Make.conf

PROGS  = morphu$(EXE_EXT) lsdbu$(EXE_EXT)

MCSRCS = morph.c morphu.c
LCSRCS = lsdb.c lsdbu.c morph.c

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

include Make.dep

Make.dep: $(SRCS)
	@echo -n "Generating dependencies... "
	@echo "# Generated automatically by \`make depend'" > $@
	@$(CC) $(CFLAGS) -MM $(SRCS) >> $@
	@echo "done"

schema.i : schema.sql
	./sql2cstr.sh < $? > $@

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
