var 	sys = require('sys'),
	util = require('util'),
	spawn = require('child_process').spawn

var markStampFirst = null;
var markStampLast = null;
var markDebug = true;

function map(obj, fn, squash) {
	var a = [];
	
	for (var key in obj) {
		if (obj.hasOwnProperty(key)) {
			var t = fn.call(obj[key], key, obj[key], function (obj) { return a.push(obj) } );
			
			if ((t && squash) || !squash) {
				a.push(t);
			}
		}
	}
	return a;
}



function mark(str) {
	if (markDebug) {
		if (!markStampFirst) {
			markStampFirst = (new Date()).getTime();
			markStampLast = markStampFirst;
			
			console.log("*mark: \t0ms \t0ms -- " + str);
		}
		else {
			var t = (new Date()).getTime();
			var totalDiff = t - markStampFirst;
			var lastDiff = t - markStampLast;
			
			markStampLast = t;
			
			console.log("mark: \t" + totalDiff + "ms \t" + lastDiff + "ms -- " + str);
		}
	}
}

function IsqlConnection (options) {
	var self = this;
	var isql = null;
	var input = [];
	var callback = null;
	var isConnected = false;
	
	var endOfQuery = '__<<EOQ>>__\n';
	var endOfData = '__<<EOD>>__';
	var endOfDataRegEx = new RegExp(endOfData,"gm");
	
	self.settings = options || {};
	
	self.connect = function () {
		mark('IsqlConnection.connect - top');
		input = [];
		
		isql = spawn('isql-json/isql-json', ///usr/local/bin/isql',
			[
				self.settings.DSN,
				self.settings.userName,
				self.settings.password,
				'-q __\<\<EOQ\>\>__\\n',
				'-f __\<\<EOD\>\>__'
			]);
		
		mark('IsqlConnection.connec - post-spawn');
		
		
		isConnected = true;
		
		isql.stderr.on('data', function (data) {
			console.log('stderr: ' + data);
		});
		
		isql.stdout.setEncoding('utf8');
		
		isql.stdout.on('data', function (data) {
			//mark('IsqlConnection - got data :' + data.toString());
			var strData = data.toString()
			//mark('IsqlConnection - regEx test:' + endOfDataRegEx.test(strData));
			
			if (endOfDataRegEx.test(strData)) {
				input.push(strData.replace(endOfDataRegEx, ''));
				
				mark('IsqlConnection - pre-parse');
				var data = JSON.parse(input.join(''));
				mark('IsqlConnection - post-parse');
				
				input = [];
				
				if (callback) {
					callback(data);
				}
			}
			else {
				input.push(strData);
			}
		});
		
		isql.on('exit', function (code) {
			mark('IsqlConnection - connection exit');
			isConnected = false;
		});
	};
	
	self.isConnected = function () {
		return isConnected;
	}
	
	self.query = function (strQuery, fn) {
		markStampFirst = null;
		
		if (!self.isConnected()) {
			self.connect();
		}
		
		mark('IsqlConnection - issuing new query');
		
		if (strQuery) {
			isql.stdin.write(strQuery + '\n__<<EOQ>>__\n');
			callback = fn
		}
	};
}

module.exports = IsqlConnection;
