#include <string.h>
#include "device.h"
#include "MidiStatus.h"
#include "MidiWriter.h"
#include "OpenWareMidiControl.h"
#if defined USE_USBD_MIDI || defined USE_USBH_MIDI
#include "midi.h"
#endif
#ifdef USE_BLE_MIDI
#include "ble_midi.h"
#endif /* USE_BLE_MIDI */
#ifdef USE_UART_MIDI_TX
#include "uart_midi.h"
#endif
#include "SerialBuffer.hpp"
#include "message.h"

void MidiWriter::sendErrorMessage(const char* msg){
  char buf[64];
  buf[0] = 0x30; // error message
  char* p = &buf[1];
  p = stpncpy(p, msg, 62);
  sendSysEx((uint8_t*)buf, p-buf);
}

void MidiWriter::sendPc(uint8_t pc){
  send(MidiMessage::pc(channel, pc));
}

void MidiWriter::sendCc(uint8_t cc, uint8_t value){
  send(MidiMessage::cc(channel, cc, value));
}

void MidiWriter::sendNote(uint8_t note, uint8_t velocity){
  send(MidiMessage::note(channel, note, velocity));
}

void MidiWriter::sendPitchBend(uint16_t value){
  send(MidiMessage::pb(channel, value));
}

void MidiWriter::sendSysEx(uint8_t* data, uint16_t size){
  /* USB-MIDI devices transmit sysex messages in 4-byte packets which
   * contain a status byte and up to 3 bytes of the message itself.
   * If the message ends with fewer than 3 bytes, a different code is
   * sent. Go through the sysex 3 bytes at a time, including the leading
   * 0xF0 and trailing 0xF7.
   */
  MidiMessage packet(USB_COMMAND_SYSEX, SYSEX, MIDI_SYSEX_MANUFACTURER,
		     uint8_t(MIDI_SYSEX_OWL_DEVICE | channel));
  send(packet);
  int count = size/3;
  uint8_t* src = data;
  while(count-- > 0){
    packet.data[1] = (*src++ & 0x7f);
    packet.data[2] = (*src++ & 0x7f);
    packet.data[3] = (*src++ & 0x7f);
    send(packet);
  }
  count = size % 3;
  switch(count){
  case 0:
    packet.data[0] = USB_COMMAND_SYSEX_EOX1;
    packet.data[1] = SYSEX_EOX;
    packet.data[2] = 0;
    packet.data[3] = 0;
    break;
  case 1:
    packet.data[0] = USB_COMMAND_SYSEX_EOX2;
    packet.data[1] = (*src++ & 0x7f);
    packet.data[2] = SYSEX_EOX;
    packet.data[3] = 0;
    break;
  case 2:
    packet.data[0] = USB_COMMAND_SYSEX_EOX3;
    packet.data[1] = (*src++ & 0x7f);
    packet.data[2] = (*src++ & 0x7f);
    packet.data[3] = SYSEX_EOX;
    break;
  }
  send(packet);
}

class MidiTransmitter {
public:
  virtual void write(MidiMessage msg){};
  virtual void transmit(){};
};

#ifdef USE_USBD_MIDI
/* The buffer is written from the audio task, the main loop and the USB
 * interrupt (replies to SysEx requests), and transmit() is called from the
 * audio task and the main loop. Every access to the buffer state therefore
 * runs with interrupts masked (about a microsecond).
 * - A transfer is started and remembered in one piece, so two contexts can
 *   never start one on the same endpoint or advance the read head twice.
 * - USB copies the data out of this buffer after usbd_midi_tx() has returned
 *   (64 byte packets, in the USB interrupt). The read head only moves once
 *   the transfer has completed; until then the bytes still count as used.
 * - A full buffer drops the new message instead of overwriting unsent data.
 */
class UsbdMidiTransmitter : public MidiTransmitter {
private:
  SerialBuffer<MIDI_OUTPUT_BUFFER_SIZE> buffer;
  volatile size_t inflight = 0; // bytes handed to USB, transfer not yet completed
  volatile bool overflow = false;
  // call with interrupts masked
  inline __attribute__((always_inline)) void transmitMasked(){
    if(usbd_midi_ready()){
      if(inflight){
	// previous transfer has completed: release its bytes
	buffer.incrementReadHead(inflight);
	inflight = 0;
      }
      size_t len = buffer.getContiguousReadCapacity();
      if(len >= 4 && usbd_midi_tx(buffer.getReadHead(), len))
	inflight = len;
    }
  }
public:
  size_t getWriteCapacity(){
    // keep one packet free: a full buffer must not look like an empty one
    size_t used = buffer.getReadCapacity();
    return used + 4 < MIDI_OUTPUT_BUFFER_SIZE ? MIDI_OUTPUT_BUFFER_SIZE - 4 - used : 0;
  }
  void write(MidiMessage msg){
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if(getWriteCapacity() < 4)
      transmitMasked(); // releases the bytes of a completed transfer
    bool ok = getWriteCapacity() >= 4;
    if(ok)
      buffer.push(msg.data, 4);
    __set_PRIMASK(primask);
    if(ok){
      overflow = false;
    }else if(!overflow && usbd_midi_connected()){
      overflow = true; // report once per episode, and only if a host is there to read
      debugMessage("midi tx ovf");
    }
  }
  inline __attribute__((always_inline)) void transmit(){
    // Unmasked early exits, so the audio task masks nothing while idle, while
    // a transfer is running or without a host. Both tests are repeated or
    // harmless under the mask: a missed chance is taken in the next block.
    if((inflight == 0 && !buffer.notEmpty()) || !usbd_midi_ready())
      return;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    transmitMasked();
    __set_PRIMASK(primask);
  }
};
UsbdMidiTransmitter usbd_midi;
#endif

#ifdef USE_USBH_MIDI
class UsbhMidiTransmitter : public MidiTransmitter {
private:
  SerialBuffer<MIDI_OUTPUT_BUFFER_SIZE> buffer;
public:
  void write(MidiMessage msg){
    buffer.push(msg.data, 4);
  }
  void transmit(){
    size_t len = buffer.getContiguousReadCapacity();
    if(len >= 4 && usbh_midi_ready()){
      usbh_midi_tx(buffer.getReadHead(), len);
      buffer.incrementReadHead(len);
      // len = buffer.getContiguousReadCapacity();
    }
  }
};
UsbhMidiTransmitter usbh_midi;
#endif

#ifdef USE_BLE
class BleMidiTransmitter : public MidiTransmitter {
private:
  SerialBuffer<MIDI_OUTPUT_BUFFER_SIZE> buffer;
public:
  void write(MidiMessage msg){
    buffer.push(msg.data, 4);
  }
  void transmit(){
    size_t len = buffer.getContiguousReadCapacity();
    if(len >= 4){
      ble_tx(buffer.getReadHead(), len);
      buffer.incrementReadHead(len);
    }
  }
};
BleMidiTransmitter ble_midi;
#endif

#ifdef USE_UART_MIDI_TX
class UartMidiTransmitter : public MidiTransmitter {
private:
  SerialBuffer<MIDI_OUTPUT_BUFFER_SIZE> buffer;
public:
  void write(MidiMessage msg){
    buffer.push(msg.data+1, 3); // skip USB byte
  }
  void transmit(){
    size_t len = buffer.getContiguousReadCapacity();
    if(len && uart_ready()){
      uart_tx(buffer.getReadHead(), len);
      buffer.incrementReadHead(len);
    }
  }
};
UartMidiTransmitter uart_midi DMA_RAM;
#endif

void MidiWriter::send(MidiMessage msg){
#ifdef USE_USBD_MIDI
  usbd_midi.write(msg);
#endif
#ifdef USE_USBH_MIDI
  usbh_midi.write(msg);
#endif
#ifdef USE_BLE
  ble_midi.write(msg);
#endif
#ifdef USE_UART_MIDI_TX
  uart_midi.write(msg);
#endif
// #ifdef USE_DIGITALBUS
//   bus_tx_frame(msg.data);
// #endif /* USE_DIGITALBUS */
}

size_t MidiWriter::getWriteCapacity(){
#ifdef USE_USBD_MIDI
  return usbd_midi.getWriteCapacity();
#else
  return MIDI_OUTPUT_BUFFER_SIZE;
#endif
}

void MidiWriter::transmit(){
#ifdef USE_USBD_MIDI
  usbd_midi.transmit();
#endif
#ifdef USE_USBH_MIDI
  usbh_midi.transmit();
#endif
#ifdef USE_BLE
  ble_midi.transmit();
#endif
#ifdef USE_UART_MIDI_TX
  uart_midi.transmit();
#endif
}
