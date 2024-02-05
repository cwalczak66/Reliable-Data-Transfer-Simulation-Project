CC=gcc
FLAGS=-g
NAME=p2

project:project2.c student2.c
	$(CC) $(FLAGS) project2.c student2.c -o $(NAME)

clean:
	rm $(NAME)