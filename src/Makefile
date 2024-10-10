PROGRAM_NAME = tetris
OBJ_PATH = ./obj/
SRCMODULES = tetris.c
OBJMODULES = $(addprefix $(OBJ_PATH), $(SRCMODULES:.c=.o))
CC = gcc 

ifeq ($(RELEASE), 1)
	CFLAGS = -Wall -static -std=gnu89 -pedantic -O2
else
	CFLAGS = -Wall -std=gnu89 -pedantic -g -O0
endif

$(PROGRAM_NAME): main.c $(OBJMODULES)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_PATH)%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_PATH)deps.mk: $(SRCMODULES)
	$(CC) -MM $^ > $@

.PHONY: clean
clean:
	rm -f $(OBJ_PATH)*.o $(PROGRAM_NAME) $(OBJ_PATH)deps.mk

ifneq (clean, $(MAKECMDGOALS))
-include $(OBJ_PATH)deps.mk
endif
