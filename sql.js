var IsqlConnection = require('./IsqlConnection.js');

var con = new IsqlConnection({ DSN : 'server_dsn_name', userName : 'user_name', password : 'password'});

process.stdin.on('data',function (strInput) {
 	con.query(strInput, function (rs) {
		console.log(rs.length);
	});
});
 
process.stdin.resume();
