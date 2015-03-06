
/*
 * flow: a Smalltalk streaming framework
 * version 3
 *
 * flow.h - global information
 *
 * Craig Latta
 * netjam.org/flow
 */

/*
 * Apparently, IBM no longer supports Linux ViaVoice development,
 * and there is now a minimum ViaVoice licensing fee of USD 18 000.
 * 
 * So much for ViaVoice in Flow! :)
 *
 * This does bring up an interesting architectural point, though.
 * If, conceptually, the Flow module wants to use another module,
 * should it be loaded as separate parts?
 */

/* included headers */

#ifdef _WIN32_WCE
typedef unsigned int size_t;
/* default EXPORT macro that does nothing (see comment in sq.h) */
#define EXPORT(returnType) returnType
#include "winsock.h"
#endif

#ifdef WIN32
#include "winsock.h"
#ifndef _WIN32_WCE
#define MAXMIDIPORTS 8
#endif
#endif

/* Only use necessary excerpts from sq.h. */
/* the virtual machine proxy definition */
#include "sqVirtualMachine.h"
/* configuration options */
#include "sqConfig.h"
/* platform-specific definitions */
#include "sqPlatformSpecific.h"

#ifdef VIAVOICE
#include <eci.h>
#include <smapi.h>
#endif

#if ((defined UNIX) || (defined macintoshSqueak))
#define UNIXISH
#endif

#ifdef UNIXISH
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

// #include <phidget21.h>
// #include "portmidi.h"


/*
 * definitions
 */

#define bitIsSet(field, mask)       ((field & mask) == mask)
#define maximumAddedTextLength      500
#ifdef _WINSOCKAPI_
#define hs2net		            ntohs
#endif

/*
 * interpreter-related definitions (which ought to be generated
 * by the interpreter, and not defined here).
 */
#define BaseHeaderSize		    4
#define	ClassString		    6
#define	ClassByteArray		    26
#define	ClassSemaphore		    18
#define byteAt(memoryLocation)	    (*((unsigned char *) (memoryLocation)))
#define byteAtput( \
		  memoryLocation, \
		  value)            (*((unsigned char *) (memoryLocation)) = value)
#define longAt(memoryLocation)      (*((int *) (memoryLocation)))
#define longAtput( \
		  memoryLocation, \
		  value)            (*((int *) (memoryLocation)) = value)
#define successFlag                 (!vm->failed())
#define null                        0

#define	convertedInteger(x)	    (x)

#ifdef UNIXISH
#define lastError()		    errno
#define TRUE			    1
#define FALSE			    0
#endif

/* sockets */

#ifdef WIN32
#define lastError()		    WSAGetLastError()
#define ECONNRESET		    WSAECONNRESET
#define EWOULDBLOCK		    WSAEWOULDBLOCK
#define EINPROGRESS		    WSAEINPROGRESS
#define ioctl( \
	      socket, \
	      command, \
	      parameter)	    ioctlsocket(socket, command, parameter)
#define close(socket)		    closesocket(socket)
#endif

/* speech */

#define SPEECH_SESSION_ID	    42

/* MIDI */

#define MIDIMessagesSize	    1024
#define SysExMessagesSize	    512
#define SysExMessageBytesSize	    4096
#define INCOMING		    1
#define OUTGOING		    2
#define SendingSysEx		    4
#define NoteOn			    0x90
#define NoteOff			    0x80
#define ProgramChange		    0xC0
#define ControlChange		    0xB0
#define PolyphonicPressure	    0xA0
#define ChannelPressure		    0xD0
#define PitchWheel		    0xE0
#define System			    0xF0


/*
 * simple types
 */

#ifdef WIN32
typedef HANDLE handle;
#endif
#ifdef UNIXISH
typedef void *handle;
#endif


/*
 * enumerations
 */

/* socket types */
enum {
  TCP = 1001,
  UDP};

/* resource operations */
enum {
  flowConnect = 2001,
  flowAccept,
  flowRead,
  flowWrite};

/* resource operation results */
enum {
  timeout = 3001,
  ready,
  error,
  successfulConnection,
  failedConnection};

/* resource states */
enum {
  flowClosed = 4001,
  flowOpen,
  flowListening};

/* file connection policies */
enum {
  mustBePresent = 5001,
  noClobber,
  clobber};


/*
 * structures
 */

typedef struct {
  long seconds, milliseconds;
}      timestamp;

typedef struct {
  /* semaphore is an external object index. */
  int             semaphore;
#ifdef UNIXISH
  pthread_t	  thread;
  int		  requestAlreadyOccurred;
  pthread_cond_t  request;
  pthread_mutex_t mutex;
#endif
#ifdef WIN32
  handle	  thread, pendingEvent;
#endif
}                 threadSync;

typedef struct {
  int		 state, result;
  char		 addressBytes[4], *hostname;
  struct in_addr address;
  threadSync	 sync;
}                resolver;

typedef struct {
  int	     operation, timeout, result;
  threadSync sync;
}	     thread;
	
typedef struct {
  int	     handle;
  thread     reading, writing;
}	     netResource;

typedef struct {
  netResource resource;
  int	      state, transport;
}	      flowSocket;


/* phidgets */

typedef struct {
  int handle;
  int digitalInputChangeSemaphoreIndex;
  int digitalOutputChangeSemaphoreIndex;
  int analogInputChangeSemaphoreIndex;
  int changedDigitalInputIndex;
  int changedDigitalOutputIndex;
  int changedAnalogInputIndex;
  int newDigitalInputValue;
  int newDigitalOutputValue;
  int newAnalogInputValue;
}     phidgetInterfaceKit;


/* MIDI */

#ifdef WIN32
#ifndef _WIN32_WCE
typedef struct {
  DWORD data;
  DWORD	timestamp;
}	midiEvent;

typedef struct {
  unsigned char	*data;
  DWORD		timestamp;
  int		length;
}		sysExEvent;

typedef struct {
  unsigned char	controllerValues[128];
  unsigned char	keyPressures[128];
  unsigned char	channelPressures[16];
  int	        pitchBendValues[16];
}		controllerCache;

typedef struct {
  /* handles and stats */
  HMIDIIN	  inputHandle;
  HMIDIOUT	  outputHandle;
  int		  outputPortIndex;
  DWORD		  stats;
  HANDLE	  outputProgress;

  /* outgoing short messages */
  unsigned char	  pendingOutgoingMessage[4];
  DWORD		  currentPendingOutgoingMessageSize;
  DWORD		  remainingPendingOutgoingMessageSize;
  DWORD		  pendingOutgoingMessageTimestamp;
  HANDLE	  outgoingShortMessagesAccess;
  HANDLE	  outgoingShortMessageAdded;
  HANDLE	  outgoingShortMessagesScheduler;
  midiEvent	  *outgoingShortMessages;
  DWORD		  numberOfOutgoingShortMessages;

  /* outgoing sysex messages */
  unsigned char	  *outgoingSystemExclusiveMessageBytes;
  DWORD		  outgoingSystemExclusiveMessageLength;

  /* incoming short messages */
  midiEvent	  *pendingIncomingShortMessage;
  int		  shortMessageAvailability;
  midiEvent	  *incomingShortMessages;
  DWORD		  incomingShortMessagesStartIndex;
  DWORD		  incomingShortMessagesStopIndex;

  /*
   * buffer space used by the lower-level driver for
   * incoming sysex messages
   */
  MIDIHDR	  *incomingSystemExclusiveMessage;
  unsigned char	  *incomingSystemExclusiveMessageBytes;

  /* incoming sysex messages */
  sysExEvent	  *pendingIncomingSystemExclusiveMessage;
  int		  systemExclusiveMessageAvailability;
  sysExEvent	  *incomingSystemExclusiveMessages;
  DWORD		  incomingSystemExclusiveMessagesStartIndex;
  DWORD		  incomingSystemExclusiveMessagesStopIndex;

  /* controller value cache */
  BOOL		  cachingControllerValues;
  controllerCache controllerCache;
}		  midiPort;

midiPort *(flowMidiPorts[MAXMIDIPORTS]);

#endif
#else
#if 0
typedef struct {
  PortMidiStream *inputStream;
  PortMidiStream *outputStream;
  int		 shortMessageAvailability;
  int		 systemExclusiveMessageAvailability;	
}		 midiPort;
#endif
#endif


/* displays */

#if 0
/*
 * for Mac displays, reorganize this
 */
 
 /* window handle type */
#define wHandleType WindowPtr
#define wIndexType int 

typedef struct windowDescriptorBlock {
	struct windowDescriptorBlock * next;
	wHandleType		handle;
	wIndexType		windowIndex;
	/* extra fields to support your platform needs */
	CGContextRef context;
	int rememberTicker;
	int dirty;
	int sync;
	int locked;
	int	width;
	int	height;
	int isInvisible;
} windowDescriptorBlock;

static windowDescriptorBlock *windowListRoot = NULL;
#endif

/* speech */
/* See ViaVoice comment above. */
#ifdef VIAVOICE
typedef struct {
  ECIHand    synthesizerHandle;
  int	     synthesizer, recognizer;
  char	     phonemeBuffer[128];
  threadSync sync;
}	     speechRelay;
#endif

typedef struct {
#ifdef WIN32
  HANDLE handle;
#endif
#ifdef UNIXISH
  FILE	 *handle;
#endif
}	 file;

typedef struct {
  int  size;
  char bytes[300000];
}      snapshot;


/*
 * global variables
 */

static const char       *moduleName = "Flow";
static threadSync       activity;

#ifdef WIN32
#ifndef _WIN32_WCE
HANDLE			midiInputAvailable;
HANDLE			availableMIDIInputAnnouncement;
HANDLE			sysExAvailable;
HANDLE			availableSysExAnnouncement;
MIDIHDR			*midiHeaders;
static int		metaMessageLengths[16] = {
                        -1, /* 0xF0: system-exclusive, read through next status byte */
			2,  /* 0xF1: MTC quarter-frame, 2 bytes */
			3,  /* 0xF2: song position pointer, 3 bytes */
			2,  /* 0xF3: song select, 2 bytes */
			0,  /* 0xF4: unassigned, read up to next status byte */
			0,  /* 0xF5: unassigned, read up to next status byte */
			1,  /* 0xF6: tune request, 1 byte */
			1,  /* 0xF7: end of system-exclusive, shouldn't happen */
			1,  /* 0xF8: MIDI clock, 1 byte */
			0,  /* 0xF9: unassigned, read up to next status byte */
			1,  /* 0xFA: MIDI start, 1 byte */
			1,  /* 0xFB: MIDI continue, 1 byte */
			1,  /* 0xFC: MIDI stop, 1 byte */
			0,  /* 0xFD: unassigned, read up to next status byte */
			1,  /* 0xFE: active sense, 1 byte */
			1}; /* 0xFF: reset, 1 byte */
#endif
#ifdef VIAVOICE
HWND		        squeakWindow;
#endif
#endif


/*
 * function prototypes
 */

#ifdef WIN32
#ifndef	_WINSOCKAPI_
void	           gethostname(char *outName, int maxLen);
#endif
#endif

/* for internal use */
char	           *copyStringAt(int stackIndex);
void	           startIP(void);
void	           writeNewResourceHandle(int size);
int	           addressForStackValue(int index);
int	           startThread(
			       threadSync *sync,
			       void *function,
			       void *parameter);
int	           startScribingThreads(
					netResource *resource,
					void *writingFunction,
					void *readingFunction,
					void *parameter);
void	           waitForThreadSignal(threadSync *sync);
void	           signalThread(threadSync *sync);
void	           signalSynchronizedResourceThread(thread *thread);
void	           stopThread(threadSync *sync);
void	           killThread(threadSync *sync);
void	           stopIP(void);
void	           startMIDI(void);
void	           stopMIDI(void);
void	           closeMIDIPort(int portIndex);

/* for external use */
#ifdef WIN32
#pragma export on
#endif

/* from flow.c */
EXPORT(int)	   setInterpreter(struct VirtualMachine *anInterpreter);
EXPORT(const char) *getModuleName(void);
EXPORT(int)	   initialiseModule(void);
EXPORT(int)	   shutdownModule(void);
EXPORT(int)	   moduleUnloaded(char *unloadedModuleName);
EXPORT(void)	   associateNetResourceWithReadabilityIndexAndWritabilityIndex(void);
EXPORT(void)	   greetings(void);
EXPORT(void)	   methodDictionaryIsMarked(void);
EXPORT(void)	   firstEmptyBehaviorFor(void);
EXPORT(void)	   clearMarkOnBehavior(void);
EXPORT(void)	   compiledMethodIsMarked(void);
EXPORT(void)	   clearMarkOnCompiledMethod(void);
EXPORT(void)	   relinquishPhysicalProcessor(void);

/* from ip.c */
EXPORT(void)	   newResolverHandleInto(void);
EXPORT(void)	   enableResolver(void);
EXPORT(void)	   registerThatResolverHasResolutionIndex(void);
EXPORT(void)	   notifyAfterResolvingHostNamed(void);
EXPORT(void)	   writeAddressBytesForResolverInto(void);
EXPORT(void)	   nameForIPAddressInto(void);
EXPORT(void)	   closeResolver(void);
EXPORT(void)	   newSocketHandleInto(void);
EXPORT(void)	   connectSocketToAddress(void);
EXPORT(void)	   notifySocketWhenItMayPerformTimeoutAfter(void);
EXPORT(void)	   bindSocketToPort(void);
EXPORT(void)	   acceptFrom(void);
EXPORT(void)	   enableSocketUsingTCP(void);
EXPORT(void)	   socketTimedOut(void);
EXPORT(void)	   tcpSocketConnectionRefused(void);
EXPORT(void)	   listenAtPortQueueSizeTCPSocket(void);
EXPORT(void)	   peerAddressIntoNameIntoTCPSocket(void);
EXPORT(void)	   dataAvailableForSocket(void);
EXPORT(void)	   nextFromTCPSocketIntoStartingAt(void);
EXPORT(void)	   nextPutFromToTCPSocketStartingAt(void);
EXPORT(void)	   tcpSocketIsActive(void);
EXPORT(void)	   nextPacketFromUDPSocketInto(void);
EXPORT(void)	   nextPacketFromUDPSocketIntoAddressInto(void);
EXPORT(void)	   sendPacketFromUDPSocketToAddress(void);
EXPORT(void)	   closeSocket(void);

/* See ViaVoice comment above. */
/* from speech.c */
#ifdef VIAVOICE
EXPORT(int)	   getWindowMessageHook(HWND *windowHandle);
EXPORT(int)	   handleSpeechRecognitionEvent(
						HWND hwnd,
						UINT message,
						WPARAM wParam,
						LPARAM lParam);
EXPORT(void)	   newSpeechRelayHandleInto(void);
EXPORT(void)	   enableSpeechRelayWithRecognitionSocketSynthesisSocketAndSynthesisCompletionIndex(void);
EXPORT(void)	   nextPutFromToSpeechRelayStartingAt(void);
EXPORT(void)	   commitSpeechRelay(void);
EXPORT(void)	   prepareForPhonemeGeneration(void);
EXPORT(void)	   generatePhonemesFor(void);
EXPORT(void)	   prepareForSynthesis(void);
EXPORT(void)	   afterSynthesisNotify(void);
EXPORT(void)	   closeSpeechRelay(void);
#endif

/* from filesystem.c */
EXPORT(void)	   newFileHandleInto(void);
EXPORT(void)	   enableNamedWithConnectionPolicy(void);
EXPORT(void)	   size(void);
EXPORT(void)	   delete(void);
EXPORT(void)	   readFromStartingAtInto(void);
EXPORT(void)	   writeToStartingAtFrom(void);
EXPORT(void)	   closeFile(void);

/* from midi.c */
EXPORT(void)	   numberOfMIDIPorts(void);
EXPORT(void)	   nameOfMIDIPortAt(void);
EXPORT(void)	   newMIDIPortHandleInto(void);
EXPORT(void)	   enableMIDIPortAtAnd(void);
EXPORT(void)	   scheduleMIDIMessagesInQuantityOn(void);
EXPORT(void)	   midiPortDataAvailable(void);
EXPORT(void)	   nextAvailableFromMIDIPortInto(void);
EXPORT(void)	   MIDIClockValue(void);
EXPORT(void)	   closeMIDIPortWithHandle(void);

/* from audio.c */
EXPORT(void)	   streamingSampledSoundmixSampleCountfromintostartingAtleftVolrightVol(void);

/* from phidgets.c */
EXPORT(void)	   enablePhidgetInterfaceKitHandleWithDigitalInputChangeSemaphoreIndexDigitalOutputChangeSemaphoreIndexAndAnalogInputChangeSemaphoreIndex(void);
EXPORT(void)	   newPhidgetInterfaceKitHandleInto(void);
EXPORT(void)	   lastChangedAnalogInputAndValueForPhidget(void);
EXPORT(void)	   lastChangedDigitalInputAndValueForPhidget(void);
EXPORT(void)	   lastChangedDigitalOutputAndValueForPhidget(void);
EXPORT(void)	   setDigitalOutputOnInterfaceTo(void);
EXPORT(void)	   closePhidget(void);

#ifdef WIN32
#pragma export off
#endif
