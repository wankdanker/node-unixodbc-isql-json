bin_PROGRAMS = isql-json 

INCLUDES = -I@top_srcdir@/ -I/usr/include/

EXTRA_DIST = \
	isql-json.h

isql_json_LDADD = \
	/usr/lib64/libodbc.so \
        $(READLINE)

#/usr/lib64/libodbcextraslc.so

isql_json_DEPENDENCIES = \
	/usr/lib64/libodbc.so

isql_json_SOURCES = isql-json.c

