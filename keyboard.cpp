#include "keyboard.h"
#include <stdio.h>
#include <string.h>

#pragma warning(disable:4996)

#ifdef _MSC_VER
#include <conio.h>
#else

#include <sys/time.h> /* struct timeval, select() */
/* ICANON, ECHO, TCSANOW, struct termios */
#include <termios.h> /* tcgetattr(), tcsetattr() */
#include <stdlib.h> /* atexit(), exit() */
#include <unistd.h> /* read() */
#include <stdio.h> /* printf() */

static struct termios g_old_kbd_mode;
/*****************************************************************************
*****************************************************************************/
static void cooked(void)
{
	tcsetattr(0, TCSANOW, &g_old_kbd_mode);
}
/*****************************************************************************
*****************************************************************************/
static void raw(void)
{
	static char init;
/**/
	struct termios new_kbd_mode;

	if(init)
		return;
/* put keyboard (stdin, actually) in raw, unbuffered mode */
	tcgetattr(0, &g_old_kbd_mode);
	memcpy(&new_kbd_mode, &g_old_kbd_mode, sizeof(struct termios));

	//new_kbd_mode.c_lflag &= ~(ICANON | ECHO);
	new_kbd_mode.c_lflag &= ~(ICANON | ECHO);
	new_kbd_mode.c_lflag|=ECHO;

	new_kbd_mode.c_cc[VTIME] = 0;
	new_kbd_mode.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &new_kbd_mode);
/* when we exit, go back to normal, "cooked" mode */
	atexit(cooked);

	init = 1;
}

/*****************************************************************************
*****************************************************************************/
static int kbhit(void)
{
	struct timeval timeout;
	fd_set read_handles;
	int status;

	raw();
/* check stdin (fd 0) for activity */
	FD_ZERO(&read_handles);
	FD_SET(0, &read_handles);
	timeout.tv_sec = timeout.tv_usec = 0;
	status = select(0 + 1, &read_handles, NULL, NULL, &timeout);
	if(status < 0)
	{
		printf("select() failed in kbhit()\n");
		exit(1);
	}
	return status;
}


/*****************************************************************************
*****************************************************************************/
static int getch(void)
{
	unsigned char temp;

	raw();
/* stdin = fd 0 */
	if(read(0, &temp, 1) != 1)
		return 0;
	return temp;
}

#endif


bool isKeyDown(void) // returns true if a key has been pressed.
{
	return kbhit() ? true : false;
}

int  getKey(void) 	  // returns a key
{
	int ret = getch();
	if ( ret == 0 )
	{
		ret = getch()+256;
	}
	return ret;
}

#define MAX_ARGS 32

const char ** getInputString(uint32_t &argc)
{
	argc = 0;
	static char buf[256];
	static int index = 0;
	static const char			*argv[MAX_ARGS];

	if ( !kbhit() ) return NULL;

	char c = (char)getch();

	if ( c == 10 || c == 13 )
	{
		buf[index] = 0;

		char *scan = buf;
		while ( *scan == 32 ) scan++;	// skip leading spaces

		if ( *scan )	// if not EOS
		{
			argc = 1;
			argv[0] = scan;	// set the first argument

			while ( *scan && argc < MAX_ARGS)	// while still data and we haven't exceeded the maximum argument count.
			{
				while ( *scan && *scan != 32 ) scan++;	// scan forward to the next space
				if ( *scan == 32 )	// if we hit a space
				{
					*scan = 0; // null-terminate the argument
					scan++;	// skip to the next character
					while ( *scan == 32 ) scan++;	// skip any leading spaces
					if ( *scan )	// if there is still a valid non-space character process that as the next argument
					{
						argv[argc] = scan;
						argc++;
					}
				}
			}
		}
		printf("\r\n");
		index = 0;
		return argc ? argv : NULL;
	}
	else if ( c == 27 )
	{
		buf[index] = 0;
	}
	else if ( c == 8 )
	{
		if ( index )
		{
			index--;
			printf("%c",c);
			printf(" ");
			printf("%c",c);
		}
	}
	else
	{
		if ( c >= 32 && c <= 127 )
		{
			buf[index] = c;
			index++;
#ifdef _MSC_VER
			printf("%c",c);
#else
#endif
		}
	}

	return NULL;
}
