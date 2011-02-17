node-unixodbc-isql-json
=======================

A hacky solution for querying ODBC data sources (including MS SQL) with nodejs.

Rather than develop a C++ module for nodejs that would bind to the unixODBC libraries, I have hacked
the original isql tool to accept SQL queries via stdin and then output the resulting recordset via stdout using 
JSON encoding. I call this tool isql-json. 

Using the nodejs child_process class, we can spawn a process of isql-json and pass queries to it via the
child_process's stdin stream and receive responses via the child_process's stdout stream.

This is by no means a finished product. There is still a ton of error handling and optimizations that need
to be addressed. 


Use at your own risk.


Install
-------

On an Ubuntu machine you need to make sure you have odbc installed:

	apt-get install unixodbc unixodbc-dev


If you are going to be querying a MS SQL server, be sure to install the tds packages:

	apt-get install freetds-bin freetds-common tdsodbc


Get the source for node-unixodbc-isql-json from github:

	git clone git://github.com/wankdanker/node-unixodbc-isql-json.git


In the isql-json directory, do the standard configure and make

	cd isql-json
	./configure
	make


Modify sql.js to include the proper DSN, userName and password for your data source


Try it out:

	node sql.js
	select top 10 * from tableName

