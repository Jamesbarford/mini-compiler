TARGET := mcmc
CC     := gcc
OUTDIR := .
CFLAGS = -Wall -O2 -DDEBUG

$(OUTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: $(TARGET)

OBJS = $(OUTDIR)/compiler.o \
       $(OUTDIR)/cstr.o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) -lreadline -lm

clean:
	rm -rf ./*.o
	rm -rf $(TARGET)

format-code:
	clang-format *.c -i

format-headers:
	clang-format *.h -i

format: format-code format-headers

complier.o: ./compiler.c ./cstr.h
cstr.o: ./cstr.c ./cstr.h
