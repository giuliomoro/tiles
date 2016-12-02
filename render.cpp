// Multi-sample-playback

// NOTES For REBUILD
// Debounce doesn't work properly
// For debounce if triggered debounce, if note < 20 % of amplitude of strike
// then ignore, otherwise new note.
// Better voice stealing algorithm

#include <Bela.h>
#include <Utilities.h>
#include <cmath>
#include <Scope.h>
#include <drums.h>

#include <pairs.h>

#include <WriteFile.h>
#include <time.h>

#include <RampGenerator.h>

#define NUM_RAMPS 8

#define BUFFER_SIZE 22050 // Delay Buffer Size (0.5 ms)

WriteFile file1;

RampGenerator rampGenerators[NUM_RAMPS];



// 7.5ms = 331, 15ms = 662, 22.5ms = 993


float gInverseSampleRate;
int gAudioFramesPerAnalogFrame;


float gHoldAmplitudes[NUM_RAMPS]; // Store amplitudes on voice allocation

// PARTICIPANT SPECIFIC

#define PARTICIPANT_NUMBER 9

// --------------------


// STATE CHANGING --------------------

int gSampleSet = 0;     // Boolean to switch between A and B (multiples of 8)
int gLatencyState = 0;  // Store with latency condition is currently active
int gAmountOfJitter = 133;       // Store the amount of jitter
int gLatencies[4] = {0, -441, -882, -441}; // Store latency values in samples

int gStateASample = 0;  // store which sample set is A
int gStateBSample = 2;  // store which sample set is B
// PART 2
int gStateCSample = 0;  // store which sample set is C
int gStateDSample = 2;  // store which sample set is D


int gStateALatency = 0; // store latency of A
int gStateBLatency = 3; // store latency of B
// PART 2
int gStateCLatency = 3; // store latency of C
int gStateDLatency = 3; // store latency of D

int gPairNumber = 0; // store the number of the current pair of settings

// -----------------------------------


/* Drum samples are pre-loaded in these buffers. Length of each
 * buffer is given in gDrumSampleBufferLengths.
 */
extern float *gDrumSampleBuffers[NUMBER_OF_DRUMS];
extern int gDrumSampleBufferLengths[NUMBER_OF_DRUMS];

// Number of voices (samples) that are possible to play at once
const int NUMBER_OF_VOICES = 40;
// Array of read pointers for multisample playback
int gReadPointers[NUMBER_OF_VOICES] = {0};
// which buffer is associated with each read pointer
int gReadPointerBufferID[NUMBER_OF_VOICES] = {0};
// Has the pointer been assigned to a sample
int gPointerState[NUMBER_OF_VOICES] = {0};

// Array of amplitudes for multisample playback
float gAmplitudes[NUMBER_OF_VOICES] = {0};

// Array of delayed read 


// Voice stealing
int gReadPointerAge[NUMBER_OF_VOICES] = {0}; // Buffer to store the sample count at which the voice was allocated for voice stealing.

uint64_t gSampleCount = 0; // stores the context sample counter

extern int gDrumSamplesLoaded;  // wait for the wav files to be loaded before accessing


// DEBOUNCE
int gDebounce[NUMBER_OF_VOICES] = {1};
int gDebounceTime = 500;
int gDebounceCounter[NUMBER_OF_VOICES] = {0};

int gInDebounceCounter = 0; // one counter for all voices
int gInDebounce = 1; // set when a note is trigger to disable neighbouring notes and save voices


// struct RampConfig   {
//     int numReadings = 12;
//     float thresholdToTrigger = 0.001;
//     float amountBelowPeak = 0.0001;
//     float rolloffRate = 0.0002;
// };

// RampConfig rampConfig;


Scope scope;

// UDP --------------------------------

#define NET_DATA_BUFFER_SIZE_RECEIVE 128

struct netReceiveData{
	int port;
	int boundToPort;
	char messageBuffer[NET_DATA_BUFFER_SIZE_RECEIVE];
	UdpServer udpServer;
};

// global variables
int gState = 0;
int gStateHasChanged = 0;
int gUdpReceivePort = 9876;
netReceiveData netDataReceiver;
AuxiliaryTask receiveUdpMessagesTask;

int gNext = 0;
int gNextHasChanged = 0;

int gRecord = 0;
int gShouldStopLogging = 0;


void receiveUdpMessages()	{
	while(!gShouldStop){
		int numBytes = netDataReceiver.udpServer.read(&netDataReceiver.messageBuffer, NET_DATA_BUFFER_SIZE_RECEIVE, false);
		if(numBytes < 0 || netDataReceiver.messageBuffer == NULL) {
		} else {
			// PARSE ROUTING MESSAGES (very ugly but it works -- maybe switch to OSC later)
			char *p;
		   p = strtok(netDataReceiver.messageBuffer, " ");
		   
		   
		   
		   int n = 0;
		   int isStateChangeMessage = -1;
		   
		   int isNextMessage = -1;
		   
		   int isRecordMessage = -1;

		   while(p)	{
		       //printf("UDP: understood \'%s\'\n",p);
	   	    n += 1;

			   if(n==1)	{
			   	if(!strncmp(p,"state",5))	{
			   		isStateChangeMessage = 1;
			   		//printf("UDP: understood \'%s\'\n",p);
			   	} 
			   	else if(!strncmp(p,"next",4))	{
			   		isNextMessage = 1;
			   		//printf("UDP: understood \'%s\'\n",p);
			   	}
			   	else if(!strncmp(p,"record",4))	{
			   		isRecordMessage = 1;
			   		//printf("UDP: understood \'%s\'\n",p);
			   	} 
			   	else {
					printf("UDP: didn't understand \'%s\'\n",p);
				   	break;
				}
			   	
			   	 
			   }

			   if(isStateChangeMessage == 1)	{
			   		if(n==2)	{
				   		int i;
						sscanf(p, "%d", &i);
						if(i<=4 && i>=0)	{
						  	gState = i;
						  	gStateHasChanged = 1;
						  //	//DEBUG -----
						  //    isStateChangeMessage = 0;
						  //    printf("STATE MESSAGE: %d \n", gState);
						  //	// ----------
						} else {
							printf("UDP: invalid state: \'%s\'\n",p);
							break;
						}
				   	}
				}
				
				if(isNextMessage == 1)	{
			   		if(n==2)	{
				  		int j;
						sscanf(p, "%d", &j);
						if(j<=14 && j>=0)	{
						  	gNext = j;
						  	gNextHasChanged = 1;
					    	//DEBUG -----
						  	// isNextMessage = 0;
						  	// printf("NEXT MESSAGE: %d \n", gNext);
						  	// ----------
						  	
						} else {
							printf("UDP: invalid next message: \'%s\'\n",p);
							break;
						}
				  	}
				}
				
				if(isRecordMessage == 1)	{
			   		if(n==2)	{
				  		int j;
						sscanf(p, "%d", &j);
						if(j<=2 && j>=0)	{
						  	gRecord = j;
						  	gShouldStopLogging = j;
						  	//gRecordHasChanged = 1;
					    	//DEBUG -----
						  	// isNextMessage = 0;
						  	rt_printf("RECORD%d\n", j);
						  	// ----------
						  	
						} else {
							printf("UDP: invalid next message: \'%s\'\n",p);
							break;
						}
				  	}
				}
				
				
				
			   p = strtok(NULL, " ");
			}

			for(int i=0;i<NET_DATA_BUFFER_SIZE_RECEIVE;i++)
				netDataReceiver.messageBuffer[i] = NULL;
		}
		usleep(10000);
	}
}








bool setup(BelaContext *context, void *userData)
{   
    
    time_t currentTime = time(0);

	char buffer[100];
	sprintf(buffer, "/root/dataLog%d_%ld_%d.bin", PARTICIPANT_NUMBER, currentTime, rand());
	rt_printf(buffer);
	file1.init(buffer); //set the file name to write to
   	//file1.setEchoInterval(1000);
    file1.setFileType(kBinary);
    //file1.setFormat("%.4f %.4f\n"); // set the format that you want to use for your output. Please use %f only (with modifiers). When in binary mode, this is used only for echoing to console
    
    
    
    
    // LOAD SETTINGS --------------
    
    gStateASample = gPairs[PARTICIPANT_NUMBER][gPairNumber][0];  // store which sample set is A
    gStateALatency = gPairs[PARTICIPANT_NUMBER][gPairNumber][1]; // store latency of A
    
    gStateBSample = gPairs[PARTICIPANT_NUMBER][gPairNumber][2];  // store which sample set is B
    gStateBLatency = gPairs[PARTICIPANT_NUMBER][gPairNumber][3]; // store latency of B
    
    //-----------------------------
    
    
    
    
    // UDP ------------------
    netDataReceiver.port = gUdpReceivePort;
	//netDataReceiver.messageBuffer = NULL;
	if(netDataReceiver.boundToPort = netDataReceiver.udpServer.init(gUdpReceivePort))	{
		printf("Successfully bound to port %d\n",gUdpReceivePort);
		// Start auxiliary task for receiving UDP messages
		receiveUdpMessagesTask = Bela_createAuxiliaryTask(*receiveUdpMessages, 75, "receive-udp-messages");
		Bela_scheduleAuxiliaryTask(receiveUdpMessagesTask);
	} else {
		printf("Unable to bind to port %d. No communication to host.\n",gUdpReceivePort);
	}
    
    // ----------------------
    
    
    gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
    
	scope.setup(8, context->audioSampleRate);

	gInverseSampleRate = 1.0 / context->analogSampleRate;

    for(int i=0;i<NUM_RAMPS;i++)    {
        rampGenerators[i] = RampGenerator();
        //rampGenerators[i].config();
    }

    // rampConfig.thresholdToTrigger = 0.001;
    // rampConfig.amountBelowPeak = 0.0001;
    // rampConfig.rolloffRate = 0.0002;
    // rampConfig.numReadings = 12;

     rt_printf("Number of drums: %d\n", NUMBER_OF_DRUMS);
    
    // for(int i=0;i<NUMBER_OF_DRUMS;i++)    {
    //     rt_printf("Length %d: %d ,", i, gDrumSampleBufferLengths[i]);
    //     rt_printf("\n");
    //     //rampGenerators[i].config();
    // }
    
    //rt_printf("%d %d %d %d\n", gStateASample, gStateALatency,gStateBSample,gStateBLatency);


	return true;
}



void render(BelaContext *context, void *userData)
{

//  UDP --------------    
    
    // Switch between setting A and B
    if(gStateHasChanged)	{
		gStateHasChanged = 0;
		rt_printf("RECEIVED UDP MSG: Switching to state %d\n", gState);

		if(gState == 0) {
		    // Setting A
		    gSampleSet = (gStateASample * 8)-8;
		    gLatencyState = gStateALatency;
		    rt_printf("PAIR %d %d\n", gSampleSet, gLatencyState);
		    
		} else if(gState == 1){
		    // Setting B
		    gSampleSet = (gStateBSample * 8)-8;
		    gLatencyState = gStateBLatency;
		    rt_printf("PAIR %d %d\n", gSampleSet, gLatencyState);
		} else if(gState == 2 && gPairNumber > 11){
		    // Setting C
		    gSampleSet = (gStateCSample * 8)-8;
		    gLatencyState = gStateCLatency;
		    rt_printf("PAIR %d %d\n", gSampleSet, gLatencyState);
		} else if(gState == 3 && gPairNumber > 11){
		    // Setting D
		    gSampleSet = (gStateDSample * 8)-8;
		    gLatencyState = gStateDLatency;
		    rt_printf("PAIR %d %d\n", gSampleSet, gLatencyState);
		}
		
		if(gPairNumber > 11){
		    
		}
		
		// Change to the next sample set.
		
	}

    // Move onto next pair of settings
    if(gNextHasChanged) {
        gNextHasChanged = 0;
        gStateHasChanged = 1; // Update the current state
        rt_printf("RECEIVED UDP MSG: Next Pair %d\n", gNext);
        
        gPairNumber = gNext;
        
        // LOAD SETTINGS --------------
        gStateASample = gPairs[PARTICIPANT_NUMBER][gPairNumber][0];  // store which sample set is A
        gStateALatency = gPairs[PARTICIPANT_NUMBER][gPairNumber][1]; // store latency of A
        gStateBSample = gPairs[PARTICIPANT_NUMBER][gPairNumber][2];  // store which sample set is B
        gStateBLatency = gPairs[PARTICIPANT_NUMBER][gPairNumber][3]; // store latency of B
        
        gStateCSample = gPairs[PARTICIPANT_NUMBER][gPairNumber][4];  // store which sample set is A
        gStateCLatency = gPairs[PARTICIPANT_NUMBER][gPairNumber][5]; // store latency of A
        gStateDSample = gPairs[PARTICIPANT_NUMBER][gPairNumber][6];  // store which sample set is B
        gStateDLatency = gPairs[PARTICIPANT_NUMBER][gPairNumber][7];
        //-----------------------------
        rt_printf("%d %d %d %d %d %d %d %d\n", gStateASample, gStateALatency,gStateBSample,gStateBLatency, gStateCSample, gStateCLatency,gStateDSample,gStateDLatency);
    }

    // --------------------
    
    
    
    
    
    
    
    float analogInPiezo[NUM_RAMPS];
    float generatedRamps[NUM_RAMPS];
    
    float threshold = 0.001;//0.000001;
    float amountBelow = 0.0005;// = 0.000000001;
    float rollOff = 0.000078;// = 0.0000078;
    
    
    
    
    
    
    

	for(unsigned int n = 0; n < context->audioFrames; n++) {
	    
	    float out = 0.0;
	    float audioIn = 0.0;
	    float audioThrough = 0.0;

        for(int i=0; i<NUM_RAMPS;i++)   {
            
            // Piezo ins
            if (i == 0){
                analogInPiezo[i] = (analogRead(context, n/gAudioFramesPerAnalogFrame, i))*1.5;
            } else {
                analogInPiezo[i] = analogRead(context, n/gAudioFramesPerAnalogFrame, i);
            }
            
            
            
            // Onset detection
            // if(i==0) {
            //     generatedRamps[0] = rampGenerators[0].processRamp(analogInPiezo[0],0.00001,0.0001,rollOff);
            // }
            // else {
            //     generatedRamps[i] = rampGenerators[i].processRamp(analogInPiezo[i],threshold,amountBelow,rollOff);
            // }
            
            generatedRamps[i] = rampGenerators[i].processRamp(analogInPiezo[i],threshold,amountBelow,rollOff);
            
            
        
            // Voice allocation
            if((generatedRamps[i] > 0.0 && gDrumSamplesLoaded && gDebounce[i] && gInDebounce) || (generatedRamps[i] > 0.01 && gDrumSamplesLoaded && gDebounce[i])){
                
                gDebounceCounter[i] = gSampleCount + gDebounceTime;
                gInDebounceCounter = gSampleCount + gDebounceTime;
                gDebounce[i] = 0;
                gInDebounce = 0;
                // if latency condition of jitter then calculate the random variation and send it
                // to startPlayingDrum. Else then send the straight value.
                int latency = 0;
                
                if(gLatencyState == 3) {
                    latency = gLatencies[gLatencyState] - (gAmountOfJitter*0.5) + (rand()%gAmountOfJitter);
                }
                else {
                    latency = gLatencies[gLatencyState];
                }
                
                
            	gHoldAmplitudes[i] = generatedRamps[i];
            	if(startPlayingDrum((i+gSampleSet), gHoldAmplitudes[i], latency) == 0){
            	    startPlayingDrum((i+gSampleSet), gHoldAmplitudes[i], latency);
            	};
            	
            	//rt_printf("TRIGGERED: %d", startPlayingDrum((i+gSampleSet), gHoldAmplitudes[i]));
            	//rt_printf("Playing: %d, amplitude: %f\n", i, gHoldAmplitudes[i]);
            }
            if(!gDebounce[i] && gSampleCount >= gDebounceCounter[i]) {
                gDebounce[i] = 1;
                //rt_printf("DEBOUNCE FINISHED: %d\n", i);
            }
            
            if(!gInDebounce && gSampleCount >= gInDebounceCounter) {
                gInDebounce = 1;
                // rt_printf("IN DEBOUNCE FINISHED\n");
                // for(int j=0; j<NUM_RAMPS;j++){
                //     rt_printf("%d ", gDebounce[j]);
                // }
                // rt_printf("\n");
            }
            
        } 

	   
	   // DEBUG ----------
	   // slider = analogRead(context, n/gAudioFramesPerAnalogFrame, 7);
	   // slider2 = analogRead(context, n/gAudioFramesPerAnalogFrame, 6);
	   // amountBelow = (slider - 0.0) * (0.5 - 0.000004) / (1.0 - 0.0) + 0.000004;
	   // rollOff = (slider2 - 0.0) * (0.5 - 0.0000004) / (1.0 - 0.0) + 0.00004;
	   // ---------------- 
	    
	    for(int j=0; j < NUMBER_OF_VOICES; j++){
	        // Prevent negative pointer to the buffer and have the delay
	        if(gReadPointers[j] >= 0 && gPointerState[j] == 1) {
	            out += gDrumSampleBuffers[gReadPointerBufferID[j]][gReadPointers[j]] * (gAmplitudes[j]);
	            //rt_printf("Playing: %d\n", j);
	        }
		}
		
		
		for(unsigned int channel = 0; channel < context->audioInChannels; channel++){
		    
		    audioIn = context->audioIn[n * context->audioInChannels + channel];
		    audioThrough = context->audioIn[n * context->audioInChannels]*0.5;
			context->audioOut[n * context->audioInChannels + channel] = out + audioThrough;
			
		}
		
		
		for(int j=0; j < NUMBER_OF_VOICES; j++){

			//Cycles through voices incrementing or decrementing through samples until the end
			// of playback when the voice is freed up.	
			if (gPointerState[j] == 1 && gReadPointers[j] < gDrumSampleBufferLengths[gReadPointerBufferID[j]]) {
				gReadPointers[j]++;
			}
			else {
				gPointerState[j] = 0;
			}
		}
		
		
		gSampleCount = context->audioFramesElapsed;
		
		// SCOPE FOR DEBUG	
		scope.log(analogInPiezo[0], analogInPiezo[1], analogInPiezo[2], analogInPiezo[3], analogInPiezo[4], analogInPiezo[5], analogInPiezo[6], analogInPiezo[7]);
	
	}
    
    if(!gShouldStopLogging){
		gShouldStopLogging  = 1;
		for(int k = 0; k < context->audioInChannels * context->audioFrames * 2 + context->analogInChannels * context->analogFrames; k++){
			file1.log(1);
		}	
		rt_printf("STOP RECORDING\n");
	}
	if(gRecord){
 		file1.log(context->audioIn, context->audioInChannels * context->audioFrames); // log audio in
 		file1.log(context->audioOut, context->audioInChannels * context->audioFrames); // log audio out
 		file1.log(context->analogIn, context->analogInChannels * context->analogFrames); // log analog ins
	}	
}


int startPlayingDrum(int drumIndex, float drumAmplitude, int drumLatency) {

	// Assigns the next free voice to play the drum sample
	for(int j=0; j < NUMBER_OF_VOICES; j++){
	    
	    //rt_printf("%d %d\n", gPointerState[j], j);
	    
		if(gPointerState[j] == 0){
			gReadPointers[j] = drumLatency; // samples of latency (negative)
			gReadPointerBufferID[j] = drumIndex;
			gPointerState[j] = 1;
			gReadPointerAge[j] = gSampleCount;

			//rt_printf("%f\n",drumAmplitude);
			
			// Set amplitude individually
            // if(drumIndex == 0 || drumIndex == 8 || drumIndex == 16 || drumIndex == 24) {
            //     gAmplitudes[j] = drumAmplitude*2;
            // }
            // else {
            //     gAmplitudes[j] = drumAmplitude;
            // }
            
            gAmplitudes[j] = drumAmplitude;
            
            
			

			rt_printf("Voice: %d Sample: %d Amplitude: %f Age: %d Latency: %d\n", j, drumIndex, gAmplitudes[j], gReadPointerAge[j], drumLatency);
			
// 			for(int i=0;i<NUMBER_OF_DRUMS;i++)    {
//                 rt_printf("Length %d: %d ,", i, gDrumSampleBufferLengths[i]);
//                 rt_printf("\n");
//                 //rampGenerators[i].config();
//             }

			return 1;
		}
		
		
		
	}
	
	// Voice allocation – oldest out
	
	rt_printf("all voices allocated ");
	
	int oldest = gReadPointerAge[0];
	int oldestPointer = 0;
	for(int i=0;i<NUMBER_OF_VOICES;i++){
	    //rt_printf("%d ", gReadPointerAge[i]);
	    if(gReadPointerAge[i] < oldest){
	        oldest = gReadPointerAge[i];
	        oldestPointer = i;
	    }
	    
	}
	
	//gPointerState[oldestPointer] = 0;
	//rt_printf("releasing voice %d\n", oldestPointer);
	
	
	
	// Voice Stealing – lowest starting volume
	
// 	float quietest = gAmplitudes[0];
// 	int quietestPointer = 0;
// 	for(int i=0;i<NUMBER_OF_VOICES;i++){
// 	    //rt_printf("%f ", gAmplitudes[i]);
// 	    if(gAmplitudes[i] < quietest){
// 	        quietest = gAmplitudes[i];
// 	        quietestPointer = i;
// 	    }
	    
//  	}
	
	// If the quietest voice is over half a second old then free it, otherwise free the oldest voice.
	
// 	if(gSampleCount - gReadPointerAge[quietestPointer] > 22050){
// 	    gPointerState[quietestPointer] = 0;
// 	    rt_printf("FREED QUIETEST\n");
// 	} 
// 	else {
	    gPointerState[oldestPointer] = 0;
	    rt_printf("FREED OLDEST\n");
// 	}
	
	
// 	rt_printf("releasing voice %d\n", quietestPointer);
	
	return 0;
}


// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BelaContext *context, void *userData)
{

}
