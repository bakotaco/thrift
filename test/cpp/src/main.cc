#include <concurrency/ThreadManager.h>
#include <concurrency/PosixThreadFactory.h>
#include <concurrency/Monitor.h>
#include <concurrency/Util.h>
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <server/TThreadPoolServer.h>
#include <transport/TServerSocket.h>
#include <transport/TSocket.h>
#include <transport/TBufferedTransport.h>
#include "StressTest.h"

#include <iostream>
#include <set>
#include <stdexcept>
#include <sstream>

using namespace std;

using namespace facebook::thrift;
using namespace facebook::thrift::protocol;
using namespace facebook::thrift::transport;
using namespace facebook::thrift::server;

using namespace test::stress;

class Server : public ServiceServerIf {
 public:
  Server(shared_ptr<TProtocol> protocol) :
    ServiceServerIf(protocol) {}

  void echoVoid() {return;}
  uint8_t echoByte(uint8_t arg) {return arg;}
  int16_t echoI16(int16_t arg) {return arg;}
  int32_t echoI32(int32_t arg) {return arg;}
  int64_t echoI64(int64_t arg) {return arg;}
  uint16_t echoU16(uint16_t arg) {return arg;}
  uint32_t echoU32(uint32_t arg) {return arg;}
  uint64_t echoU64(uint64_t arg) {return arg;}
  string echoString(string arg) {return arg;}
  list<uint8_t> echoList(list<uint8_t> arg) {return arg;}
  set<uint8_t> echoSet(set<uint8_t> arg) {return arg;}
  map<uint8_t, uint8_t> echoMap(map<uint8_t, uint8_t> arg) {return arg;}
};

class ClientThread: public Runnable {
public:

  ClientThread(shared_ptr<TTransport>transport, shared_ptr<ServiceClient> client, Monitor& monitor, size_t& workerCount, size_t loopCount, TType loopType) :
    _transport(transport),
    _client(client),
    _monitor(monitor),
    _workerCount(workerCount),
    _loopCount(loopCount),
    _loopType(loopType)
  {}

  void run() {

    // Wait for all worker threads to start 

    {Synchronized s(_monitor);
	while(_workerCount == 0) {
	  _monitor.wait();
	}
    }

    _startTime = Util::currentTime();

    _transport->open();

    switch(_loopType) {
    case T_VOID: loopEchoVoid(); break;
    case T_BYTE: loopEchoByte(); break;
    case T_I16: loopEchoI16(); break;
    case T_I32: loopEchoI32(); break;
    case T_I64: loopEchoI64(); break;
    case T_U16: loopEchoU16(); break;
    case T_U32: loopEchoU32(); break;
    case T_U64: loopEchoU64(); break;
    case T_STRING: loopEchoString(); break;
    default: cerr << "Unexpected loop type" << _loopType << endl; break;
    }
    
    _endTime = Util::currentTime();

    _transport->close();
    
    _done = true;
      
    {Synchronized s(_monitor);

      _workerCount--;
	  
      if(_workerCount == 0) {
	
	_monitor.notify();
      }
    }
  }

  void loopEchoVoid() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      _client->echoVoid();
    }
  }

  void loopEchoByte() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      uint8_t arg = 1;
      uint8_t result;
      result =_client->echoByte(arg);
      assert(result == arg);
    }
  }
  
  void loopEchoI16() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      uint16_t arg = 1;
      uint16_t result;
      result =_client->echoI16(arg);
      assert(result == arg);
    }
  }

  void loopEchoI32() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      uint32_t arg = 1;
      uint32_t result;
      result =_client->echoI32(arg);
      assert(result == arg);
    }
  }

  void loopEchoI64() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      uint64_t arg = 1;
      uint64_t result;
      result =_client->echoI64(arg);
      assert(result == arg);
    }
  }
  
  void loopEchoU16() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      uint16_t arg = 1;
      uint16_t result;
      result =_client->echoU16(arg);
      assert(result == arg);
    }
  }

  void loopEchoU32() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      uint32_t arg = 1;
      uint32_t result;
      result =_client->echoU32(arg);
      assert(result == arg);
    }
  }

  void loopEchoU64() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      uint64_t arg = 1;
      uint64_t result;
      result =_client->echoU64(arg);
      assert(result == arg);
    }
  }
  
  void loopEchoString() {
    for(size_t ix = 0; ix < _loopCount; ix++) {
      string arg = "hello";
      string result;
      result =_client->echoString(arg);
      assert(result == arg);
    }
  }
  
  shared_ptr<TTransport> _transport;
  shared_ptr<ServiceClient> _client;
  Monitor& _monitor;
  size_t& _workerCount;
  size_t _loopCount;
  TType _loopType;
  long long _startTime;
  long long _endTime;
  bool _done;
  Monitor _sleep;
};
    
int main(int argc, char **argv) {

  int port = 9090;
  string serverType = "thread-pool";
  string protocolType = "binary";
  size_t workerCount = 4;
  size_t clientCount = 10;
  size_t loopCount = 10000;
  TType loopType  = T_VOID;
  string callName = "echoVoid";
  bool runServer = true;

  ostringstream usage;

  usage <<
    argv[0] << " [--port=<port number>] [--server] [--server-type=<server-type>] [--protocol-type=<protocol-type>] [--workers=<worker-count>] [--clients=<client-count>] [--loop=<loop-count>]" << endl <<
    "\tclients        Number of client threads to create - 0 implies no clients, i.e. server only.  Default is " << clientCount << endl <<
    "\thelp           Prints this help text." << endl <<
    "\tcall           Service method to call.  Default is " << callName << endl <<
    "\tloop           The number of remote thrift calls each client makes.  Default is " << loopCount << endl <<
    "\tport           The port the server and clients should bind to for thrift network connections.  Default is " << port << endl <<
    "\tserver         Run the Thrift server in this process.  Default is " << runServer << endl <<
    "\tserver-type    Type of server, \"simple\" or \"thread-pool\".  Default is " << serverType << endl <<
    "\tprotocol-type  Type of protocol, \"binary\", \"ascii\", or \"xml\".  Default is " << protocolType << endl <<
    "\tworkers        Number of thread pools workers.  Only valid for thread-pool server type.  Default is " << workerCount << endl;
    
  map<string, string>  args;
  
  for(int ix = 1; ix < argc; ix++) {

    string arg(argv[ix]);

    if(arg.compare(0,2, "--") == 0) {

      size_t end = arg.find_first_of("=", 2);

      string key = string(arg, 2, end - 2);

      if(end != string::npos) {
	args[key] = string(arg, end + 1);
      } else {
	args[key] = "true";
      }
    } else {
      throw invalid_argument("Unexcepted command line token: "+arg);
    }
  }

  try {

    if(!args["clients"].empty()) {
      clientCount = atoi(args["clients"].c_str());
    }

    if(!args["help"].empty()) {
      cerr << usage.str();
      return 0;
    }

    if(!args["loop"].empty()) {
      loopCount = atoi(args["loop"].c_str());
    }

    if(!args["call"].empty()) {
      callName = args["call"];
    }

    if(!args["port"].empty()) {
      port = atoi(args["port"].c_str());
    }

    if(!args["server"].empty()) {
      runServer = args["server"] == "true";
    }

    if(!args["server-type"].empty()) {
      serverType = args["server-type"];
      
      if(serverType == "simple") {

      } else if(serverType == "thread-pool") {

      } else {

	throw invalid_argument("Unknown server type "+serverType);
      }
    }

    if(!args["workers"].empty()) {
      workerCount = atoi(args["workers"].c_str());
    }

  } catch(exception& e) {
    cerr << e.what() << endl;
    cerr << usage;
  }

  shared_ptr<PosixThreadFactory> threadFactory = shared_ptr<PosixThreadFactory>(new PosixThreadFactory());

  if(runServer) {

    // Dispatcher
    shared_ptr<TBinaryProtocol> binaryProtocol(new TBinaryProtocol);

    shared_ptr<Server> server(new Server(binaryProtocol));

    // Options
    shared_ptr<TServerOptions> serverOptions(new TServerOptions());

    // Transport
    shared_ptr<TServerSocket> serverSocket(new TServerSocket(port));

    // ThreadFactory

    shared_ptr<Thread> serverThread;

    if(serverType == "simple") {
      
      serverThread = threadFactory->newThread(shared_ptr<Runnable>(new TSimpleServer(server, serverOptions, serverSocket)));
      
    } else if(serverType == "thread-pool") {

      shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(workerCount);

      threadManager->threadFactory(threadFactory);

      threadManager->start();

      serverThread = threadFactory->newThread(shared_ptr<TServer>(new TThreadPoolServer(server,
											serverOptions,
											serverSocket,
											threadManager)));
    }

    cerr << "Starting the server on port " << port << endl;

    serverThread->start();
    
    // If we aren't running clients, just wait forever for external clients

    if(clientCount == 0) {
      serverThread->join();
    }
  }

  if(clientCount > 0) {

    Monitor monitor;

    size_t threadCount = 0;

    set<shared_ptr<Thread> > clientThreads;

    if(callName == "echoVoid") { loopType = T_VOID;}
    else if(callName == "echoByte") { loopType = T_BYTE;}
    else if(callName == "echoI16") { loopType = T_I16;}
    else if(callName == "echoI32") { loopType = T_I32;}
    else if(callName == "echoI64") { loopType = T_I64;}
    else if(callName == "echoU16") { loopType = T_U16;}
    else if(callName == "echoU32") { loopType = T_U32;}
    else if(callName == "echoU64") { loopType = T_U64;}
    else if(callName == "echoString") { loopType = T_STRING;}
    else {throw invalid_argument("Unknown service call "+callName);}

    for(size_t ix = 0; ix < clientCount; ix++) {
    
      shared_ptr<TSocket> socket(new TSocket("127.0.01", port));
      shared_ptr<TBufferedTransport> bufferedSocket(new TBufferedTransport(socket, 2048));
      shared_ptr<TBinaryProtocol> binaryProtocol(new TBinaryProtocol());
      shared_ptr<ServiceClient> serviceClient(new ServiceClient(bufferedSocket, binaryProtocol));
    
      clientThreads.insert(threadFactory->newThread(shared_ptr<ClientThread>(new ClientThread(bufferedSocket, serviceClient, monitor, threadCount, loopCount, loopType))));
    }
  
    for(std::set<shared_ptr<Thread> >::const_iterator thread = clientThreads.begin(); thread != clientThreads.end(); thread++) {
      (*thread)->start();
    }

    long long time00;
    long long time01;
  
    {Synchronized s(monitor);
      threadCount = clientCount;
    
      cerr << "Launch "<< clientCount << " client threads" << endl;
    
      time00 =  Util::currentTime();
    
      monitor.notifyAll();
      
      while(threadCount > 0) {
	monitor.wait();
      }
    
      time01 =  Util::currentTime();
    }
  
    long long firstTime = 9223372036854775807LL;
    long long lastTime = 0;

    double averageTime = 0;
    long long minTime = 9223372036854775807LL;
    long long maxTime = 0;
  
    for(set<shared_ptr<Thread> >::iterator ix = clientThreads.begin(); ix != clientThreads.end(); ix++) {
      
      shared_ptr<ClientThread> client = dynamic_pointer_cast<ClientThread>((*ix)->runnable());
      
      long long delta = client->_endTime - client->_startTime;
      
      assert(delta > 0);

      if(client->_startTime < firstTime) {
	firstTime = client->_startTime;
      }
      
      if(client->_endTime > lastTime) {
	lastTime = client->_endTime;
      }
      
      if(delta < minTime) {
	minTime = delta;
      }
      
      if(delta > maxTime) {
	maxTime = delta;
      }
      
      averageTime+= delta;
    }
    
    averageTime /= clientCount;
    
    
    cout <<  "workers :" << workerCount << ", client : " << clientCount << ", loops : " << loopCount << ", rate : " << (clientCount * loopCount * 1000) / ((double)(time01 - time00)) << endl;
    
    cerr << "done." << endl;
  }

  return 0;
}