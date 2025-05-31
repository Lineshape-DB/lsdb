include Make.conf

.SUFFIXES : .c

LSDBLIB = liblsdb.a

LIBSRCS = morph.c lsdb.c interp.c

PROGS  = morphu$(EXE_EXT) lsdbu$(EXE_EXT)

MCSRCS = morphu.c
LCSRCS = lsdbu.c

CHDRS  = include/lsdb/morph.h include/lsdb/morphP.h \
	 include/lsdb/lsdb.h include/lsdb/lsdbP.h

LIBOBJS = ${LIBSRCS:.c=.o}

MCOBJS = ${MCSRCS:.c=.o}
LCOBJS = ${LCSRCS:.c=.o}

SRCS   = $(LIBSRCS) $(MCSRCS) $(LCSRCS)
COBJS  = $(LIBOBJS) $(MCOBJS) $(LCOBJS)

CFLAGS = $(DEBUG) $(LINT) $(OPTIMIZE) $(TCOVERAGE) $(PROFILING) -I ./include
LDFLAGS = $(DEBUG) 

all: $(PROGS)

$(LSDBLIB) : $(LIBOBJS)
	ar cr $@ $^

morphu$(EXE_EXT): $(MCOBJS) $(LSDBLIB)
	$(CC) $(LDFLAGS) -o $@ $(MCOBJS) -L . -llsdb $(LIBS)

lsdbu$(EXE_EXT): $(LCOBJS) $(LSDBLIB)
	$(CC) $(LDFLAGS) -o $@ $(LCOBJS) -L . -llsdb $(LIBS)

include Make.dep

Make.dep: $(SRCS) schema.i
	@echo -n "Generating dependencies... "
	@echo "# Generated automatically by \`make depend'" > $@
	@$(CC) $(CFLAGS) -MM $(SRCS) >> $@
	@echo "done"

schema.i : schema.sql
	./sql2cstr.sh < $? > $@


install: $(PROGS) include/lsdb/lsdb.h include/lsdb/morph.h $(LSDBLIB)
	$(INSTALLDIR) $(BINDIR)
	$(INSTALL) $(PROGS) $(BINDIR)
	$(INSTALLDIR) $(INCDIR)/lsdb
	$(INSTALLDATA) include/lsdb/lsdb.h include/lsdb/morph.h $(INCDIR)/lsdb
	$(INSTALLDIR) $(LIBDIR)
	$(INSTALLDATA) $(LSDBLIB) $(LIBDIR)
	$(INSTALLDIR) $(PKGDIR)
	$(INSTALLDATA) lsdb.pc $(PKGDIR)
	$(INSTALLDIR) $(VAPIDIR)
	$(INSTALLDATA) lsdb.vapi $(VAPIDIR)

clean:
	$(RM) $(PROGS) $(COBJS) \
	Make.dep schema.i tags ChangeLog *.bak \
	*.bb *.bbg *.da *.gcda *.gcno *.gcov

depend: Make.dep

tags: $(SRCS) $(CHDRS)
	ctags -f $@ $(SRCS) $(CHDRS)

count:
	@sloccount . | \
        egrep --color 'Total ((Estimated Cost)|(Physical Source Lines)).*'
