var IsqlConnection = require('./IsqlConnection.js');

var con = new IsqlConnection({ DSN : 'odbc_dsn_name', userName : 'user_name', password : 'password'});

process.stdout.write('> ');

var queryHistory = [];

process.stdin.on('data',function (strInput) {
	queryHistory.push(strInput);
	
 	con.query(strInput, function (rs) {
		console.log(rs.length + ' records returned');
		
		process.stdout.write('> ');
	});
});
 
process.stdin.resume();
