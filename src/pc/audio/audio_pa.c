#include <portaudio.h>
#include "audio_api.h"
#include "audio_pa.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define SAMPLE_RATE   (32000)
#define FRAMES_PER_BUFFER  (4352/4)

#define TABLE_SIZE   (4352/2)
typedef struct
{
    float samples[TABLE_SIZE];
    char message[20];
}
paTestData;

paTestData data;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t newSamplesCond = PTHREAD_COND_INITIALIZER;

bool newSamples = false;

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData )
{
    //paTestData *data = (paTestData*)userData;
    float *out = (float*)outputBuffer;
    unsigned long i;

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    (void) framesPerBuffer;
    (void) userData;

    pthread_mutex_lock(&lock);
    while (!newSamples)
        pthread_cond_wait(&newSamplesCond, &lock);
    for( i=0; i<TABLE_SIZE; i += 2 )
    {
        *out++ = data.samples[i]; //left
        *out++ = data.samples[i+1]; //right
    }
    newSamples = false;
    pthread_mutex_unlock(&lock);

    return paContinue;
}

static bool audio_pa_init(void) {
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    PaStreamParameters outputParameters;
    PaStream *stream;
    PaError err;

    printf("PortAudio Test: output sine wave. SR = %d, BufSize = %d\n", SAMPLE_RATE, FRAMES_PER_BUFFER);

    /*for(int i=0; i<TABLE_SIZE; i++ )
    {
        printf("TILL HERE\n\n");
        data.samples[i] = (float) sin( ((double)i/(double)TABLE_SIZE) * M_PI * 2. );
    }*/

    err = Pa_Initialize();
    if( err != paNoError ){
        printf("Couldn't init portaudio\n");
        return false;
    }
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        return false;
    }
    outputParameters.channelCount = 2;       /* stereo output */
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        NULL, /* no input */
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,      /* we won't output out of range samples so don't bother clipping them */
        patestCallback,
        &data );
    if( err != paNoError ) {
        printf("Couldn't open portaudio stream\n");
        return false;
    }

    //sprintf( data->message, "No Message" );
    /*err = Pa_SetStreamFinishedCallback( stream, &StreamFinished );
    if( err != paNoError ){
        printf("Couldn't set portaudio finishing callback\n");
        return false;
    }*/

    err = Pa_StartStream( stream );
    if( err != paNoError ){
        printf("Couldn't start portaudio stream\n");
        return false;
    }

    return true;
    //scanf("%d");

    //Pa_Terminate();

   /* err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;

    Pa_Terminate();
    printf("Test finished.\n");

    return true;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return false;
    return true;*/
}

static int audio_pa_buffered(void) {
    return 1;
}

static int audio_pa_get_desired_buffered(void) {
    return 1100;
}

static void audio_pa_play(const uint8_t *buf, size_t len) {
    pthread_mutex_lock(&lock);
    for(int i = 0; i < len; i += 4){
        int16_t val = (buf[i+1] << 8 ) | (buf[i]);
        data.samples[i/2] = (float)val/(float)(1<<16);
        int16_t val2 = (buf[i+3] << 8 ) | (buf[i+2]);
        data.samples[i/2+1] = (float)val2/(float)(1<<16);
    }
    newSamples = true;
    pthread_cond_signal(&newSamplesCond);
#ifdef AUDIO_DEBUG
        FILE *fptr;

        if ((fptr = fopen("audio_buffer.bin","ab+")) == NULL){
            printf("Error! opening file");
            // Program exits if the file pointer returns NULL.
            exit(2);
        }

        fwrite(data.samples, sizeof(float), TABLE_SIZE, fptr);
        fclose(fptr);
#endif
        pthread_mutex_unlock(&lock);
}

struct AudioAPI audio_pa = {
    audio_pa_init,
    audio_pa_buffered,
    audio_pa_get_desired_buffered,
    audio_pa_play
};