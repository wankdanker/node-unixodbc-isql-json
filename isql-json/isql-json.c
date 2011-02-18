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

#include <config.h>
#include "isql-json.h"
#ifdef HAVE_READLINE
    #include <readline/readline.h>
    #include <readline/history.h>
#endif

#ifdef HAVE_SETLOCALE
    #ifdef HAVE_LOCALE_H
        #include <locale.h>
    #endif 
#endif

static int OpenDatabase( SQLHENV *phEnv, SQLHDBC *phDbc, char *szDSN, char *szUID, char *szPWD );
static int ExecuteSQL( SQLHDBC hDbc, char *szSQL );
static int ExecuteHelp( SQLHDBC hDbc, char *szSQL );
static int CloseDatabase( SQLHENV hEnv, SQLHDBC hDbc );

static void WriteHeaderJSON( SQLHSTMT hStmt );
static void WriteBodyJSON( SQLHSTMT hStmt );
static void WriteFooterJSON( SQLHSTMT hStmt );

static int DumpODBCLog( SQLHENV hEnv, SQLHDBC hDbc, SQLHSTMT hStmt );
static int get_args(char *string, char **args, int maxarg);
static void free_args(char **args, int maxarg);
static void output_help(void);
static void unescape( char * t, char * s);


SQLHENV hEnv                        = 0;
SQLHDBC hDbc                        = 0;
int	bVerbose                    = 0;
int     version3                    = 0;
int     bEchoQuery                  = 0;
int     bEchoLine                   = 0;
int     bFlushRecord                = 0;
int     bComma                      = 1;
SQLUSMALLINT    has_moreresults     = 1;
char    *szEOD;

int main( int argc, char *argv[] )
{
    int     nArg, count;
    char    *szDSN;
    char    *szUID;
    char    *szPWD;
    char    *szSQL;
    char    *szEOQ;

    char    *line_buffer;
    int     buffer_size = 9000;
    int     line_buffer_size = 9000;
    int     bufpos,linen;
    char    prompt[24];

    szDSN = NULL;
    szUID = NULL;
    szPWD = NULL;
    szEOQ = "go\n";
    szEOD = "end\n";
    
    if ( argc < 2 )
    {
        fputs( szSyntax, stderr );
        exit( 1 );
    }

#ifdef HAVE_SETLOCALE
    /*
     * Default locale
     */
    setlocale( LC_ALL, "" );
#endif

    /****************************
     * PARSE ARGS
     ***************************/
    for ( nArg = 1, count = 1 ; nArg < argc; nArg++ )
    {
        if ( argv[nArg][0] == '-' )
        {
            /* Options */
            switch ( argv[nArg][1] )
            {
                case 's':
                    buffer_size = atoi( &(argv[nArg][2]) );
                    line_buffer_size = buffer_size;
                    break;
		case 'c':
		    bComma = 0;
		    break;
		case 'p':
		    bFlushRecord = 1;
		    break;
		case 'e':
		    bEchoQuery = 1;
		    break;
		case 'l':
		    bEchoLine = 1;
		    break;
		case 'q':
		    szEOQ = calloc(sizeof(char), strlen(argv[nArg]+3));
		    unescape(argv[nArg]+3, szEOQ);

		    break;
		case 'f':
		    szEOD = calloc(sizeof(char), strlen(argv[nArg]+3));
		    unescape(argv[nArg]+3, szEOD);

                    break;
                case '3':
                    version3 = 1;
                case 'v':
                    bVerbose = 1;
                    break;
                case '-':
                    printf( "unixODBC " VERSION "\n" );
                    exit(0);
                default:
                    fputs( szSyntax, stderr );
                    exit( 1 );
            }
            continue;
        }
        else if ( count == 1 )
            szDSN = argv[nArg];
        else if ( count == 2 )
            szUID = argv[nArg];
        else if ( count == 3 )
            szPWD = argv[nArg];
        count++;
    }

    szSQL = calloc( 1, buffer_size + 1 );
    line_buffer = calloc( 1, buffer_size + 1 );

    /****************************
     * CONNECT
     ***************************/

    if ( !OpenDatabase( &hEnv, &hDbc, szDSN, szUID, szPWD ) )
        exit( 1 );

    /****************************
     * EXECUTE
     ***************************/

    //here we could printf something indicating that we are connected

    linen = 0;
    bufpos = 0;

    do
    {
        char *line = NULL;
        int malloced = 0;
        int len = 0;
        int dont_copy, exec_now;

	exec_now = 0;
	
        szSQL[ bufpos ] = '\0';

	line = fgets( line_buffer, line_buffer_size, stdin );
	if ( !line )        /* EOF - ctrl D */
	{
		malloced = 1;
		line = strdup( "quit" );
	}
	else
	{
		if ( line[ 0 ] == '\n' ) 
		{
			malloced = 1;
			//line = strdup( "quit" );
			
		}
		else 
		{
			malloced = 0;
		}
	}
        
	len = strlen(line);
	
        dont_copy = 0;

	if ( len >= 4 && memcmp( line, "quit", 4 ) == 0 )
	{
		if ( malloced )
		{
			free(line);
		}
		break;
	}

        /*
         * check to see if this line is the "end of query" string
         */
	if (bEchoLine) {
		printf( "{ \"lineEcho\" : \"%s\"}" , line);
	}
	
//  	printf("%d ", len);
//  	printf("%d ", strlen(szEOQ));
//  	printf("%s ", szEOQ);
	
 	if ( len == strlen(szEOQ) && memcmp( line, szEOQ, strlen(szEOQ) ) == 0 ) //line[ 0 ] == '\0' )
	{
		dont_copy = 1;
		exec_now = 1;
	}

        if ( !dont_copy )
        {
            /*
             * is there space
             */

            if ( len > 0 && bufpos + len + 2 > buffer_size )
            {
                szSQL = realloc( szSQL, bufpos + len + 2 );
                buffer_size = bufpos + len + 2;
            }

            /*
             * insert space between the lines
             * the above length check will make sure there is room for 
             * the extra space
             */
            if ( linen > 1 )
            {
                szSQL[ bufpos ] = ' ';
                bufpos ++;
            }

            memcpy( szSQL + bufpos, line, len );
            bufpos += len;
            szSQL[ bufpos ] = '\0';
        }

	if (exec_now > 0) {
		if ( bufpos >= 4 && memcmp( szSQL, "help", 4 ) == 0 )
		{
			ExecuteHelp( hDbc, szSQL);
		}
		else
		{
			ExecuteSQL( hDbc, szSQL);
		}
		
		linen = 0;
		bufpos = 0;
	}
	
    } while ( 1 );

    /****************************
     * DISCONNECT
     ***************************/
    CloseDatabase( hEnv, hDbc );

    exit( 0 );
}

/****************************
 * OpenDatabase - do everything we have to do to get a viable connection to szDSN
 ***************************/
static int
OpenDatabase( SQLHENV *phEnv, SQLHDBC *phDbc, char *szDSN, char *szUID, char *szPWD )
{
    if ( version3 )
    {
        if ( SQLAllocHandle( SQL_HANDLE_ENV, NULL, phEnv ) != SQL_SUCCESS )
        {
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocHandle( SQL_HANDLE_ENV )\n" );
            return 0;
        }

        if ( SQLSetEnvAttr( *phEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) 3, 0 ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, 0, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLSetEnvAttr( SQL_HANDLE_DBC )\n" );
            SQLFreeHandle( SQL_HANDLE_ENV, *phEnv );
            return 0;
        }

        if ( SQLAllocHandle( SQL_HANDLE_DBC, *phEnv, phDbc ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, 0, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocHandle( SQL_HANDLE_DBC )\n" );
            SQLFreeHandle( SQL_HANDLE_ENV, *phEnv );
            return 0;
        }

        if ( !SQL_SUCCEEDED( SQLConnect( *phDbc, (SQLCHAR*)szDSN, SQL_NTS, (SQLCHAR*)szUID, SQL_NTS, (SQLCHAR*)szPWD, SQL_NTS )))
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLConnect\n" );
            SQLFreeHandle( SQL_HANDLE_DBC, *phDbc );
            SQLFreeHandle( SQL_HANDLE_ENV, *phEnv );
            return 0;
        }
    }
    else
    {
        if ( SQLAllocEnv( phEnv ) != SQL_SUCCESS )
        {
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocEnv\n" );
            return 0;
        }

        if ( SQLAllocConnect( *phEnv, phDbc ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, 0, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocConnect\n" );
            SQLFreeEnv( *phEnv );
            return 0;
        }

        if ( !SQL_SUCCEEDED( SQLConnect( *phDbc, (SQLCHAR*)szDSN, SQL_NTS, (SQLCHAR*)szUID, SQL_NTS, (SQLCHAR*)szPWD, SQL_NTS )))
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLConnect\n" );
            SQLFreeConnect( *phDbc );
            SQLFreeEnv( *phEnv );
            return 0;
        }
    }

    /*
     * does the driver support SQLMoreResults
     */

    if ( !SQL_SUCCEEDED( SQLGetFunctions( *phDbc, SQL_API_SQLMORERESULTS, &has_moreresults )))
    {
        has_moreresults = 0;
    }

    return 1;
}

/****************************
 * ExecuteSQL - create a statement, execute the SQL, and get rid of the statement
 *            - show results as per request; bHTMLTable has precedence over other options
 ***************************/
static int
ExecuteSQL( SQLHDBC hDbc, char *szSQL )
{
    SQLHSTMT        hStmt;
    SQLSMALLINT     cols;
    SQLLEN          nRows                   = 0;
    SQLINTEGER      ret;
    SQLCHAR         *szSepLine;
    int             mr;

    szSepLine = calloc(1, 32001);

    /****************************
     * EXECUTE SQL
     ***************************/
    if ( version3 )
    {
        if ( SQLAllocHandle( SQL_HANDLE_STMT, hDbc, &hStmt ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocHandle( SQL_HANDLE_STMT )\n" );
            free(szSepLine);
            return 0;
        }
    }
    else
    {
        if ( SQLAllocStmt( hDbc, &hStmt ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocStmt\n" );
            free(szSepLine);
            return 0;
        }
    }

    if ( SQLPrepare( hStmt, (SQLCHAR*)szSQL, strlen( szSQL )) != SQL_SUCCESS )
    {
        if ( bVerbose ) DumpODBCLog( hEnv, hDbc, hStmt );
        fprintf( stderr, "[ISQL]ERROR: Could not SQLPrepare\n" );
        SQLFreeStmt( hStmt, SQL_DROP );
        free(szSepLine);
        return 0;
    }

    ret =  SQLExecute( hStmt );

    if ( ret == SQL_NO_DATA )
    {
        fprintf( stderr, "[ISQL]INFO: SQLExecute returned SQL_NO_DATA\n" );
    }
    else if ( ret == SQL_SUCCESS_WITH_INFO )
    {
        if ( bVerbose ) DumpODBCLog( hEnv, hDbc, hStmt );
        fprintf( stderr, "[ISQL]INFO: SQLExecute returned SQL_SUCCESS_WITH_INFO\n" );
    }
    else if ( ret != SQL_SUCCESS )
    {
        if ( bVerbose ) DumpODBCLog( hEnv, hDbc, hStmt );
        fprintf( stderr, "[ISQL]ERROR: Could not SQLExecute\n" );
        SQLFreeStmt( hStmt, SQL_DROP );
        free(szSepLine);
        return 0;
    }

    /*
     * Loop while SQLMoreResults returns success
     */

    mr = 0;

    do
    {
        if ( mr )
        {
            if ( ret == SQL_SUCCESS_WITH_INFO )
            {
                if ( bVerbose ) DumpODBCLog( hEnv, hDbc, hStmt );
                fprintf( stderr, "[ISQL]INFO: SQLMoreResults returned SQL_SUCCESS_WITH_INFO\n" );
            }
        }
        mr = 1;
        strcpy ((char*) szSepLine, "" ) ;

        /*
         * check to see if it has generated a result set
         */

        if ( SQLNumResultCols( hStmt, &cols ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, hStmt );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLNumResultCols\n" );
            SQLFreeStmt( hStmt, SQL_DROP );
            free(szSepLine);
            return 0;
        }

	/****************************
	* WRITE HEADER
	***************************/
	WriteHeaderJSON( hStmt);

	/****************************
	* WRITE BODY
	***************************/
	WriteBodyJSON( hStmt );
        
        /****************************
         * WRITE FOOTER
         ***************************/
	WriteFooterJSON( hStmt );
    }
    while ( has_moreresults && SQL_SUCCEEDED( ret = SQLMoreResults( hStmt )));

    /****************************
     * CLEANUP
     ***************************/
    SQLFreeStmt( hStmt, SQL_DROP );
    free(szSepLine);

    return 1;
}

/****************************
 * ExecuteHelp - create a statement, execute the SQL, and get rid of the statement
 *             - show results as per request; bHTMLTable has precedence over other options
 ***************************/
static int
ExecuteHelp( SQLHDBC hDbc, char *szSQL )
{
    SQLHSTMT hStmt;
    SQLLEN nRows = 0;
    SQLRETURN nReturn;
    SQLCHAR *szSepLine;
    char *args[10];
    int n_args;

    if (!(szSepLine = calloc(1, 32001)))
    {
        fprintf(stderr, "[ISQL]ERROR: Failed to allocate line");
        return 0;
    }

    /****************************
     * EXECUTE SQL
     ***************************/
    if ( version3 )
    {
        if ( SQLAllocHandle( SQL_HANDLE_STMT, hDbc, &hStmt ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocHandle( SQL_HANDLE_STMT )\n" );
            free(szSepLine);
            return 0;
        }
    }
    else
    {
        if ( SQLAllocStmt( hDbc, &hStmt ) != SQL_SUCCESS )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, 0 );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLAllocStmt\n" );
            free(szSepLine);
            return 0;
        }
    }
    n_args = get_args(szSQL, &args[0], sizeof(args) / sizeof(args[0]));

    if (n_args == 2 )
    {
        if (strcmp(args[1], "help") == 0)
        {
            output_help();
            free(szSepLine);
            return 0;
        }

        /* COLUMNS */
        nReturn = SQLColumns( hStmt, NULL, 0, NULL, 0, (SQLCHAR*)args[1], SQL_NTS, NULL, 0 );
        if ( (nReturn != SQL_SUCCESS) && (nReturn != SQL_SUCCESS_WITH_INFO) )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, hStmt );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLColumns\n" );
            SQLFreeStmt( hStmt, SQL_DROP );
            free(szSepLine);
            return 0;
        }
    }
    else
    {
        SQLCHAR *catalog = NULL;
        SQLCHAR *schema = NULL;
        SQLCHAR *table = NULL;
        SQLCHAR *type = NULL;

        if (n_args > 2)
        {
            catalog = (SQLCHAR*)args[1];
            schema = (SQLCHAR*)args[2];
            table = (SQLCHAR*)args[3];
            type = (SQLCHAR*)args[4];
        }

        /* TABLES */
        nReturn = SQLTables( hStmt, catalog, SQL_NTS, schema, SQL_NTS,
                             table, SQL_NTS, type, SQL_NTS );
        if ( (nReturn != SQL_SUCCESS) && (nReturn != SQL_SUCCESS_WITH_INFO) )
        {
            if ( bVerbose ) DumpODBCLog( hEnv, hDbc, hStmt );
            fprintf( stderr, "[ISQL]ERROR: Could not SQLTables\n" );
            SQLFreeStmt( hStmt, SQL_DROP );
            free(szSepLine);
            free_args(args, sizeof(args) / sizeof(args[0]));
            return 0;
        }
    }

    /****************************
     * WRITE HEADER
     ***************************/
	WriteHeaderJSON( hStmt );

    /****************************
     * WRITE BODY
     ***************************/
	WriteBodyJSON( hStmt );

    /****************************
     * WRITE FOOTER
     ***************************/
	WriteFooterJSON( hStmt );

    /****************************
     * CLEANUP
     ***************************/
    SQLFreeStmt( hStmt, SQL_DROP );
    free(szSepLine);
    free_args(args, sizeof(args) / sizeof(args[0]));
    return 1;
}


/****************************
 * CloseDatabase - cleanup in prep for exiting the program
 ***************************/
static int CloseDatabase( SQLHENV hEnv, SQLHDBC hDbc )
{
    SQLDisconnect( hDbc );
    if ( version3 )
    {
        SQLFreeHandle( SQL_HANDLE_DBC, hDbc );
        SQLFreeHandle( SQL_HANDLE_ENV, hEnv );
    }
    else
    {
        SQLFreeConnect( hDbc );
        SQLFreeEnv( hEnv );
    }

    return 1;
}

/****************************
 * WRITE JSON
 ***************************/


//this is from cJSON
static char *print_string_ptr(const char *str)
{
	const char *ptr;
	char *ptr2,*out;
	int len=0;
	
	ptr=str;
	
	while (*ptr && ++len) {
		if ((unsigned char)*ptr<32 || *ptr=='\"' || *ptr=='\\') len++;ptr++;
	}
	
	out=(char*)malloc(len+3);
	ptr2=out;
	ptr=str;
	
	*ptr2++='\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++='\\';
			switch (*ptr++)
			{
				case '\\':	*ptr2++='\\';	break;
				case '\"':	*ptr2++='\"';	break;
				case '\b':	*ptr2++='b';	break;
				case '\f':	*ptr2++='f';	break;
				case '\n':	*ptr2++='n';	break;
				case '\r':	*ptr2++='r';	break;
				case '\t':	*ptr2++='t';	break;
				default: ptr2--;	break;	// eviscerate with prejudice.
			}
		}
	}
	*ptr2++='\"';*ptr2++=0;
	return out;
}

static void WriteHeaderJSON( SQLHSTMT hStmt )
{
    printf( "[" );
}

static void WriteBodyJSON( SQLHSTMT hStmt )
{
    SQLINTEGER      nCol                            = 0;
    SQLSMALLINT     nColumns                        = 0;
    SQLLEN          nIndicator                      = 0;
    SQLCHAR         szColumnValue[MAX_DATA_WIDTH+1];
    SQLCHAR         szColumnName[MAX_DATA_WIDTH+1]; 
    SQLLEN          nColumnType;
    SQLRETURN       nReturn                         = 0;
    SQLRETURN       ret;
    int             nRecordIndex                    = 0;
    SQLLEN          *aColumnTypes                   = NULL;
    SQLCHAR	   **aColumnNames                   = NULL;
   // SQLCHAR	    *szColumnName2                  = NULL;
    *szColumnValue = '\0';

    if ( SQLNumResultCols( hStmt, &nColumns ) != SQL_SUCCESS )
        nColumns = -1;

    aColumnTypes = (SQLLEN *)  malloc(sizeof(SQLLEN) * nColumns);
    aColumnNames = (SQLCHAR**) malloc(nColumns * sizeof(SQLCHAR*));
    
    /*
     * get the ColumnType and ColumnName for each column in the recordset
     * and store them in arrays
     */
    for ( nCol = 1; nCol <= nColumns; nCol++ )
    {
	    SQLColAttribute( hStmt, nCol, SQL_COLUMN_TYPE, NULL, NULL, NULL, &aColumnTypes[ nCol -1 ] );
	    SQLColAttribute( hStmt, nCol, SQL_DESC_LABEL, szColumnName, sizeof(szColumnName), NULL, NULL );
	    
	    aColumnNames[ nCol -1 ] = calloc(strlen(szColumnName) + 1, sizeof(szColumnName[0]));
	    strncpy(aColumnNames[ nCol -1 ], szColumnName, strlen(szColumnName) + 1);
    }
    
    while ( (ret = SQLFetch( hStmt )) == SQL_SUCCESS ) /* ROWS */
    {
	if ( bComma && nRecordIndex > 0 ) {
		printf( ",\n" );
	}
	
	if (bFlushRecord) {
		fflush(stdout);
	}
	
	printf( "{" );

        for ( nCol = 1; nCol <= nColumns; nCol++ ) /* COLS */
        {
	
            nColumnType = aColumnTypes[nCol -1];
      
            printf( "\"%s\":", aColumnNames[nCol -1] );

            nReturn = SQLGetData( hStmt, nCol, SQL_C_CHAR, (SQLPOINTER)szColumnValue, sizeof(szColumnValue), &nIndicator );
	    
            if ( nReturn == SQL_SUCCESS && nIndicator != SQL_NULL_DATA )
            {
		//		printf( "\"" );
                //fputs((char*) szColumnValue, stdout );
		//		printf( "\"" );
		switch (nColumnType) {
			case SQL_NUMERIC :
				printf( "%s", szColumnValue );
				break;
			case SQL_DECIMAL :
				printf( "%s", szColumnValue );
				break;
			case SQL_INTEGER :
				printf( "%s", szColumnValue );
				break;
			case SQL_SMALLINT :
				printf( "%s", szColumnValue );
				break;
			case SQL_FLOAT :
				printf( "%s", szColumnValue );
				break;
			case SQL_REAL :
				printf( "%s", szColumnValue );
				break;
			case SQL_DOUBLE :
				printf( "%s", szColumnValue );
				break;
			default :
				fputs(print_string_ptr((char*) szColumnValue), stdout );
				//printf("%s", szColumnValue);
		}
		
		//fputs(print_string_ptr((char*) szColumnValue), stdout );
            }
            else if ( nReturn == SQL_ERROR )
            {
                ret = SQL_ERROR;
                break;
            }
            else
                printf( " null" );

		if ( nCol < nColumns ) {
			printf (",") ;
		}
        }
        
        if (ret != SQL_SUCCESS)
            break;
	
        printf( "}" );
	
	nRecordIndex++;
    }
    
   // fflush(stdout);
    
    free(aColumnTypes);
}

static void WriteFooterJSON( SQLHSTMT hStmt )
{
    printf( "]\n\n" );
    fflush(stdout);
    printf( "%s", szEOD );
    fflush(stdout);
}




static int DumpODBCLog( SQLHENV hEnv, SQLHDBC hDbc, SQLHSTMT hStmt )
{
    SQLCHAR     szError[501];
    SQLCHAR     szSqlState[10];
    SQLINTEGER  nNativeError;
    SQLSMALLINT nErrorMsg;

    if ( version3 )
    {
        int rec;
        if ( hStmt )
        {
            rec = 0;
            while ( SQLGetDiagRec( SQL_HANDLE_STMT, hStmt, ++rec, szSqlState, &nNativeError, szError, 500, &nErrorMsg ) == SQL_SUCCESS )
            {
                printf( "[%s]%s\n", szSqlState, szError );
            }
        }

        if ( hDbc )
        {
            rec = 0;
            while ( SQLGetDiagRec( SQL_HANDLE_DBC, hDbc, ++rec, szSqlState, &nNativeError, szError, 500, &nErrorMsg ) == SQL_SUCCESS )
            {
                printf( "[%s]%s\n", szSqlState, szError );
            }
        }

        if ( hEnv )
        {
            rec = 0;
            while ( SQLGetDiagRec( SQL_HANDLE_ENV, hEnv, ++rec, szSqlState, &nNativeError, szError, 500, &nErrorMsg ) == SQL_SUCCESS )
            {
                printf( "[%s]%s\n", szSqlState, szError );
            }
        }
    }
    else
    {
        if ( hStmt )
        {
            while ( SQLError( hEnv, hDbc, hStmt, szSqlState, &nNativeError, szError, 500, &nErrorMsg ) == SQL_SUCCESS )
            {
                printf( "[%s]%s\n", szSqlState, szError );
            }
        }

        if ( hDbc )
        {
            while ( SQLError( hEnv, hDbc, 0, szSqlState, &nNativeError, szError, 500, &nErrorMsg ) == SQL_SUCCESS )
            {
                printf( "[%s]%s\n", szSqlState, szError );
            }
        }

        if ( hEnv )
        {
            while ( SQLError( hEnv, 0, 0, szSqlState, &nNativeError, szError, 500, &nErrorMsg ) == SQL_SUCCESS )
            {
                printf( "[%s]%s\n", szSqlState, szError );
            }
        }
    }

    return 1;
}

static int get_args(char *string, char **args, int maxarg) {
    int nargs = 0;
    char *copy;
    char *p;
    const char *sep = " ";
    char *arg;
    unsigned int i;

    if (!string || !args) return 0;

    if (!(copy = strdup(string))) return 0;

    for (i = 0; i < maxarg; i++)
    {
        args[i] = NULL;
    }

    p = copy;
    while ((arg = strtok(p, sep)))
    {
        p = NULL;

        if (strcmp(arg, "\"\"") == 0)
            args[nargs++] = strdup("");
        else if (strcmp(arg, "null") == 0)
            args[nargs++] = NULL;
        else
            args[nargs++] = strdup(arg);
        if (nargs > maxarg) return maxarg;
    }
    free(copy);
    return nargs;
}

static void free_args(char **args, int maxarg) {
    unsigned int i;

    for (i = 0; i < maxarg; i++)
    {
        if (args[i])
        {
            free(args[i]);
            args[i] = NULL;
        }
    }
}

static void output_help(void) {
    fprintf(stderr, \
            "help usage:\n\n" \
            "help help - output this help\n" \
            "help - call SQLTables and output the result-set\n" \
            "help table_name - call SQLColumns for table_name and output the result-set\n" \
            "help catalog schema table type - call SQLTables with these arguments\n" \
            "  where any argument may be specified as \"\" (for the empty string) \n" \
            "  or null to pass a null pointer argument.\n" \
            "\n" \
            " e.g.\n" \
            " help %% \"\" \"\" \"\" - output list of catalogs\n" \
            " help \"\" %% \"\" \"\" - output list of schemas\n" \
            " help null null b%% null - output all tables beginning with b\n" \
            " help null null null VIEW - output list of views\n" \
            "\n");
}

/*  Copies string s to string t, converting escape sequences
    into their appropriate special characters. See the comment
    for escape() for remarks regarding which escape sequences
    are translated.                                    
    
    from:
    http://users.powernet.co.uk/eton/kandr2/krx302.html
    
*/

void unescape( char * t, char * s) {
    int i, j;
    i = j = 0;
    
    while ( t[i] ) {
        switch ( t[i] ) {
        case '\\':
            
            /*  We've found an escape sequence, so translate it  */
            
            switch( t[++i] ) {
            case 'n':
                s[j] = '\n';
                break;
                
            case 't':
                s[j] = '\t';
                break;
                
            case 'a':
                s[j] = '\a';
                break;
                
            case 'b':
                s[j] = '\b';
                break;
                
            case 'f':
                s[j] = '\f';
                break;
                
            case 'r':
                s[j] = '\r';
                break;
                
            case 'v':
                s[j] = '\v';
                break;
                
            case '\\':
                s[j] = '\\';
                break;
                
            case '\"':
                s[j] = '\"';
                break;
                
            default:
                
                /*  We don't translate this escape
                    sequence, so just copy it verbatim  */
                
                s[j++] = '\\';
                s[j] = t[i];
            }
            break;
            
        default:
            
            /*  Not an escape sequence, so just copy the character  */
            
            s[j] = t[i];
        }
        ++i;
        ++j;
    }
    s[j] = t[i];    /*  Don't forget the null character  */
}
