CC=gcc
CFLAGS= -Wall -g -lpthread -pthread

OBJ = radio_controller.o
OBJ2 = radio_server.o
GRN=\033[0;32m
NC=\033[0m # No Color
all: radio_controller radio_server clean

%.o: %.c $(DEPS)
	@$(CC) -c -o $@ $< $(CFLAGS)

radio_server: $(OBJ)
	@echo -e "${GRN}Compiling the server side${NC}"
	@$(CC) -o $@ $^ $(CFLAGS)
	@echo -e "${GRN}Great Success! ${NC}"
	
radio_controller: $(OBJ2)
	@echo -e "${GRN}Compiling the server side${NC}"
	@$(CC) -o $@ $^ $(CFLAGS)
	@echo -e "${GRN}Great Success! ${NC}"
clean:
	@rm -f *.o core *.core 
