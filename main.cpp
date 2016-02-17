#include <iostream>
#include <stdint-gcc.h>
#include <unistd.h>
#include <stdint.h>
#include <chrono>

#include "libusb/libusb.h"
#include "midifile/midifile.h"

#define STEAM_CONTROLLER_MAGIC_PERIOD_RATIO 495483.0

using namespace std;

double midiFrequency[128]  = {8.1758, 8.66196, 9.17702, 9.72272, 10.3009, 10.9134, 11.5623, 12.2499, 12.9783, 13.75, 14.5676, 15.4339, 16.3516, 17.3239, 18.354, 19.4454, 20.6017, 21.8268, 23.1247, 24.4997, 25.9565, 27.5, 29.1352, 30.8677, 32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55, 58.2705, 61.7354, 65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989, 103.826, 110, 116.541, 123.471, 130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220, 233.082, 246.942, 261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440, 466.164, 493.883, 523.251, 554.365, 587.33, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880, 932.328, 987.767, 1046.5, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760, 1864.66, 1975.53, 2093, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520, 3729.31, 3951.07, 4186.01, 4434.92, 4698.64, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040, 7458.62, 7902.13, 8372.02, 8869.84, 9397.27, 9956.06, 10548.1, 11175.3, 11839.8, 12543.9};

void delay_ms(unsigned int ms){
    for(; ms > 0 ; ms --)
        usleep(1000);
}

libusb_device_handle* SteamController_OpenAndClaim(int *interface_num){
    libusb_device_handle* dev_handle;
    //Open Steam Controller device
    if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1102)) != NULL){ // Wired Steam Controller
        cout<<"Found wired Steam Controller"<<endl;
        (*interface_num) = 2;
    }
    else if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1142)) != NULL){ // Steam Controller dongle
        cout<<"Found Steam Dongle, will attempt to use the first Steam Controller"<<endl;
        (*interface_num) = 1;
    }
    else{
        cout<<"No device found"<<endl;
        std::cin.ignore();
        return NULL;
    }

    //On Linux, automatically detach and reattach kernel module
    libusb_set_auto_detach_kernel_driver(dev_handle,1);

    //Claim the USB interface controlling the haptic actuators
    int r = libusb_claim_interface(dev_handle,(*interface_num));
    if(r < 0) {
        cout<<"Interface claim Error "<<r<<endl;
        std::cin.ignore();
        libusb_close(dev_handle);
        return NULL;
    }

    return dev_handle;
}


int SteamController_PlayNote(libusb_device_handle *dev_handle, int haptic, unsigned int note,double duration ){
    unsigned char dataBlob[64] = {0x8f,
                                  0x07,
                                  0x00, //Trackpad select : 0x01 = left, 0x00 = right
                                  0xff, //LSB Pulse High Duration
                                  0xff, //MSB Pulse High Duration
                                  0xff, //LSB Pulse Low Duration
                                  0xff, //MSB Pulse Low Duration
                                  0xff, //LSB Pulse repeat count
                                  0x04, //MSB Pulse repeat count
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    double frequency = midiFrequency[note];
    double period = 1.0 / frequency;
    uint16_t periodCommand = period * STEAM_CONTROLLER_MAGIC_PERIOD_RATIO;

    uint16_t repeatCount = duration / period;

    //cout << "Frequency : " <<frequency << ", Period : "<<periodCommand << ", Repeat : "<< repeatCount <<"\n";

    dataBlob[2] = haptic;
    dataBlob[3] = periodCommand % 0xff;
    dataBlob[4] = periodCommand / 0xff;
    dataBlob[5] = periodCommand % 0xff;
    dataBlob[6] = periodCommand / 0xff;
    dataBlob[7] = repeatCount % 0xff;
    dataBlob[8] = repeatCount / 0xff;

    int r;
    r = libusb_control_transfer(dev_handle,0x21,9,0x0300,2,dataBlob,64,1000);
    if(r < 0) {
        cout<<"Command Error "<<r<<endl;
        std::cin.ignore();
        return 1;
    }

    return 0;
}

float timeElapsedSince(std::chrono::steady_clock::time_point tOrigin){
    using namespace std::chrono;
    steady_clock::time_point tNow = steady_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(tNow - tOrigin);
    return time_span.count();
}

void playSong(libusb_device_handle *steamcontroller_handle, char* songfile){

    MidiFile_t midifile;

    cout << "Loading midi file " << songfile << endl;

    //Open Midi File
    midifile = MidiFile_load(songfile);

    if(midifile == NULL){
        cout << "Unable to open song file " << songfile << endl;
        return;
    }

    //Check if file contains at least one midi event
    if(MidiFile_getFirstEvent(midifile) == NULL){
        cout << "Song file is empty !!" << endl;
        return;
    }

    //Waiting for user to press enter
    cout << "Ready, press ENTER to start" << endl;
    std::cin.ignore();

    //Get current time point, will be used to know elapsed time
    std::chrono::steady_clock::time_point tOrigin = std::chrono::steady_clock::now();

    //Iterate through events
    MidiFileEvent_t currentEvent;
    for ( currentEvent = MidiFile_getFirstEvent(midifile) ; currentEvent != NULL ; currentEvent = MidiFileEvent_getNextEventInFile(currentEvent)) {
        if(MidiFileEvent_getType(currentEvent) != MIDI_FILE_EVENT_TYPE_META){

            //Retrieving event time
            long currentEventTick = MidiFileEvent_getTick(currentEvent);
            float currentEventTime = MidiFile_getTimeFromTick(midifile,currentEventTick);

            //If required, wait for event
            int waitTime = (currentEventTime - timeElapsedSince(tOrigin)) * 1000.0f;
            if (waitTime > 0) {
                delay_ms(waitTime);
            }

            //If it is a note start event
            if (MidiFileEvent_isNoteStartEvent(currentEvent)){
                //Look for the matching note end event since we need to now how long to play
                MidiFileEvent_t endEvent = MidiFileNoteStartEvent_getNoteEndEvent(currentEvent);

                //If not found, skip this event
                if(endEvent == NULL)continue;

                //Get channel event
                int eventChannel = MidiFileNoteStartEvent_getChannel(currentEvent);

                //If channel is other than 0 or 1, skip this event, we cannot play it with only 1 steam controller
                if(eventChannel < 0 || eventChannel > 1) continue;

                //Get end event time
                long endEventTick = MidiFileEvent_getTick(endEvent);
                float endEventTime = MidiFile_getTimeFromTick(midifile,endEventTick);

                //Compute note duration
                float duration = endEventTime - currentEventTime;

                //Get event note
                int eventNote = MidiFileNoteStartEvent_getNote(currentEvent);

                //Finally drive the Steam Controller
                duration-= 0.02f;   //This is to deal with wireless lagg happening sometimes
                SteamController_PlayNote(steamcontroller_handle,eventChannel,eventNote,duration);

                cout << ((eventChannel == 0) ? "RIGHT" : "LEFT ") << " haptic : note " << eventNote << " for "<< (int)(duration*1000) << " ms" <<endl;
            }
        }
    }

    cout << "Playback completed " << endl;
}

int main(int argc, char** argv)
{
    libusb_device_handle *dev_handle; //a device handle
    int interface_num = 0;


    int r; //for return values

    cout <<"Steam Controller Singer by Pila"<<endl;

    if(argc != 2){
        cout << "Usage : " << argv[0] << " midisong.mid" << endl;
        return 1;
    }

    //Initializing LIBUSB
    r = libusb_init(NULL);
    if(r < 0) {
        cout<<"LIBUSB Init Error "<<r<<endl;
        std::cin.ignore();
        return 1;
    }

    libusb_set_debug(NULL, 3);

    //Gaining access to Steam Controller
    dev_handle = SteamController_OpenAndClaim(&interface_num);
    if(dev_handle == NULL)
        return 1;


    //Playing song
    playSong(dev_handle,argv[1]);


    //Releasing access to Steam Controller
    r = libusb_release_interface(dev_handle,interface_num);
    if(r < 0) {
        cout<<"Interface release Error "<<r<<endl;
        std::cin.ignore();
        return 1;
    }
    libusb_close(dev_handle);

    return 0;
}
