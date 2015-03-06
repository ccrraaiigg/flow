
/*
 * flow: a Smalltalk streaming framework
 * version 3
 *
 * midi.c - MIDI primtives
 *
 * Craig Latta
 * netjam.org/flow
 */

#include "flow.h"
extern struct VirtualMachine *vm;


/*
 * primitives
 */

void numberOfMIDIPorts(void) {
  /* numberOfPorts */

  int numberOfPorts;

  vm->pop(1);
  vm->pushInteger(Pm_CountDevices());}


void nameOfMIDIPortAt(void) {
  /* nameOfPortAt: portIndex */

  int	portIndex = vm->stackIntegerValue(0);
  char	*portName, *portNameObjectContentsPointer;
  int	portNameObject,
        portNameSize,
        numberOfInputPorts,
        numberOfOutputPorts;

  const PmDeviceInfo *deviceInfo = Pm_GetDeviceInfo(portIndex);
  
  portName = deviceInfo->name;
  portNameSize = strlen(portName);
  portNameObject = (
		    vm->instantiateClassindexableSize(
						      vm->classString(),
						      portNameSize));
  if (vm->failed()) {
    vm->primitiveFail();
    return;}

  portNameObjectContentsPointer = ((char *) vm->firstIndexableField(portNameObject));
  if (vm->failed()) {
    vm->primitiveFail();
    return;}
  memcpy(
	 portNameObjectContentsPointer,
	 portName,
	 portNameSize);
  vm->popthenPush(
		  2,
		  portNameObject);}


void newMIDIPortHandleInto(void) {
	/* newResourceHandleInto: theHandle */

	writeNewResourceHandle(sizeof(midiPort));}


void associateMIDIPortWithShortMessageReadabilityIndexAndSystemExclusiveMessageReadabilityIndex(void) {
  /*
   * associate: midiPortHandle
   * withShortMessageReadabilityIndex: shortMessageReadabilityIndex
   * andSystemExclusiveMessageReadabilityIndex: systemExclusiveMessageReadabilityIndex
   */

  midiPort *port = (midiPort *) (addressForStackValue(2));

  port->shortMessageAvailability = vm->stackIntegerValue(1);
  port->systemExclusiveMessageAvailability = vm->stackIntegerValue(0);}


void enableMIDIPortAtAnd(void) {
  /*
   * enable: midiPortHandle
   * at: outputPortIndex
   * and: inputPortIndex
   */

  int		 outputPortIndex = vm->stackIntegerValue(1);
  int		 inputPortIndex = vm->stackIntegerValue(0);
  midiPort	 *port = (midiPort *) (addressForStackValue(2));
  PortMidiStream *outputStream, *inputStream;
  unsigned int	 outputResult, inputResult;

  outputResult = Pm_OpenOutput(
			       &outputStream,
			       outputPortIndex,
			       NULL,
			       0,
			       NULL,
			       NULL,
			       1);

  if (outputResult != pmNoError) {
    vm->primitiveFail();
    return;}

  port->outputStream = outputStream;

  inputResult = Pm_OpenInput(
			     &inputStream,
			     inputPortIndex,
			     NULL,
			     1024,
			     NULL,
			     NULL);

  if (inputResult != pmNoError) {
    vm->primitiveFail();
    return;}

  port->inputStream = inputStream;

  vm->pop(3);}


void MIDIClockValue(void) {
  /* midiClockValue */

  PmTimestamp currentTime = Pt_Time();
	
  vm->pop(1);
  vm->pushInteger(currentTime);}


void scheduleMIDIMessagesInQuantityOn(void) {
  /*
   * scheduleMessages: messages
   * inQuantity: size
   * on: theHandle
   */

  midiPort *port = (midiPort *) addressForStackValue(0);
  long	   length = (long) vm->stackIntegerValue(1);
  PmEvent  *messages = (PmEvent *)((vm->stackObjectValue(2)) + BaseHeaderSize);
  int	   result;

  result = Pm_Write(
		    port->outputStream,
		    messages,
		    length);
  if (result != pmNoError) {
    vm->primitiveFail();
    return;}

  vm->pop(3);}


void midiPortDataAvailable(void) {
  /* dataAvailableForMIDIPort: midiPortHandle */

  midiPort		*port = (midiPort *) (addressForStackValue(0));


  vm->popthenPush(
		  2,
		  ((Pm_Poll(port->inputStream))
		    ? vm->trueObject()
		    : vm->falseObject()));}


void nextAvailableMIDIDataFromInto(void) {
  /*
   * nextAvailableFrom: midiPortHandle
   * into: targetByteArray
   */


  int	   targetByteArray = vm->stackObjectValue(0);
  midiPort *port = (midiPort *)(addressForStackValue(1));
  int      numberOfPacketsRead;

  numberOfPacketsRead = Pm_Read(
				port->inputStream,
				targetByteArray + BaseHeaderSize,
				1024);
	
  vm->pop(3);
  vm->pushInteger(numberOfPacketsRead);}


void closeMIDIPortWithHandle(void) {
  /* closeMIDIPortWithHandle: handle */

  midiPort *port = (midiPort *) (addressForStackValue(0));

  Pm_Close(port->outputStream);
  Pm_Close(port->inputStream);
  vm->pop(1);}

