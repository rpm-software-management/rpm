CFLAGS=-g -Wall
OBJS=tetj.o

LIB=libtetj.a

all: $(LIB) 

install:

$(LIB): $(OBJS)
	rm -f $(LIB);ar clq $(LIB) $(OBJS)

clean:
	rm -f $(OBJS) $(LIB)
