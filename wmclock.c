/* wmclock.c: a dockable clock applet for Window Maker
 * created 1999-Apr-09 jmk
 * 
 * by Jim Knoble <jmknoble@pobox.com>
 * Copyright (C) 1999 Jim Knoble
 * 
 * Significant portions of this software are derived from asclock by
 * Beat Christen <spiff@longstreet.ch>.  Such portions are copyright
 * by Beat Christen and the other authors of asclock.
 * 
 * Disclaimer:
 * 
 * The software is provided "as is", without warranty of any kind,
 * express or implied, including but not limited to the warranties of
 * merchantability, fitness for a particular purpose and
 * noninfringement. In no event shall the author(s) be liable for any
 * claim, damages or other liability, whether in an action of
 * contract, tort or otherwise, arising from, out of or in connection
 * with the software or the use or other dealings in the software.
 */

#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "dynlist.h"

/**********************************************************************/
#define ONLY_SHAPED_WINDOW	1

#define NUM_TIME_POSITIONS	5
#define NUM_X_POSITIONS		11
#define NUM_Y_POSITIONS		4

#define DIGIT_1_X_POS		0
#define DIGIT_2_X_POS		1
#define DIGIT_3_X_POS		3
#define DIGIT_4_X_POS		4
#define DIGIT_Y_POS		0
#define LED_NUM_Y_OFFSET	0
#define LED_THIN_1_X_OFFSET	13
#define LED_NUM_WIDTH		9
#define LED_NUM_HEIGHT		11
#define LED_THIN_1_WIDTH	5

#define COLON_X_POS		2
#define COLON_Y_POS		DIGIT_Y_POS
#define COLON_X_OFFSET		90
#define COLON_Y_OFFSET		0
#define BLANK_X_OFFSET		119
#define BLANK_Y_OFFSET		COLON_Y_OFFSET
#define COLON_WIDTH		3
#define COLON_HEIGHT		11

#define AMPM_X_POS		5
#define AM_X_OFFSET		94
#define AM_Y_OFFSET		5
#define PM_X_OFFSET		107
#define PM_Y_OFFSET		5
#define AM_WIDTH		12
#define AM_HEIGHT		6
#define PM_WIDTH		11
#define PM_HEIGHT		6

#define MONTH_X_POS		10
#define MONTH_Y_POS		3
#define MONTH_X_OFFSET		0
#define MONTH_WIDTH		22
#define MONTH_HEIGHT		6

#define DATE_LEFT_X_POS		7
#define DATE_CENTER_X_POS	8
#define DATE_RIGHT_X_POS	9
#define DATE_Y_POS		2
#define DATE_Y_OFFSET		0
#define DATE_NUM_WIDTH		9
#define DATE_NUM_HEIGHT		13

#define WEEKDAY_X_POS		6
#define WEEKDAY_Y_POS		1
#define WEEKDAY_X_OFFSET	0
#define WEEKDAY_WIDTH		20
#define WEEKDAY_HEIGHT		6

#define LED_XPM_BRIGHT_LINE_INDEX	3
#define LED_XPM_BRIGHT_CHAR		'+'
#define LED_XPM_DIM_LINE_INDEX		4
#define LED_XPM_DIM_CHAR		'@'

#define DEFAULT_XPM_CLOSENESS	40000

#define DIM_NUMERATOR	5
#define DIM_DENOMINATOR	10
#define makeDimColor(c)	(((c) * DIM_NUMERATOR) / DIM_DENOMINATOR)

/**********************************************************************/
#ifndef ONLY_SHAPED_WINDOW
# include "clk.xpm"
#endif /* !ONLY_SHAPED_WINDOW */
#include "month.xpm"
#include "weekday.xpm"
#include "xpm/date.xpm"
#include "xpm/led.xpm"
#include "xpm/mask.xpm"

void showUsage(void);
void showVersion(void);
int buildCommand(char *, char **, int *, int *);
void executeCommand(char *);
void showError(const char *, const char*);
void showFatalError(const char *, const char*);
void GetXpms(void);
int flushExposeEvents(GtkStatusIcon *);
void redrawWindow(GdkPixbuf *);
int mytime(void);
void showYear(void);
void showTime12(void);
void showTime24(void);
void showTime(void);
char* extractProgName(char *);
int processArgs(int, char **);

/**********************************************************************/
int enable12HourClock = 0;	/* default value is 24h format */
int enableShapedWindow = 1;	/* default value is noshape */
int enableBlinking = 1;		/* default is blinking */
int startIconified = 0;		/* default is not iconified */
int enableYearDisplay = 0;	/* default is to show time, not year */

int timePos12[NUM_TIME_POSITIONS]  = { 5, 14, 24, 28, 37 };
int timePos24[NUM_TIME_POSITIONS]  = { 4,  8, 17, 22, 31 };
/* with shape */
int xPosShaped[NUM_X_POSITIONS] = { 0, 0, 0, 0, 0, 40, 17, 17, 22, 27, 15 };
int yPosShaped[NUM_Y_POSITIONS] = { 3, 21, 30, 45 };

#ifndef ONLY_SHAPED_WINDOW
/* no shape */
int xPosUnshaped[NUM_X_POSITIONS] = { 5, 5, 5, 5, 5, 45, 21, 21, 26, 31, 19 };
int yPosUnshaped[NUM_Y_POSITIONS] = { 7, 25, 34, 49 };
#endif /* !ONLY_SHAPED_WINDOW */

int xPos[NUM_X_POSITIONS];
int yPos[NUM_Y_POSITIONS];

GtkStatusIcon	*win;

char *progName;
char *geometry = "";
char *ledColor = "LightSeaGreen";

char *commandToExec = NULL;
char *commandBuf = NULL;
int   commandLength = 0;
int   commandIndex = 0;

char *errColorCells = "not enough free color cells or xpm not found\n";

char *userClockXpm;
char *userMonthXpm;
char *userWeekdayXpm;
int   useUserClockXpm = 0;
int   useUserMonthXpm = 0;
int   useUserWeekdayXpm = 0;

GdkPixbuf *clockBg, *led, *months, *dateNums, *weekdays;
GdkPixbuf *visible;

time_t actualTime;
long   actualMinutes;

static struct tm *localTime;

char *usageText[] = {
"Options:",
"    -12                     show 12-hour time (am/pm)",
"    -24                     show 24-hour time",
"    -year                   show year instead of time",
"    -noblink                don't blink",
"    -exe <command>          start <command> on mouse click",
"    -led <color>            use <color> as color of led",
#ifndef ONLY_SHAPED_WINDOW
"    -clockxpm <filename>    get clock background from pixmap in <filename>",
#endif /* !ONLY_SHAPED_WINDOW */
"    -monthxpm <filename>    get month names from pixmap in <filename>",
"    -weekdayxpm <filename>  get weekday names from pixmap in <filename>",
"    -version                display the version",
NULL
};

char *version = VERSION;

/**********************************************************************/
/* Display usage information */
void showUsage(void)
{
   char **cpp;
   
   fprintf(stderr, "Usage:  %s [option [option ...]]\n\n", progName);
   for (cpp = usageText; *cpp; cpp++)
    {
       fprintf(stderr, "%s\n", *cpp);
    }
   fprintf(stderr,"\n");
   exit(1);
}

/* Display the program version */
void showVersion()
{
   fprintf(stderr, "%s version %s\n", progName, version);
   exit(1);
}

/* Build the shell command to execute */
int buildCommand(char *command, char **buf, int *buf_len, int *i)
{
   int status;
   
   status = append_string_to_buf(buf, buf_len, i, command);
   if (APPEND_FAILURE == status)
    {
       return (0);
    }
   status = append_string_to_buf(buf, buf_len, i, " &");
   return ((APPEND_FAILURE == status) ? 0 : 1);
}

/* Execute the given shell command */
void executeCommand(char *command)
{
   int status;

   if (NULL == command)
    {
       return;
    }
   status = system(command);

   if (-1 == status)
    {
       perror("system");
    }
}

/* Display an error message */
void showError(const char *message, const char *data)
{
   fprintf(stderr,"%s: can't %s %s\n", progName, message, data);
}

/* Display an error message and exit */
void showFatalError(const char *message, const char *data)
{
   showError(message, data);
   exit(1);
}

/* Konvertiere XPMIcons nach Pixmaps */
void GetXpms(void)
{
   static char     **clock_xpm;
   GdkColor          color;
   char              ledBright[64];
   char              ledDim[64];
   int               status;
   
#ifdef ONLY_SHAPED_WINDOW
   clock_xpm = mask_xpm;
#else /* !ONLY_SHAPED_WINDOW */
   clock_xpm = enableShapedWindow ? mask_xpm : clk_xpm;
#endif /* ONLY_SHAPED_WINDOW */
   
   /* get user-defined color */
   if(!gdk_color_parse(ledColor, &color))
    {
       showError("parse color", ledColor);
    }
   
   sprintf(ledBright, "%c      c #%04X%04X%04X", LED_XPM_BRIGHT_CHAR,
	   color.red, color.green, color.blue);
   led_xpm[LED_XPM_BRIGHT_LINE_INDEX] = &ledBright[0];
   
   color.red   = makeDimColor(color.red);
   color.green = makeDimColor(color.green);
   color.blue  = makeDimColor(color.blue);
   sprintf(&ledDim[0], "%c      c #%04X%04X%04X", LED_XPM_DIM_CHAR,
	   color.red, color.green, color.blue);
   led_xpm[LED_XPM_DIM_LINE_INDEX] = &ledDim[0];

   if (useUserClockXpm)
    {
		clockBg = gdk_pixbuf_new_from_xpm_data((const char **)userClockXpm);
    }
   else
    {
		clockBg = gdk_pixbuf_new_from_xpm_data((const char **)clock_xpm);
    }
   if (clockBg == NULL)
    {
       showFatalError("create clock pixmap:", errColorCells);
    }

#ifdef ONLY_SHAPED_WINDOW
   visible = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 1, 8, gdk_pixbuf_get_width(clockBg), gdk_pixbuf_get_height(clockBg));
#else /* !ONLY_SHAPED_WINDOW */
   visible = gdk_pixbuf_new_from_xpm_data(clk_xpm);
#endif /* ONLY_SHAPED_WINDOW */

   led = gdk_pixbuf_new_from_xpm_data((const char **)led_xpm);
   if (led == NULL)
    {
       showFatalError("create led pixmap:", errColorCells);
    }

   if (useUserMonthXpm)
    {
	   months = gdk_pixbuf_new_from_xpm_data((const char **)userMonthXpm);
    }
   else 
    {
	   months = gdk_pixbuf_new_from_xpm_data((const char **)month_xpm);
    }
   if (months == NULL)
    {
       showFatalError("create month pixmap:", errColorCells);
    }

   dateNums = gdk_pixbuf_new_from_xpm_data((const char **)date_xpm);
   if (dateNums == NULL)
    {
       showFatalError("create date pixmap:", errColorCells);
    }

   if (useUserWeekdayXpm) 
    {
	   weekdays = gdk_pixbuf_new_from_xpm_data((const char **)userWeekdayXpm);
    }
   else
    {
	   weekdays = gdk_pixbuf_new_from_xpm_data((const char **)weekday_xpm);
    }
   if (weekdays == NULL)
    {
       showFatalError("create weekday pixmap:", errColorCells);
    }
}

/* Remove expose events for a specific window from the queue */
int flushExposeEvents(GtkStatusIcon *w)
{
   int    i = 0;
#if 0
   XEvent dummy;
   
   while (XCheckTypedWindowEvent(dpy, w, Expose, &dummy))
    {
       i++;
    }
#endif
   return(i);
}

/* (Re-)Draw the main window and the icon window */
void redrawWindow(GdkPixbuf *v)
{
   flushExposeEvents(win);
   gtk_status_icon_set_from_pixbuf(win, v);
}

/* Fetch the system time and time zone */
int mytime(void)
{
   struct timeval  tv;
   struct timezone tz;
   
   gettimeofday(&tv, &tz);
   
   return(tv.tv_sec);
}

/* Display the current year in the LED display */
void showYear(void)
{
   int year;
   int digitXOffset;
   int digitYOffset;
   
   year = localTime->tm_year + 1900;
   
   digitYOffset = LED_NUM_Y_OFFSET;
   digitXOffset = LED_NUM_WIDTH * (year / 1000);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_1_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * ((year % 100) % 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_2_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * ((year / 10) % 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_3_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * (year % 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_4_X_POS], yPos[DIGIT_Y_POS]);
}

/* Display time in twelve-hour mode, with am/pm indicator */
void showTime12(void)
{
   int digitXOffset;
   int digitYOffset;
   int localHour = localTime->tm_hour % 12;
   
   if (0 == localHour)
    {
       localHour = 12;
    }
   if (localTime->tm_hour < 12)
    {
       /* AM */
	   gdk_pixbuf_copy_area(led, AM_X_OFFSET, AM_Y_OFFSET, AM_WIDTH, AM_HEIGHT, visible, xPos[AMPM_X_POS], yPos[DIGIT_Y_POS] + AM_Y_OFFSET);
    }
   else
    {
       /* PM */
	   gdk_pixbuf_copy_area(led, PM_X_OFFSET, PM_Y_OFFSET, PM_WIDTH, PM_HEIGHT, visible, xPos[AMPM_X_POS], yPos[DIGIT_Y_POS] + PM_Y_OFFSET);
    }
   
   digitYOffset = LED_NUM_Y_OFFSET;
   if (localHour > 9)
    {
       digitXOffset = LED_THIN_1_X_OFFSET;
	   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_THIN_1_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_1_X_POS], yPos[DIGIT_Y_POS]);
    }
   digitXOffset = LED_NUM_WIDTH * (localHour % 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_2_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * (localTime->tm_min / 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_3_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * (localTime->tm_min % 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_4_X_POS], yPos[DIGIT_Y_POS]);
}

/* Display time in 24-hour mode, without am/pm indicator */
void showTime24(void)
{
   int digitXOffset;
   int digitYOffset;
   
   digitYOffset = LED_NUM_Y_OFFSET;
   digitXOffset = LED_NUM_WIDTH * (localTime->tm_hour / 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_1_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * (localTime->tm_hour % 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_2_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * (localTime->tm_min / 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_3_X_POS], yPos[DIGIT_Y_POS]);
   digitXOffset = LED_NUM_WIDTH * (localTime->tm_min % 10);
   gdk_pixbuf_copy_area(led, digitXOffset, digitYOffset, LED_NUM_WIDTH, LED_NUM_HEIGHT, visible, xPos[DIGIT_4_X_POS], yPos[DIGIT_Y_POS]);
}

void showTime(void)
{
   int xOffset;
   int yOffset;
   
   /* Zeit auslesen */
   actualTime = mytime();
   actualMinutes = actualTime / 60;
   
   localTime = localtime(&actualTime);
   
   /* leere clock holen */
   gdk_pixbuf_copy_area(clockBg, 0, 0,  gdk_pixbuf_get_width(clockBg), gdk_pixbuf_get_height(clockBg), visible, 0, 0);
   
   if (enableYearDisplay)
    {
       showYear();
    }
   else if (enable12HourClock)
    {
       showTime12();
    }
   else
    {
       showTime24();
    }
   
   /* Monat */
   xOffset = MONTH_X_OFFSET;
   yOffset = MONTH_HEIGHT * (localTime->tm_mon);
   gdk_pixbuf_copy_area(months, xOffset, yOffset, MONTH_WIDTH, MONTH_HEIGHT, visible, xPos[MONTH_X_POS], yPos[MONTH_Y_POS]);
   
   /* Datum */
   yOffset = DATE_Y_OFFSET;
   if (localTime->tm_mday > 9)
    {
       xOffset = DATE_NUM_WIDTH * (((localTime->tm_mday / 10) + 9) % 10);
	   gdk_pixbuf_copy_area(dateNums, xOffset, yOffset, DATE_NUM_WIDTH, DATE_NUM_HEIGHT, visible, xPos[DATE_LEFT_X_POS], yPos[DATE_Y_POS]);
       xOffset = DATE_NUM_WIDTH * (((localTime->tm_mday % 10) + 9) % 10);
	   gdk_pixbuf_copy_area(dateNums, xOffset, yOffset, DATE_NUM_WIDTH, DATE_NUM_HEIGHT, visible, xPos[DATE_RIGHT_X_POS], yPos[DATE_Y_POS]);
    }
   else
    {
       xOffset = DATE_NUM_WIDTH * (localTime->tm_mday - 1);
	   gdk_pixbuf_copy_area(dateNums, xOffset, yOffset, DATE_NUM_WIDTH, DATE_NUM_HEIGHT, visible, xPos[DATE_CENTER_X_POS], yPos[DATE_Y_POS]);
    }
   
   /* Wochentag */
   xOffset = WEEKDAY_X_OFFSET;
   yOffset = WEEKDAY_HEIGHT * ((localTime->tm_wday + 6) % 7);
   gdk_pixbuf_copy_area(weekdays, xOffset, yOffset, WEEKDAY_WIDTH, WEEKDAY_HEIGHT, visible, xPos[WEEKDAY_X_POS], yPos[WEEKDAY_Y_POS]);
   
   if ((!enableBlinking) && (!enableYearDisplay))
    {
       /* Sekunden Doppelpunkt ein */
       xOffset = COLON_X_OFFSET;
       yOffset = COLON_Y_OFFSET;
	   gdk_pixbuf_copy_area(led, xOffset, yOffset, COLON_WIDTH, COLON_HEIGHT, visible, xPos[COLON_X_POS], yPos[COLON_Y_POS]);
    }
}

/* Extract program name from the zeroth program argument */
char *extractProgName(char *argv0)
{
   char *prog_name = NULL;
   
   if (NULL != argv0)
    {
       prog_name = strrchr(argv0, '/');
       if (NULL == prog_name)
	{
	   prog_name = argv0;
	}
       else
	{
	   prog_name++;
	}
    }
   return (prog_name);
}

/* Process program arguments and set corresponding options */
int processArgs(int argc, char **argv)
{
   int i;
   
   for (i = 1; i < argc; i++)
    {
       if (0 == strcmp(argv[i], "--"))
	{
	   break;
	}
       else if ((0 == strcmp(argv[i], "-12")) ||
		(0 == strcmp(argv[i], "-1")) ||
		(0 == strcmp(argv[i], "--12")))
	{
	   enable12HourClock = 1;
	}
       else if ((0 == strcmp(argv[i], "-24")) ||
		(0 == strcmp(argv[i], "-2")) ||
		(0 == strcmp(argv[i], "--24")))
	{
	   enable12HourClock = 0;
	}
       else if ((0 == strcmp(argv[i], "-exe")) ||
		(0 == strcmp(argv[i], "-e")) ||
		(0 == strcmp(argv[i], "--exe")))
	{
	   if (++i >= argc)
	    {
	       showUsage();
	    }
	   commandToExec = argv[i];
	}
       else if ((0 == strcmp(argv[i], "-led")) ||
		(0 == strcmp(argv[i], "-l")) ||
		(0 == strcmp(argv[i], "--led")))
	{
	   if (++i >= argc)
	    {
	       showUsage();
	    }
	   ledColor = argv[i];
	}
       else if ((0 == strcmp(argv[i], "-clockxpm")) ||
		(0 == strcmp(argv[i], "-c")) ||
		(0 == strcmp(argv[i], "--clockxpm")))
	{
#ifndef ONLY_SHAPED_WINDOW
	   if (++i >= argc)
	    {
	       showUsage();
	    }
	   userClockXpm = argv[i];
	   useUserClockXpm = 1;
#endif /* !ONLY_SHAPED_WINDOW */
	}
       else if ((0 == strcmp(argv[i], "-monthxpm")) ||
		(0 == strcmp(argv[i], "-m")) ||
		(0 == strcmp(argv[i], "--monthxpm")))
	{
	   if (++i >= argc)
	    {
	       showUsage();
	    }
	   userMonthXpm = argv[i];
	   useUserMonthXpm = 1;
	}
       else if ((0 == strcmp(argv[i], "-weekdayxpm")) ||
		(0 == strcmp(argv[i], "-w")) ||
		(0 == strcmp(argv[i], "--weekdayxpm")))
	{
	   if (++i >= argc)
	    {
	       showUsage();
	    }
	   userWeekdayXpm = argv[i];
	   useUserWeekdayXpm = 1;
	}
       else if ((0 == strcmp(argv[i], "-noblink")) ||
		(0 == strcmp(argv[i], "-n")) ||
		(0 == strcmp(argv[i], "--noblink")))
	{
	   enableBlinking = 0;
	}
       else if ((0 == strcmp(argv[i], "-year")) ||
		(0 == strcmp(argv[i], "-y")) ||
		(0 == strcmp(argv[i], "--year")))
	{
	   enableYearDisplay = 1;
	}
       else if ((0 == strcmp(argv[i], "-position")) ||
		(0 == strcmp(argv[i], "-p")) ||
		(0 == strcmp(argv[i], "--position")))
	{
#ifndef ONLY_SHAPED_WINDOW
	   if (++i >= argc)
	    {
	       showUsage();
	    }
	   geometry = argv[i];
#endif /* !ONLY_SHAPED_WINDOW */
	}
       else if ((0 == strcmp(argv[i], "-shape")) ||
		(0 == strcmp(argv[i], "-s")) ||
		(0 == strcmp(argv[i], "--shape")))
	{
	   enableShapedWindow = 1;
	}
       else if ((0 == strcmp(argv[i], "-iconic")) ||
		(0 == strcmp(argv[i], "-i")) ||
		(0 == strcmp(argv[i], "--iconic")))
	{
#ifndef ONLY_SHAPED_WINDOW
	   startIconified = 1;
#endif /* !ONLY_SHAPED_WINDOW */
	}
       else if ((0 == strcmp(argv[i], "-version")) ||
		(0 == strcmp(argv[i], "-V")) ||
		(0 == strcmp(argv[i], "--version")))
	{
	   showVersion();
	}
       else if ((0 == strcmp(argv[i], "-help")) ||
		(0 == strcmp(argv[i], "-h")) ||
		(0 == strcmp(argv[i], "--help")))
	{
	   showUsage();
	}
       else
	{
	   fprintf(stderr, "%s: unrecognized option `%s'\n",
		   progName, argv[i]);
	   showUsage();
	}
    }
   return (i);
}

gboolean timer(gpointer user_data)
{
	struct timeval nextEvent;

	if (actualTime != mytime())
	{
		actualTime = mytime();
		if (actualMinutes != (actualTime / 60))
		{
			showTime();
			if (!enableBlinking)
			{
				redrawWindow(visible);
			}
		}
		if (enableBlinking && (!enableYearDisplay))
		{  
			if (actualTime % 2)
			{
				/* Sekunden Doppelpunkt ein */
				gdk_pixbuf_copy_area(led, COLON_X_OFFSET, COLON_Y_OFFSET, COLON_WIDTH, COLON_HEIGHT, visible, xPos[COLON_X_POS], yPos[COLON_Y_POS]);
			}
			else
			{
				/* Sekunden Doppelpunkt aus */
				gdk_pixbuf_copy_area(led, BLANK_X_OFFSET, BLANK_Y_OFFSET, COLON_WIDTH, COLON_HEIGHT, visible, xPos[COLON_X_POS], yPos[COLON_Y_POS]);
			}
			redrawWindow(visible);
		}
		if (0 == (actualTime % 2))
		{
			/* Clean up zombie processes */
			if (NULL != commandToExec) 
			{
				waitpid(0, NULL, WNOHANG);
			}
		}
	}
	
	/* We compute the date of next event, in order to avoid polling */
	if (enableBlinking)
	{
		gettimeofday(&nextEvent,NULL);
		nextEvent.tv_sec = 0;
		nextEvent.tv_usec = 1000000-nextEvent.tv_usec;
	}
	else
	{
		if (enableYearDisplay)
		{
			nextEvent.tv_sec = 86400-actualTime%86400;
			nextEvent.tv_usec = 0;
		}
		else
		{
			nextEvent.tv_sec = 60-actualTime%60;
			nextEvent.tv_usec = 0;
		}
	}

	g_timeout_add(nextEvent.tv_sec * 1000 + nextEvent.tv_usec/1000, timer, NULL);

	return FALSE;
}

void click(GtkStatusIcon *status_icon, 
                        gpointer user_data)
{
	if (NULL != commandToExec)
	{
		pid_t fork_pid;

		if ((NULL == commandBuf) &&
				(!buildCommand(commandToExec, &commandBuf,
							   &commandLength, &commandIndex)))
		{
			return;
		}

		fork_pid = fork();
		switch (fork_pid)
		{
			case 0:
				/* We're the child process;
				 * run the command and exit.
				 */
				executeCommand(commandBuf);
				/* When the system() call finishes, we're done. */
				exit(0);
				break;
			case -1:
				/* We're the parent process, but
				 * fork() gave an error.
				 */
				perror("fork");
				break;
			default:
				/* We're the parent process;
				 * keep on doing what we normally do.
				 */
				break;
		}
	}
}

/**********************************************************************/
int main(int argc, char **argv)
{
	int           i;

	/* Parse command line options */
	progName = extractProgName(argv[0]);
	processArgs(argc, argv);

	/* init led position */
#ifndef ONLY_SHAPED_WINDOW
	for (i = 0; i < NUM_Y_POSITIONS; i++)
	{
		yPos[i] = enableShapedWindow ? yPosShaped[i] : yPosUnshaped[i];
	}
	for (i = 0; i < NUM_X_POSITIONS; i++)
	{
		xPos[i] = enableShapedWindow ? xPosShaped[i] : xPosUnshaped[i]; 
	}
#else /* ONLY_SHAPED_WINDOW */
	for (i = 0; i < NUM_Y_POSITIONS; i++)
	{
		yPos[i] = yPosShaped[i];
	}
	for (i = 0; i < NUM_X_POSITIONS; i++)
	{
		xPos[i] = xPosShaped[i];
	}
#endif /* !ONLY_SHAPED_WINDOW */
	for (i = 0; i < NUM_TIME_POSITIONS; i++)
	{
		if (enable12HourClock && (!enableYearDisplay))
		{
			xPos[i] += timePos24[i];
		}
		else
		{
			xPos[i] += timePos12[i];
		}
	}

	gtk_init(&argc, &argv);

	/* Icon Daten nach XImage konvertieren */
	GetXpms();

	/* Create a window to hold the banner */
	win = gtk_status_icon_new_from_pixbuf(clockBg);

	g_timeout_add(0, timer, NULL);
	g_signal_connect(G_OBJECT(win), "activate", 
					 G_CALLBACK(click), NULL);

	showTime();
	redrawWindow(visible);
	gtk_main();

#ifdef ONLY_SHAPED_WINDOW
	g_object_unref(visible);
#endif /* ONLY_SHAPED_WINDOW */

	return (0);
}

