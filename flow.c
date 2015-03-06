
/*
 * flow: a Smalltalk external streaming framework
 * version 3
 *
 * flow.c - module initialization and utilities
 *
 * Craig Latta
 * netjam.org/flow
 */

#include "flow.h"

struct VirtualMachine *vm;


/*
 * initialization/finalization
 */

int setInterpreter(struct VirtualMachine *anInterpreter) {
  int ok;

  vm = anInterpreter;
#if 0
  ok = vm->majorVersion() == VM_PROXY_MAJOR;
  if (ok == 0) return 0;
  ok = vm->minorVersion() >= VM_PROXY_MINOR;
#endif
  return 1;}


const char* getModuleName(void) {
  return moduleName;}


int initialiseModule(void) {
#ifdef UNIXISH
  pthread_cond_t request = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  activity.request = request;
  activity.mutex = mutex;
#endif
  startIP();
  //	startMIDI();
  return TRUE;}


#ifdef WIN32
/* See ViaVoice comment in speech.c. */
#ifdef VIAVOICE
/*
 * needsWork: This function is only called on WIN32. I don't know if
 * an equivalent function is necessary on any other platform. I haven't
 * put a lot of thought into the design of this facility. Currently
 * there is still support for just one window message hook. Should there
 * be support for several hooks? If so, does their execution order matter,
 * and, if so, how is it determined? Etc.
 */
int getWindowMessageHook(HWND *windowHandle) {
  squeakWindow = *windowHandle;


  return (int)&handleSpeechRecognitionEvent;}
#endif
#endif


int shutdownModule(void) {
  stopIP();
  //	stopMIDI();
  return TRUE;}


int moduleUnloaded(char *unloadedModuleName) {
  /*
   * There are currently no dependencies between Flow and other
   * modules, as far as I know...
   */

  return TRUE;}


/*
 * utilities
 */

char *copyStringAt(int stackIndex) {
  int  string = vm->stackObjectValue(stackIndex);
  int  stringLength;
  char *stringCopy;


  if (!((vm->fetchClassOf(string)) == (vm->classString())
	|| (vm->fetchClassOf(string)) == (vm->classByteArray()))) {
    vm->primitiveFail();
    return NULL;}

  stringLength = vm->byteSizeOf(string);

  /*
   * needsWork: make sure all callers of copyStringAt() are freeing
   * properly.
   */
  stringCopy = (char *) malloc(stringLength + 1);

  strncpy(
	  stringCopy,
	  (char*)string + BaseHeaderSize,
	  stringLength);
  stringCopy[stringLength] = '\0';

  return stringCopy;}


void startIP(void) {
#ifdef WIN32
  WSADATA wsadata;
  WORD	  wVersionRequired = 0x0002;

  if (WSAStartup(wVersionRequired, &wsadata) != 0)
    vm->primitiveFail();
#endif
}

#ifdef WIN32
#ifndef _WIN32_WCE
void announceAvailableMIDIInput(void *ignored) {
  int	   index;
  midiPort *port;


  while (1) {
    WaitForSingleObject(
			midiInputAvailable,
			INFINITE);
    for (index = 0; index < MAXMIDIPORTS; index++) {
      port = flowMidiPorts[index];
      if (port && (port->incomingShortMessagesStartIndex != port->incomingShortMessagesStopIndex))
	synchronizedSignalSemaphoreWithIndex(port->shortMessageAvailability);}}}


void announceAvailableSysEx(void *ignored) {
  int	   index;
  midiPort *port;


  while (1) {
    WaitForSingleObject(sysExAvailable, INFINITE);
    for (index = 0; index < MAXMIDIPORTS; index++) {
      port = flowMidiPorts[index];
      if (port && (port->incomingSystemExclusiveMessagesStartIndex != port->incomingSystemExclusiveMessagesStopIndex))
	synchronizedSignalSemaphoreWithIndex(port->systemExclusiveMessageAvailability);}}}
#endif
#endif

#if 0
void startMIDI(void) {
#ifdef WIN32
#ifndef _WIN32_WCE
  DWORD threadID;


  midiInputAvailable = (
			CreateEvent(
				    NULL,
				    0,
				    0,
				    NULL));

  availableMIDIInputAnnouncement = (
				    CreateThread(
						 NULL,
						 0,
						 (LPTHREAD_START_ROUTINE) &announceAvailableMIDIInput,
						 NULL,
						 0,
						 &threadID));

  sysExAvailable = (
		    CreateEvent(
				NULL,
				0,
				0,
				NULL));

  availableSysExAnnouncement = (
				CreateThread(
					     NULL,
					     0,
					     (LPTHREAD_START_ROUTINE) &announceAvailableSysEx,
					     NULL,
					     0,
					     &threadID));
#endif
#else
  Pm_Initialize();
  Pt_Start(
	   1,
	   NULL,
	   NULL);
#endif
}

void stopMIDI(void) {
#ifdef WIN32
#ifndef _WIN32_WCE
  int portIndex;


  /* Close all open ports. */
  for (portIndex = 0; portIndex < MAXMIDIPORTS; portIndex++) closeMIDIPort(portIndex);
  TerminateThread(availableMIDIInputAnnouncement, 0);
  CloseHandle(midiInputAvailable);
  TerminateThread(availableSysExAnnouncement, 0);
  CloseHandle(sysExAvailable);
#endif
#else
  Pm_Terminate();
#endif
}
#endif

int addressForStackValue(int index) {
  return *((int *) (vm->stackObjectValue(index) + BaseHeaderSize));}


void writeNewResourceHandle(int size) {
  int handleOop = vm->stackObjectValue(0);
  int address;


  address = (int) calloc(
			 1,
			 size);
  memcpy(
	 (void *) (handleOop + BaseHeaderSize),
	 (const void *) &address,
	 4);
  vm->pop(1);}


int startThread(
		threadSync *sync,
		void *function,
		void *parameter) {
#ifdef UNIXISH
  pthread_t	  thread;
  pthread_cond_t  request = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
#ifdef WIN32
  DWORD		  dwThreadID;
  HANDLE	  hThread;
#endif

#ifdef UNIXISH
  sync->requestAlreadyOccurred = FALSE;
  sync->thread = thread;
  sync->request = request;
  sync->mutex = mutex;

  /*
   * Apparently, in POSIX, one may meaningfully set the priority
   * only of threads with superuser privileges.
   */
  pthread_create(
		 &sync->thread,
		 NULL,
		 function,
		 parameter);
#endif
#ifdef WIN32
  sync->pendingEvent = 
    CreateEvent(
		NULL,	/* no security */
		FALSE,	/* can implicitly reset */
		FALSE,	/* initial pendingEvent reset */
		NULL);	/* no name */

  if (NULL == (hThread =
		CreateThread(
			     (LPSECURITY_ATTRIBUTES) NULL,
			     0,
			     (LPTHREAD_START_ROUTINE) function,
			     (LPVOID) parameter,
			     0,
			     &dwThreadID))) {
    return FALSE;}

  SetThreadPriority(
		    hThread,
		    THREAD_PRIORITY_HIGHEST);
  sync->thread = hThread;
#endif
  return TRUE;}


int startScribingThreads(
			 netResource	*resource,
			 void		*writingFunction,
			 void		*readingFunction,
			 void		*parameter) {

  if (!startThread(
		   &resource->writing.sync,
		   writingFunction,
		   parameter))
    return FALSE;
  else {
    if (!startThread(
		     &resource->reading.sync,
		     readingFunction,
		     parameter))
      return FALSE;
    else return TRUE;}}


void waitForThreadSignal(threadSync *sync) {
#ifdef UNIXISH
  pthread_mutex_lock(&sync->mutex);
  while(!sync->requestAlreadyOccurred) {
    pthread_cond_wait(&sync->request, &sync->mutex);}
  sync->requestAlreadyOccurred = FALSE;
  pthread_mutex_unlock(&sync->mutex);
#endif
#ifdef WIN32
  WaitForSingleObject(sync->pendingEvent, INFINITE);
#endif
}


void signalThread(threadSync *sync) {
#ifdef UNIXISH
  pthread_mutex_lock(&sync->mutex);
  sync->requestAlreadyOccurred = TRUE;
  pthread_cond_signal(&sync->request);
  pthread_mutex_unlock(&sync->mutex);
#endif
#ifdef WIN32
  SetEvent(sync->pendingEvent);
#endif
}


void signalSynchronizedResourceThread(thread *thread) {
  thread->timeout = vm->stackValue(0);
  if (thread->timeout == vm->nilObject())
    thread->timeout = -1;
  else if (thread->timeout < 0) thread->timeout = 0;
  thread->operation = vm->stackIntegerValue(1);
  signalThread(&thread->sync);
  vm->pop(3);}


void stopThread(threadSync *sync) {
  signalThread(sync);

  /* Wait until the thread has been halted. */
#ifdef UNIXISH
  pthread_join(sync->thread, NULL);
#endif
#ifdef WIN32
  WaitForSingleObject(sync->thread, INFINITE);
  CloseHandle(sync->thread);
  CloseHandle(sync->pendingEvent);
  sync->pendingEvent = NULL;
#endif
}


void killThread(threadSync *sync) {
#ifdef UNIXISH
  pthread_cancel(sync->thread);
#endif
#ifdef WIN32
  TerminateThread(sync->thread, 0);
  CloseHandle(sync->thread);
  CloseHandle(sync->pendingEvent);
  sync->pendingEvent = NULL;
#endif
}


void synchronizedSignalSemaphoreWithIndex(int index) {
  vm->signalSemaphoreWithIndex(index);
  signalThread(&activity);}


void stopIP(void) {
#ifdef WIN32
  WSACleanup();
#endif
}


/* primitives */

/* just a simple function for testing that the library is loaded */
void greetings(void) {
  /* Pop the receiver, then push the true object. */
  vm->pop(1);
  vm->pushInteger(43);}


void associateNetResourceWithReadabilityIndexAndWritabilityIndex(void) {
  /*
   * associate: netResourceHandle
   * withReadabilityIndex: readabilityIndex
   * andWritabilityIndex: writabilityIndex
   */


  netResource		*netResourcePointer = (netResource *)(addressForStackValue(2));


  if (vm->stackObjectValue(2) == vm->nilObject()) {
    vm->primitiveFail();
    return;}

  netResourcePointer->reading.sync.semaphore = vm->stackIntegerValue(1);
  netResourcePointer->writing.sync.semaphore = vm->stackIntegerValue(0);

  vm->pop(3);}


void associateWithReadabilityIndexAndWritabilityIndex(void) {
  associateNetResourceWithReadabilityIndexAndWritabilityIndex();}


void methodDictionaryIsMarked(void) {
  /* methodDictionaryIsMarked: aMethodDictionary */

  int methodDictionary = vm->stackObjectValue(0);


  vm->popthenPush(
		  2,
		  (((longAt(methodDictionary) >> 8) & 15) == 5)
		    ? vm->trueObject()
		    : vm->falseObject());}


void firstEmptyBehaviorFor(void) {
  /* firstEmptyBehaviorFor: anObject */

  int anObject = vm->stackObjectValue(0);
  int currentClass = vm->fetchClassOf(anObject);
  int nilObject = vm->nilObject();
  int dictionary;


  while (currentClass != nilObject) {
    dictionary = longAt(((((char *) currentClass)) + 4) + (1 << 2));
    if (dictionary == nilObject) {
      vm->popthenPush(2, currentClass);
      return;}
    currentClass = longAt(((((char *) currentClass)) + 4) + (0 << 2));}

  vm->popthenPush(2, nilObject);}


void clearMarkOnBehavior(void) {
  /* clearMarkOnBehavior: aBehavior */

  int aBehavior = vm->stackObjectValue(0);
  int dictionary = longAt(((((char *) aBehavior)) + 4) + (1 << 2));


  longAtput(dictionary, (longAt(dictionary)) & 4294963455U);
  longAtput(dictionary, (longAt(dictionary)) | 768);
  vm->pop(1);}


void compiledMethodIsMarked(void) {
  /* compiledMethodIsMarked: aCompiledMethod */

  int compiledMethod = vm->stackObjectValue(0);
  int flagByte, idByte;


  flagByte = byteAt(((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 1);
  if (flagByte < 252) {
    idByte = byteAt((((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 3) - flagByte);
  } else {
    idByte = byteAt((((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 1) - 5);}

  vm->popthenPush(
		  2,
		  (idByte >= 128) ? vm->trueObject() : vm->falseObject());}


void clearMarkOnCompiledMethod(void) {
  /* clearMarkOnCompiledMethod: aCompiledMethod */

  int compiledMethod = vm->stackObjectValue(0);
  int flagByte, oldIDByte;


  flagByte = byteAt(((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 1);
  if (flagByte < 252) {
    oldIDByte = byteAt((((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 3) - flagByte);
    byteAtput(
	      (((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 3) - flagByte,
	      oldIDByte & 127);
  } else {
    oldIDByte = byteAt((((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 1) - 5);
    byteAtput(
	      (((compiledMethod + 4) + (vm->byteSizeOf(compiledMethod))) - 1) - 5,
	      oldIDByte & 127);}

  vm->pop(1);}


void forkMemoryUsingProcessor(void) {
  /* forkMemory: memoryPath usingProcessor: processorPath */
  /* fork and exec the local history memory */
  
  int memoryPath = copyStringAt(1);
  int processorPath = copyStringAt(0);

#ifdef UNIXISH
  pid_t fork_pid, exec_pid;
  fork_pid = vfork();
  if (fork_pid == 0) {
    exec_pid = execl(
		     (const char *)processorPath,
		     (const char *)processorPath,
		     (const char *)memoryPath,
		     (char *)0);}
#else
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  CreateProcess(
		processorPath,
		memoryPath,
		NULL,
		NULL,
		TRUE,
		0,
		NULL,
		NULL,
		&si,
		&pi);
#endif
}
  

void relinquishPhysicalProcessor(void) {
  /* relinquishPhysicalProcessor */

  /* the next delay wakeup time */
  int	          nextWakeupTick = vm->getNextWakeupTick();
  int	          now = (vm->ioMicroMSecs() & 0x1fffffff);
  int	          timeoutInMilliseconds;
#ifdef UNIXISH
  struct timeval  currentTime;
  struct timespec wakeupTime;
#endif

  if (nextWakeupTick <= now) {
    if (nextWakeupTick == 0)
      /*
       * Timer interrupts have been disabled; wait
       * for as long as possible before anyone might
       * notice. Specifically, wait for one frame
       * at a typical GUI frame rate (60 Hz).
       */
      timeoutInMilliseconds = (1000/60);
    else
      /*
       * The next delay wakeup time has passed and
       * the system should resume as soon as
       * possible.
       */
      timeoutInMilliseconds = 0;}
  else
    timeoutInMilliseconds = nextWakeupTick - now;

#ifdef UNIXISH
#if 1
  pthread_mutex_lock(&activity.mutex);
  gettimeofday(&currentTime, NULL);
  wakeupTime.tv_nsec = (currentTime.tv_usec * 1000) + (timeoutInMilliseconds * 1000000);
  wakeupTime.tv_sec = currentTime.tv_sec + (wakeupTime.tv_nsec / 1000000000);
  wakeupTime.tv_nsec %= 1000000000;

  pthread_cond_timedwait(
			 &activity.request,
			 &activity.mutex,
			 &wakeupTime);

  pthread_mutex_unlock(&activity.mutex);
#endif
#if 0
  /*
   * an experiment to see how much CPU Squeak uses during idle
   * when it sleeps via select()
   */
  struct timeval timeout = {0, timeoutInMilliseconds};
  struct fd_set reads, writes, exceptions;

  FD_ZERO(&reads);
  FD_ZERO(&writes);
  FD_ZERO(&exceptions);

  select(0, &reads, &writes, &exceptions, &timeout);
#endif
#if 0
  /*
   * an experiment to see how much CPU Squeak uses during idle
   * when it sleeps via nanosleep()
   */
  struct timespec rqtp = {0, timeoutInMilliseconds * 1000 * 1000};
  struct timespec rmtp;
  while ((nanosleep(&rqtp, &rmtp) < 0) && (errno == EINTR))
    rqtp = rmtp;
#endif
#endif

  vm->setInterruptCheckCounter(0);}
