#srcdir=@srcdir@
#VPATH = @srcdir@

BUILT_SOURCES = parse.h
AM_YFLAGS = -d

noinst_LIBRARIES = librabbit.a

librabbit_a_SOURCES = st.c node.c eval.c memory.c object.c \
                      array.c class.c scope.c symbol.c numeric.c \
                      parse.y version.c string.c \
                      st.h rabbit.h node.h version.h lexer.c lexer.h

bin_PROGRAMS = rabbit

rabbit_SOURCES = main.c
rabbit_LDADD   = librabbit.a

CFLAGS = -O2 -Wall -g -ldl -rdynamic

