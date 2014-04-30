var zmq = require('zmq'), sock = zmq.socket('dealer');

sock.connect('tcp://127.0.0.1:8050');

sock.on('message', function(){
  Array.prototype.forEach.call(arguments, function (elem, index) {
    console.log(elem + " | " + index);
  });

  process.exit(0);
});

var list = new Array("", "PC01", "hello", "world");
//var list = new Array("111");

sock.send(list);
