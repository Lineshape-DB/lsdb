include Make.conf

PROGS =	morph$(EXE_EXT)

CSRCS = morph.c
#CHDRS = morph.h

COBJS  = ${CSRCS:.c=.o}

SRCS  = $(CSRCS)

CFLAGS = $(DEBUG) $(LINT) $(OPTIMIZE) $(TCOVERAGE) $(PROFILING)
LDFLAGS = $(DEBUG) 

all: $(PROGS)

morph$(EXE_EXT): $(COBJS)
	$(CC) $(LDFLAGS) -o $@ $(COBJS) $(LIBS)

include Make.dep

Make.dep: $(SRCS)
	@echo -n "Generating dependencies... "
	@echo "# Generated automatically by \`make depend'" > $@
	@$(CC) $(CFLAGS) -MM $(SRCS) >> $@
	@echo "done"

clean:
	$(RM) $(PROG) $(COBJS) \
	Make.dep tags ChangeLog *.bak \
	*.bb *.bbg *.da *.gcda *.gcno *.gcov

depend: Make.dep

tags: $(SRCS) $(CHDRS)
	ctags -f $@ $(SRCS) $(CHDRS)

count:
	@sloccount . | \
        egrep --color 'Total ((Estimated Cost)|(Physical Source Lines)).*'
