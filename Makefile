_END		=	\e[0m
_BOLD		=	\e[1m
_UNDER		=	\e[4m
_REV		=	\e[7m

_WHITE		=	\e[1;30m
_RED		=	\e[1;31m
_GREEN		=	\e[1;32m
_YELLOW		=	\e[1;33m
_BLUE		=	\e[1;34m
_PURPLE		=	\e[1;35m
_CYAN		=	\e[1;36m
_WHITE		=	\e[1;37m

_GONE		=	\e[2K\r

CXX					=	c++
CXX_WARNINGS 		= -Wall -Wextra -Werror
CXX_RESTRICTION 	= -std=c++98 -pedantic
CXX_DEPENDENCIES 	= -MMD -MP -MF $(DEPS_DIR)/$*.d
CXXFLAGS 			= $(CXX_WARNINGS) $(CXX_RESTRICTION) $(CXX_DEPENDENCIES)
DEBUG				=	-g -fsanitize=address

NAME		=	ircserv

SRC_DIR		=	./src/
OBJ_DIR		=	./obj/
DEPS_DIR	=	./deps/

INCLUDE		=	-I ./inc/

SRCS		=	main.cpp \
				server.cpp \
				Client.cpp \
				Channel.cpp

SRC			=	$(addprefix $(SRC_DIR), $(SRCS))
OBJS		=	$(SRCS:.cpp=.o)
OBJ			=	$(addprefix $(OBJ_DIR), $(OBJS))
DEPS		=	$(SRCS:.cpp=.d)
DEP			=	$(addprefix $(DEPS_DIR), $(DEPS))

# all:	$(NAME)
all:	debug

$(NAME): $(OBJ)
	printf "$(_GONE) $(_GREEN) All files compiled into $(OBJ_DIR) $(_END)\n"
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ) $(INCLUDE)
	
	printf "$(_GONE) $(_GREEN) Executable $(NAME) created $(_END)\n"

$(OBJ): | $(OBJ_DIR) $(DEPS_DIR)

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	printf "$(_GONE) $(_YELLOW) Compiling $< $(_END)"
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDE)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(DEPS_DIR):
	mkdir -p $(DEPS_DIR)

clean:
	rm -rf $(OBJ_DIR) $(DEPS_DIR)
	printf "$(_GONE) $(_RED) $(OBJ_DIR) and $(DEPS_DIR) have been deleted $(_END)\n"

fclean: clean
	rm -rf $(NAME)
	printf "$(_GONE) $(_RED) $(NAME) has been deleted $(_END)\n"

re: fclean all

debug: CXXFLAGS += $(DEBUG)
debug: $(NAME)

.SILENT:

-include $(DEP)