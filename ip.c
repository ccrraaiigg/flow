
/*
 * flow: a Smalltalk streaming framework
 * version 3
 *
 * ip.c - Internet Protocol primitives
 *
 * Craig Latta
 * netjam.org/flow
 */

#include "flow.h"
extern struct VirtualMachine *vm;


/*
 * utilities
 */

int socketClosed(flowSocket *socketPointer) {
  char buffer;
  int  status,
       closed;
  int  socket = socketPointer->resource.handle;

  if (socketPointer->state == flowClosed)
    return TRUE;
  else {
    /*
     * Check for socket closure, without consuming any data that might
     * be readable.
     */
    status = recv(
		  socket,
		  &buffer,
		  1,
		  MSG_PEEK | MSG_OOB | MSG_DONTWAIT);

    if ((status == 0) || ((status == -1) && (lastError() == ECONNRESET))) {
      socketPointer->state = flowClosed;
      killThread(&socketPointer->resource.reading.sync);
      killThread(&socketPointer->resource.writing.sync);
      close(socketPointer->resource.handle);
      free((void *)socketPointer);
      closed = TRUE;}
    else closed = FALSE;

    return closed;}}


/*
 * thread functions
 */

/* Resolve network addresses. */
void resolve(void *parameter) {
  struct hostent *host;
  resolver       *resolverPointer = (resolver *) parameter;
  int            result;
 
  for(;;) {
    waitForThreadSignal(&resolverPointer->sync);
    if (resolverPointer->state == flowClosed) break;
    host = gethostbyname(resolverPointer->hostname);
    result = lastError();

    if (host == 0) {
      if (result == NO_RECOVERY) break;
      else {goto signal;}}

    if (result != HOST_NOT_FOUND) {
      resolverPointer->result = convertedInteger(host->h_addr_list[0] != 0);
      memcpy(
	     resolverPointer->addressBytes,
	     host->h_addr_list[0],
	     4);}

  signal:
    synchronizedSignalSemaphoreWithIndex(resolverPointer->sync.semaphore);}}


/* Connect or accept to open a connection, then handle read requests. */
void waitForConnectionsAndReceivedData(void *parameter) {
  struct timeval delay,
                 *delayPointer;
  flowSocket	 *socketPointer = (flowSocket *) parameter;
  int		 result,
                 selectResult,
                 socket,
                 operation;
  int		 nonblocking = FALSE;
  int            getsockoptResult = 0;
  int            getsockoptOptionLength = sizeof(getsockoptResult);
  fd_set	 readingFileDescriptors,
                 writingFileDescriptors,
                 exceptionFileDescriptors;

  for(;;) {
    waitForThreadSignal(&socketPointer->resource.reading.sync);

    if (socketPointer->resource.reading.timeout == -1)
      delayPointer = NULL;
    else {
      delay.tv_sec = socketPointer->resource.reading.timeout / 1000;
      delay.tv_usec = (socketPointer->resource.reading.timeout % 1000) * 1000;
      delayPointer = &delay;}

    operation = socketPointer->resource.reading.operation;
    socket = socketPointer->resource.handle;
    result = ioctl(
		   socket,
		   FIONBIO,
		   &nonblocking);
    if (result == -1) {
      result = error;
      goto signal;}


    if (operation == flowRead || operation == flowAccept) {
      FD_ZERO(&readingFileDescriptors);
      FD_SET(
	     socket,
	     &readingFileDescriptors);

      /*
       * Perform a blocking select(), so that this thread waits
       * for at least one byte of received data before continuing.
       */
      selectResult = select(
			    socket + 1,
			    &readingFileDescriptors,
			    0,
			    0,
			    delayPointer);

      switch(selectResult) {
        case 0:
	  result = timeout; 
	  break;
        case -1:
	  result = error;
	  break;
        default:
	  result = ready;
	  break;}}

    else if (operation == flowConnect) {
      FD_ZERO(&writingFileDescriptors);
      FD_SET(
	     socket,
	     &writingFileDescriptors);
      FD_ZERO(&exceptionFileDescriptors);
      FD_SET(
	     socket,
	     &exceptionFileDescriptors);

      selectResult = select(
			    socket + 1,
			    0,
			    &writingFileDescriptors,
			    &exceptionFileDescriptors,
			    delayPointer);

      switch(selectResult) {
        case 0:
	  result = timeout; 
	  break;
        case -1:
	  result = error;
	  break;
        default:
	  if (FD_ISSET(
		       socket,
		       &writingFileDescriptors)) {
	    if (
 		 (
		   getsockopt(
			      socket,
			      SOL_SOCKET,
			      SO_ERROR,
			      &getsockoptResult,
			      &getsockoptOptionLength
		 )
                   < 0))
	      // apparently this can happen on Solaris
	      result = error;
	    else
            result = getsockoptResult ? failedConnection : successfulConnection;}
	  else
	    result = error;

	  break;}}

  signal:
    if (result == error) break;

    socketPointer->resource.reading.result = convertedInteger(result);
    if (socketPointer->resource.reading.result == timeout)
      result = result;
    synchronizedSignalSemaphoreWithIndex(socketPointer->resource.reading.sync.semaphore);}}


/* Handle writing requests. */
void waitForSendBufferSpace(void *parameter) {
  struct timeval delay,
                 *delayPointer;
  int		 result,
                 selectResult,
                 socket;
  int		 nonblocking = FALSE;
  fd_set	 writingFileDescriptors,
                 errorFileDescriptors;
  flowSocket	 *socketPointer = (flowSocket *) parameter;

  for(;;) {
    waitForThreadSignal(&socketPointer->resource.writing.sync);

    if (socketPointer->resource.writing.timeout == -1) delayPointer = NULL;
    else {
      delay.tv_sec = socketPointer->resource.writing.timeout / 1000;
      delay.tv_usec = (socketPointer->resource.writing.timeout % 1000) * 1000;

      delayPointer = &delay;}

    socket = socketPointer->resource.handle;
    FD_ZERO(&writingFileDescriptors);
    FD_ZERO(&errorFileDescriptors);
    FD_SET(
	   socket,
	   &writingFileDescriptors);
    FD_SET(
	   socket,
	   &errorFileDescriptors);

    /*
     * Perform a blocking select(), so that this thread waits for at least one byte of
     * send-buffer space to be available before continuing.
     */
    result = ioctl(
		   socket,
		   FIONBIO,
		   &nonblocking);
    if (result == -1) {
      result = error;
      goto signal;}

    selectResult = select(
			  socket + 1,
			  0,
			  &writingFileDescriptors,
			  &errorFileDescriptors,
			  delayPointer);

    switch(selectResult) {
      case 0:
	result = timeout; 
	break;
      case -1:
	result = error;
	break;
      default:
	result = ready;
	break;}

  signal:
    if (result == timeout) {
      result = timeout;}
    if (result == error) {
      break;}

    socketPointer->resource.writing.result = convertedInteger(result);
    synchronizedSignalSemaphoreWithIndex(socketPointer->resource.writing.sync.semaphore);}}


/*
 * primitives
 */

void newResolverHandleInto(void) {
  /* newResourceHandleInto: fourByteInteger */

  writeNewResourceHandle(sizeof(resolver));}


void enableResolver(void) {
  /* startResolver: resolverHandle */

  int      resolverAddress = addressForStackValue(0);
  resolver *resolverPointer = (resolver *) resolverAddress;


  if (!(vm->failed())) {
    resolverPointer->hostname = (char *)malloc(1);
    resolverPointer->hostname[0] = '\0';
    resolverPointer->address.s_addr = 0;
    resolverPointer->state = flowOpen;

    if (!startThread(
		     &resolverPointer->sync,
		     resolve,
		     (void *) resolverAddress))
      vm->primitiveFail();
    else
      vm->pop(1);}}


void registerThatResolverHasResolutionIndex(void) {
  /*
   * registerThatResolver: resolverHandle
   * hasResolutionIndex: resolutionIndex
   */


  resolver *resolverPointer = (resolver *)(addressForStackValue(1));


  if (!(vm->failed())) {
    resolverPointer->sync.semaphore = vm->stackIntegerValue(0);
    vm->pop(2);}}


void notifyAfterResolvingHostNamed(void) {
  /*
   * notify: resolverHandle
   * afterResolvingHostNamed: hostname
   */

  resolver *resolverPointer = (resolver *)(addressForStackValue(1));


  if (!(vm->failed())) {
    if (resolverPointer->state != flowOpen) {
      vm->primitiveFail();
      return;}

    resolverPointer->hostname = copyStringAt(0);
    if (resolverPointer->hostname == NULL) return;

    signalThread(&resolverPointer->sync);
    vm->pop(2);}}


void writeAddressBytesForResolverInto(void) {
  /*
   * writeBytesForResolver: resolverHandle
   * into: aByteArray
   */

  resolver *resolverPointer = (resolver *) (addressForStackValue(1));
  int      addressBytes = vm->stackObjectValue(0);


  if (!(vm->failed())) {
    if (!(vm->fetchClassOf(addressBytes) == (vm->classByteArray()))) {
      vm->primitiveFail();
      return;}
    else {
      memcpy(
	     (void *) (addressBytes + BaseHeaderSize),
	     (unsigned char *) (resolverPointer->addressBytes),
	     4);
      vm->pop(2);}}}


void nameForIPAddressInto(void) {
  /*
   * nameForIPAddress: ipAddress
   * into: aString
   */

  int            name = vm->stackObjectValue(0); /* a String */
  int            address = vm->stackObjectValue(1); /* a ByteArray */
  struct hostent *hostInfo;


  hostInfo = gethostbyaddr(
			   (char *) address + 4,
			   4,
			   AF_INET);
  if (hostInfo == 0) {
    vm->primitiveFail();
    return;}

  strncpy(
	  (char *) (name + BaseHeaderSize),
	  hostInfo->h_name,
	  strlen(hostInfo->h_name));
  vm->pop(2);}


void closeResolver(void) {
  /* close: resolverHandle */

  resolver *resolverPointer = (resolver *) (addressForStackValue(0));


  if (!(vm->failed())) {
    resolverPointer->state = flowClosed;
    stopThread(&resolverPointer->sync);
    free((void *) resolverPointer);
    vm->pop(1);}}


void newSocketHandleInto(void) {
  /* newResourceHandleInto: theHandle */

  /* writeNewResourceHandle() pops the parameter from the object stack */
  writeNewResourceHandle(sizeof(flowSocket));}


void enableSocketUsingTCP(void) {
  /*
   * enableSocket: socketHandle
   * usingTCP: usesTCP
   */

  int	     transport = vm->stackObjectValue(0);
  int	     socketAddress = addressForStackValue(1);
  flowSocket *socketPointer = (flowSocket *) socketAddress;
  int	     aSocket;
  int	     on = 1;


  if (!(vm->failed())) {
    if (vm->stackObjectValue(1) == vm->nilObject()) {
      vm->primitiveFail();
      return;}

    if (transport == vm->trueObject())
      transport = TCP;
    else if (transport == vm->falseObject())
      transport = UDP;
    else {
      vm->primitiveFail();
      return;}

    aSocket = socket(
		     AF_INET,
		     transport == TCP
		       ? SOCK_STREAM
		       : SOCK_DGRAM,
		     0);
    if (aSocket < 0) {
      vm->primitiveFail();
      return;}
    setsockopt(
	       aSocket,
	       SOL_SOCKET,
	       SO_REUSEADDR,
	       &on,
	       sizeof(on));

    socketPointer->state = flowOpen;
    socketPointer->transport = transport;
    socketPointer->resource.handle = aSocket;

    if (transport == TCP) {
      if (!startScribingThreads(
				&socketPointer->resource,
				waitForSendBufferSpace,
				waitForConnectionsAndReceivedData,
				(void *) socketAddress)) {
	vm->primitiveFail();
	return;}}
    vm->pop(2);}}


void connectSocketToAddress(void) {
  /*
   * connect: socketHandle
   * toAddress: targetAddress
   */

  struct sockaddr_in address;
  flowSocket	     *socketPointer = (flowSocket *) (addressForStackValue(1));
  int		     addressBytes = vm->stackObjectValue(0); /* a ByteArray */
  int		     result;


  if (!(vm->failed())) {
    memcpy(
	   &address.sin_addr.s_addr,
	   (unsigned char *) (addressBytes + BaseHeaderSize),
	   4);
    memcpy(
	   &address.sin_port,
	   (unsigned char *) (addressBytes + BaseHeaderSize + 4),
	   2);
    /*
     * SocketAddresses store their port bytes in
     * network order, there's no need to convert.
     */
    address.sin_family = AF_INET;

    if (socketPointer->transport == TCP) {
      /* Make the socket non-blocking for inline connection. */
      int nonblocking = TRUE;
      result = ioctl(
		     socketPointer->resource.handle,
		     FIONBIO,
		     &nonblocking);
      if (result == -1){
	vm->primitiveFail();
	return;}
      
      result = connect(
		       socketPointer->resource.handle,
		       (struct sockaddr *) &address,
		       sizeof address);

      switch (result) {
        case 0:
	  /* connected */
	  break;
        case -1:
	  if ((lastError() != EWOULDBLOCK) && (lastError() != EINPROGRESS)) {
	    vm->primitiveFail();
	    return;}
	  break;}}

    /* Each UDP socket object specifies an address for each write, from a cache. */
    else {
      vm->primitiveFail();
      return;}
    vm->pop(2);}}


void notifySocketWhenItMayPerformTimeoutAfter(void) {
  /*
   * notify: socketHandle
   * whenItMayPerform: operation
   * timeoutAfter: timeoutInMilliseconds
   */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(2));

  if (!(vm->failed())) {
    switch(vm->stackIntegerValue(1)) {
      case flowConnect:
      case flowAccept:
      case flowRead:
	/*
	 * signalSynchronizedResourceThread() pops the parameters from
	 * the object stack
	 */
	signalSynchronizedResourceThread(&socketPointer->resource.reading);
	break;

      case flowWrite:
	/*
	 * signalSynchronizedResourceThread() pops the parameters from
	 * the object stack
	 */
	signalSynchronizedResourceThread(&socketPointer->resource.writing);
	break;

      default:
	vm->primitiveFail();
	return;}}}


void bindSocketToPort(void) {
  /*
   * bindSocket: socketHandle
   * toPort: thePort
   */

  flowSocket	     *socketPointer = (flowSocket *) (addressForStackValue(1));
  int		     port = vm->stackIntegerValue(0); 
  struct sockaddr_in address;


  if (!(vm->failed())) {
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = ntohs((unsigned short) port);
    address.sin_family = AF_INET;

    if (bind(socketPointer->resource.handle, (struct sockaddr *) &address, sizeof address) < 0) {
      vm->primitiveFail();
      return;}

    vm->pop(2);}}


void acceptFrom(void) {
  /*
   * accept: clientHandle
   * from: serverHandle
   */

  flowSocket *serversocketPointer = (flowSocket *) (addressForStackValue(0));
  flowSocket *clientsocketPointer = (flowSocket *) (addressForStackValue(1));
  int	     result;


  if (!(vm->failed())) {
    if (serversocketPointer->state != flowListening) {
      vm->primitiveFail();
      return;}
    if (clientsocketPointer->state != flowOpen) {
      vm->primitiveFail();
      return;}

    result = accept(
		    serversocketPointer->resource.handle,
		    0,
		    0);
    if (result < 0) {
      vm->primitiveFail();
      return;}

    close(clientsocketPointer->resource.handle);
    clientsocketPointer->resource.handle = result;

    vm->pop(2);}}


void socketTimedOut(void) {
  /* timedOut: socketHandle */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(0));


  if (!(vm->failed())) {
    if (socketClosed(socketPointer))
      vm->popthenPush(
		      2,
		      vm->trueObject());
    else {
      vm->popthenPush(
		      2,
		      (socketPointer->resource.reading.result == timeout)
		        ? vm->trueObject()
		        : vm->falseObject());}}}


void tcpSocketConnectionRefused(void) {
  /* connectionRefused: socketHandle */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(0));

	
  if (!(vm->failed())) {
    vm->popthenPush(
		    2,
		    (socketPointer->resource.reading.result == failedConnection)
                      ? vm->trueObject()
		      : vm->falseObject());}}


void listenAtPortQueueSizeTCPSocket(void) {
  /*
   * listenAtPort: thePort
   * queueSize: theQueueSize
   * socket: socketHandle
   */

  flowSocket	     *socketPointer = (flowSocket *) (addressForStackValue(0));
  int		     queueSize = vm->stackIntegerValue(1);
  int		     port = vm->stackIntegerValue(2);
  int		     result;
  struct sockaddr_in address;


  if (!(vm->failed())) {
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = ntohs((unsigned short) port);
    address.sin_family = AF_INET;

    result = bind(
		  socketPointer->resource.handle,
		  (struct sockaddr *) &address,
		  sizeof address);
    if (result < 0) {
      vm->primitiveFail();
      return;}
    if (listen(socketPointer->resource.handle,queueSize) < 0) {
      vm->primitiveFail();
      return;}

    socketPointer->state = flowListening;

    vm->pop(3);}}


void peerAddressIntoNameIntoTCPSocket(void) {
  /*
   * peerAddressInto: aByteArray
   * nameInto: aString
   * socket: socketHandle
   */

  flowSocket      *socketPointer = (flowSocket *)(addressForStackValue(0));
  int		  peerName = vm->stackObjectValue(1); /* a String */
  int		  peerAddressObject = vm->stackObjectValue(2); /* a ByteArray */
  struct sockaddr peerAddress;
  int		  peerAddressLength = sizeof(peerAddress);
  unsigned short  port;
  struct hostent  *hostInfo;


  if (!(vm->failed())) {
    if (!(getpeername(        
		      socketPointer->resource.handle,
		      &peerAddress,
		      &peerAddressLength) == 0)) {
      vm->primitiveFail();
      return;}

    memcpy(
	   (unsigned char *) (peerAddressObject + BaseHeaderSize),
	   peerAddress.sa_data + 2,
	   4);
    memcpy(
	   &port,
	   peerAddress.sa_data,
	   2);
    port = ntohs(port);
    memcpy(
	   (unsigned char *) (peerAddressObject + BaseHeaderSize + 4),
	   &port,
	   2);
 
    hostInfo = gethostbyaddr(
			     (char *) &peerAddress + 4,
			     4,
			     AF_INET);
    if (hostInfo != 0)
      strncpy(
	      (char *) (peerName + BaseHeaderSize),
	      hostInfo->h_name,
	      strlen(hostInfo->h_name));
	
    vm->pop(3);}}


void dataAvailableForSocket(void) {
  /* dataAvailableFor: theHandle */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(0));
  fd_set     readingFileDescriptors;
  int	     nonblocking;
  timestamp  timeoutInMilliseconds;


  if (!(vm->failed())) {
    if (socketClosed(socketPointer)) {
      vm->popthenPush(
		      2,
		      vm->falseObject());
      return;}
    else {
      /* Perform a blocking select() with a zero-duration timeoutInMilliseconds. */
      nonblocking = FALSE;
      if (ioctl(socketPointer->resource.handle, FIONBIO, &nonblocking) == -1) {
	vm->primitiveFail();
	return;}
      timeoutInMilliseconds.seconds = 0;
      timeoutInMilliseconds.milliseconds = 0;

      FD_ZERO(&readingFileDescriptors);
      FD_SET(
	     socketPointer->resource.handle,
	     &readingFileDescriptors);

      if (
	  select(
		 socketPointer->resource.handle + 1,
		 &readingFileDescriptors,
		 0,
		 0,
		 (struct timeval *) &timeoutInMilliseconds) > 0)
	vm->popthenPush(
			2,
			vm->trueObject());
      else
	vm->popthenPush(
			2,
			vm->falseObject());}}}


/* Suck from the Great Teat in the Sky... */
void nextFromTCPSocketIntoStartingAt(void) {
  /*
   * next: bytesToReadInteger
   * from: tcpSocketHandle
   * into: targetByteArray
   * startingAt: targetStartIndex
   */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(2));
  int	     targetStartIndex = vm->stackIntegerValue(0);
  int	     targetBytes = vm->stackObjectValue(1);
  int	     bytesToRead = vm->stackIntegerValue(3);
  int	     result;


  if (!(vm->failed())) {
    if (!(vm->isWordsOrBytes(targetBytes))) {      
      vm->primitiveFail();
      return;}

    result = recv(
		  socketPointer->resource.handle,
		  (char *)(targetBytes + BaseHeaderSize + targetStartIndex - 1),
		  bytesToRead,
		  0);

    if (result == -1) {
      vm->primitiveFail();
      return;}

    vm->pop(5);
    vm->pushInteger(result);}}


/* Give something back to the universe... */
void nextPutFromToTCPSocketStartingAt(void) {
  /*
   * nextPut: bytesToWriteInteger
   * from: sourceByteArray
   * to: tcpSocketHandle
   * startingAt: sourceStartIndex
   */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(1));
  int	     sourceBytes = vm->stackObjectValue(2);
  int	     result = -1;
  int	     nonblocking = TRUE;


  if (!(vm->failed())) {
    /*
     * Hmm. isWordsOrBytes() isn't safe, apparently (saw it crash the VM when
     * sourceBytes was mistakenly set to a SmallInteger). This is probably not complete
     * coverage; might want to go back to the old class checks (String, ByteArray, etc.)
     */
    if ((sourceBytes == 0) || !(vm->isWordsOrBytes(sourceBytes))) {
      vm->primitiveFail();
      return;}

    /* Prepare the socket for a non-blocking send(). */
    result = ioctl(
		   socketPointer->resource.handle,
		   FIONBIO,
		   &nonblocking);
    if (result == -1) {
      result = lastError();
      vm->primitiveFail();
      return;}

    result = send(
		  socketPointer->resource.handle,
		  (char *) (sourceBytes + BaseHeaderSize + vm->stackIntegerValue(0) - 1),
		  vm->stackIntegerValue(3),
		  0);

    if ((result == -1) && (lastError() == EWOULDBLOCK)) {
      /*
       * We ought to have been able to send at least one
       * byte, because the Smalltalk process invoking this
       * primitive waited before doing so, on a semaphore
       * signalled by the waitForSendBufferSpace()
       * thread. Therefore, at least one byte of send-buffer
       * space should be available. Otherwise, select() does
       * not work correctly on this platform. Fail this
       * invocation; let Smalltalk deal with it. 
       */
      vm->primitiveFail();
      return;}

    socketPointer->resource.writing.result = convertedInteger(result);
    vm->pop(5);
    vm->pushInteger(result);}}


void tcpSocketIsActive(void) {
  /* isActive: socketHandle */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(0));


  if (!(vm->failed())) {
    if (socketClosed(socketPointer))
      vm->popthenPush(
		      2,
		      vm->falseObject());
    else
      vm->popthenPush(
		      2,
		      vm->trueObject());}}


void nextPacketFromUDPSocketInto(void) {
  /*
   * nextPacketFrom: udpSocketHandle
   * into: packetByteArray
   */

  flowSocket      *socketPointer = (flowSocket *) (addressForStackValue(1));
  int		  packet = vm->stackObjectValue(0);
  int		  result;
  struct sockaddr address;
  int		  addressSize = sizeof address;


  if (!(vm->failed())) {
    if (!(vm->isWordsOrBytes(packet))) {
      vm->primitiveFail();
      return;}

    result = recvfrom(
		      socketPointer->resource.handle,
		      (char *)(packet + BaseHeaderSize),
		      vm->byteSizeOf(packet),
		      0,
		      &address,
		      &addressSize);

    socketPointer->resource.reading.result = convertedInteger(result);
    if (socketPointer->resource.reading.result == timeout)
      result = result;
    if (result == -1) {
      result = lastError();
      vm->primitiveFail();
      return;}

    vm->pop(3);
    vm->pushInteger(result);}}


void nextPacketFromUDPSocketIntoAddressInto(void) {
  /*
   * nextPacketFrom: udpSocketHandle
   * into: packetByteArray
   * addressInto: addressBytes
   */

  flowSocket      *socketPointer = (flowSocket *) (addressForStackValue(2));
  int		  sourceAddress = vm->stackObjectValue(0);
  int		  packet = vm->stackObjectValue(1);
  int		  result;
  struct sockaddr address;
  int		  addressSize = sizeof address;
  unsigned short  port;


  if (!(vm->failed())) {
    if (!((vm->fetchClassOf(sourceAddress)) == (vm->classByteArray())) ||
	 (!(vm->isWordsOrBytes(packet)))) {
      vm->primitiveFail();
      return;}

    result = recvfrom(
		      socketPointer->resource.handle,
		      (char *) (packet + BaseHeaderSize),
		      vm->byteSizeOf(packet),
		      0,
		      &address,
		      &addressSize);

    socketPointer->resource.reading.result = convertedInteger(result);
    if (socketPointer->resource.reading.result == timeout)
      result = result;

    if (result == -1) {
      result = lastError();
      vm->primitiveFail();
      return;}

    memcpy(
	   (unsigned char *) (sourceAddress + BaseHeaderSize),
	   address.sa_data + 2,
	   4);
    memcpy(
	   &port,
	   address.sa_data,
	   2);
    port = ntohs(port);
    memcpy(
	   (unsigned char *) (sourceAddress + BaseHeaderSize + 4),
	   &port,
	   2);

    vm->pop(4);
    vm->pushInteger(result);}}


void sendPacketFromUDPSocketToAddress(void) {
  /*
   * sendPacket: packetByteArray
   * from: udpSocketHandle
   * toAddress: addressBytes
   */

  flowSocket         *socketPointer = (flowSocket *) (addressForStackValue(1));
  int		     addressBytes = vm->stackObjectValue(0);
  int		     packetBytes = vm->stackObjectValue(2);
  int		     result;
  struct sockaddr_in address;
  int		     addressSize = sizeof address;


  if (!(vm->failed())) {
    /* Validate addressBytes and packetBytes. */
    if (!((vm->fetchClassOf(addressBytes)) == (vm->classByteArray())) ||
	(!(vm->isWordsOrBytes(packetBytes)))) {
      vm->primitiveFail();
      return;}

    address.sin_addr.s_addr = (unsigned long) (*((unsigned long *) (addressBytes + BaseHeaderSize)));
    address.sin_port = ntohs((unsigned short) (*(unsigned short *) (addressBytes + BaseHeaderSize + 4)));
    address.sin_family = AF_INET;

    result = sendto(
		    socketPointer->resource.handle,
		    (char *) (packetBytes + BaseHeaderSize),
		    vm->byteSizeOf(packetBytes),
		    0,
		    (struct sockaddr *) &address,
		    addressSize);

    if (result == -1) result = lastError();
	  
    socketPointer->resource.writing.result = convertedInteger(result);
    vm->pop(4);
    vm->pushInteger(result);}}


void closeSocket(void) {
  /* close: socketHandle */

  flowSocket *socketPointer = (flowSocket *) (addressForStackValue(0));


  if (!(vm->failed())) {
    if (socketPointer->state == flowClosed) {
      vm->primitiveFail();
      return;}
    else {
      killThread(&socketPointer->resource.reading.sync);
      killThread(&socketPointer->resource.writing.sync);
      close(socketPointer->resource.handle);
      free((void *) socketPointer);
      socketPointer->state = flowClosed;
      vm->pop(1);}}}
