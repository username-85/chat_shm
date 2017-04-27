#include "shm_buffer.h"

#include <errno.h>
#include <ncurses.h>
#include <unistd.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>

// chat history window
static WINDOW *chat_win;
// window for message input. And for some info messages output
static WINDOW *msg_win;
// initscr successfully called
bool ncurses_on;

static void fail_exit(char const *msg);
static void term_init(void);
static void term_deinit(void);
static int send_msg(char *txt);

int main(void)
{
	if (open_shm_buffer() != 0)
		fail_exit("open_shm_buffer error");

	term_init();

	wprintw(chat_win, "/*************************************/\n");
	wprintw(chat_win, "/*          <F10> - quit             */\n");
	wprintw(chat_win, "/*     other keys - start message    */\n");
	wprintw(chat_win, "/*************************************/\n");

	send_msg("online");

	while (1) {
		bool redisplay = 0;

		while(1) {
			char *msg = get_str_from_shm();
			if (!msg)
				break;
			wprintw(chat_win, "%s\n", msg);
			redisplay = 1;
		}
		if (redisplay)
			wrefresh(chat_win);

		noecho();
		int ch = wgetch(chat_win);

		if (ch == KEY_F(10)) {
			break;
		} else if (ch != ERR) {
			curs_set(1);
			echo();
			wprintw(msg_win, ">> ");
			wrefresh(msg_win);

			char buf[BUF_STRLEN] = {0};
			wgetnstr(msg_win, buf, BUF_STRLEN - 1);

			werase(msg_win);
			wrefresh(msg_win);
			curs_set(0);

			if (strnlen(buf, BUF_STRLEN))
				send_msg(buf);
		}
	}

	send_msg("offline");

	close_shm_buffer();
	term_deinit();

	exit(EXIT_SUCCESS);
}

static void term_init(void)
{
	setlocale(LC_ALL, "");

	if (initscr() == NULL)
		fail_exit("Failed to initialize ncurses");

	ncurses_on = true;

	cbreak();
	noecho();
	curs_set(0);

	if (LINES >= 2) {
		chat_win = newwin(LINES - 1, COLS, 0, 0);
		msg_win = newwin(LINES -1, COLS, LINES - 1, 0);
	} else {
		fail_exit("Window is too small");
	}

	if (chat_win == NULL || msg_win == NULL)
		fail_exit("Failed ctreate windows");

	keypad(chat_win, TRUE);
	wtimeout(chat_win, 100);
	scrollok(chat_win,TRUE);
}

static void fail_exit(char const *msg)
{
	//? !isendwin()
	if (ncurses_on)
		endwin();
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

static void term_deinit(void)
{
	delwin(msg_win);
	delwin(chat_win);
	if (ncurses_on) {
		endwin();
		ncurses_on = false;
	}
}

static int send_msg(char *txt)
{
	char msg[BUF_STRLEN] = {0};
	snprintf(msg, BUF_STRLEN, "%d : %s", getpid(), txt);
	return add_str_to_shm(msg);
}

