/****************************************************************************************************
 * isql-json
 ****************************************************************************************************
 * This code was adapted by Dan VerWeire (dverweire@gmail.com) from the original isql code by 
 * Peter Harvey (pharvey@codebydesign.com) which was released under GPL 18.FEB.99
 * 
 * I have attempted to remove all irrelevant code from the original isql so that we can have a small
 * and hopefully fast tool that can query odbc databases and return the results in JSON encoding.
 * 
 * My intention for this is a stop-gap measure for the lack of unixODBC support in node.js. I am not
 * a c/c++ programmer and this is the best I can do on my own.
 * 
 * I hope I'm not breaking an rules. 
 * 
 * Thanks to Peter Harvey and anyone else who worked on the original isql code. 
 * 
 ****************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>

char *szSyntax =
"\n" \
" Usage: isql-json [UID [PWD]] [options] \n" \
"\n" \
" Options \n" \
"\n" \
"   -s         buffer size \n" \
"   -c         do not display comma after each record \n" \
"   -p         flush after each reacord \n" \
"   -e         echo the query before execution \n" \
"   -l         echo the line input \n" \
"   -q         specify end of query string. Default is go\\n \n" \
"   -f         specify end of recordset string. Default is end \n" \
"   -3         use ODBC 3 calls \n" \
"   -v         turn on verbose \n" \
"\n" \
" Commands \n" \
"\n" \
"    help - list tables\n" \
"    help table - list columns in table\n" \
"    help help - list all help options\n" \
"\n" \
" Examples                                   \n" \
"                                            \n" \
"    isql-json WebDB MyID MyPWD -q __<<EOQ>>__\\n -f __<<EOD>>__ < my.sql \n" \
"\n" \
"    The file my.sql can contain mulit-line sql commands. \n" \
"    After each complete command to be executed a new line\n" \
"    is required which should contain the string supplied by  \n" \
"    the -f option.  \n" \
"\n" \
"    Example my.sql:  \n" \
"\n" \
"        select top 10 * from customer\n" \
"        __<<EOD>>__\n" \
"      \n" \
"\n\n";




#define MAX_DATA_WIDTH 5000000

#ifndef max
#define max( a, b ) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min( a, b ) (((a) < (b)) ? (a) : (b))
#endif


