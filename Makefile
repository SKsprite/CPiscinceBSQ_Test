CC		= cc
SRC		= main.c
CFLAGS	= -Wall -Werror -Wextra
OBJ		= $(SRC:.c=.o)
NAME	= test_harness

ifdef BSQ_PATH
CFLAGS	+= -DBSQ='"$(BSQ_PATH)"'
endif

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(NAME)
	./$(NAME) all

show: $(NAME)
	./$(NAME) --show-output all

valid: $(NAME)
	./$(NAME) valid

invalid: $(NAME)
	./$(NAME) invalid

large: $(NAME)
	./$(NAME) large

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re test show valid invalid large
