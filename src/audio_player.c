/*
 * This example creates an SDL window and renderer, and draws a
 * rotating texture to it, reads back the rendered pixels, converts them to
 * black and white, and then draws the converted image to a corner of the
 * screen.
 *
 * This isn't necessarily an efficient thing to do--in real life one might
 * want to do this sort of thing with a render target--but it's just a visual
 * example of how to use SDL_RenderReadPixels().
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif
/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static int texture_width = 0;
static int texture_height = 0;
static SDL_Texture *converted_texture = NULL;
static int converted_texture_width = 0;
static int converted_texture_height = 0;

static SDL_AudioStream* stream = NULL;
static SDL_AudioSpec spec;
static Uint8* wav_data = NULL;
static Uint32 wav_data_len = 0;
static bool IsPause = false;
float rotation = 0;
static const SDL_Rect cbuttons_pause_rect = { 46, 0, 23, 18 };
static const SDL_Rect cbuttons_pause_rect_pressed = { 46, 18, 23, 18 };
static const SDL_Rect rewind_rect = {16, 82, 20, 20 };
static const SDL_Rect stop_rect = {82, 82, 20, 20};
static const SDL_Rect pause_rect = { 60, 82, 20, 20 };
static const SDL_Rect resume_rect = {37, 82, 20, 20};
static const SDL_Rect next_rect = { 105, 82, 20, 20 };
static const SDL_Rect pre_rect = { 16, 82, 20, 20 };
#define WINDOW_WIDTH 275
#define WINDOW_HEIGHT 116
#define MAX_SONGS 100
static char* files_path[MAX_SONGS];
static UINT32 song_index = 0;
// Helper: check if file ends with .wav (case-insensitive)
int has_wav_extension(const char* filename) {
    const char* ext = strrchr(filename, '.');
#ifdef _WIN32
    if (ext && (_stricmp(ext, ".wav") == 0 || _stricmp(ext, ".WAV") == 0)) {
#else
    if (ext && (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".WAV") == 0)) {
#endif
        return 1;
    }
    return 0;
}

// Main function: list .wav files
int list_wav_files(const char* directory, char*** file_list, int* count) {
    int capacity = 30;
    *file_list = malloc(capacity * sizeof(char*));
    if (*file_list == NULL) {
        return -1;
    }

    *count = 0;

#ifdef _WIN32
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*.wav", directory);

    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile(search_path, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if (has_wav_extension(findFileData.cFileName)) {
            if (*count >= capacity) {
                capacity *= 2;
                *file_list = realloc(*file_list, capacity * sizeof(char*));
            }
            (*file_list)[*count] = strdup(findFileData.cFileName);
            (*count)++;
        }
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);

#else
    DIR* dp = opendir(directory);
    if (dp == NULL) return -1;

    struct dirent* entry;
    while ((entry = readdir(dp)) != NULL) {
        if (has_wav_extension(entry->d_name)) {
            if (*count >= capacity) {
                capacity *= 2;
                *file_list = realloc(*file_list, capacity * sizeof(char*));
            }
            (*file_list)[*count] = strdup(entry->d_name);
            (*count)++;
        }
    }
    closedir(dp);
#endif

    return 0;
}

SDL_Texture *load_texture(const char *fname)
{
    SDL_Surface *surface = SDL_LoadBMP(fname);
    if (!surface) {
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    return texture;  // MAY BE NULL.
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Surface *surface = NULL;

    char *bmp_path = NULL;
    char* wav_path = NULL;

    SDL_SetAppMetadata("Example Renderer Read Pixels", "1.0", "com.example.renderer-read-pixels");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Audio Player", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Load the .wav file from wherever the app is being run from. */
    SDL_asprintf(&wav_path, "%smusic", SDL_GetBasePath());  /* allocate a string of the full file path */
    char** files;
    int count;


    if (list_wav_files(wav_path, &files, &count) == 0) {
        for (int i = 0; i < count; i++) {
            
            SDL_asprintf(&files_path[i], "%s/%s", wav_path, files[i]);
            SDL_Log("Found: %s\n", files_path[i]);
            free(files[i]);
        }
        free(files);
    }
    else {
        SDL_Log("Error listing .wav files\n");
    }

    if (!SDL_LoadWAV(*files_path, &spec, &wav_data, &wav_data_len)) { // Load first file
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }    
    SDL_free(wav_path);  /* done with this string. */

    /* Create our audio stream in the same format as the .wav file. It'll convert to what the audio hardware wants. */
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(stream);

    /* Textures are pixel data that we upload to the video hardware for fast drawing. Lots of 2D
       engines refer to these as "sprites." We'll do a static texture (upload once, draw many
       times) with data from a bitmap file. */

    /* SDL_Surface is pixel data the CPU can access. SDL_Texture is pixel data the GPU can access.
       Load a .bmp into a surface, move it to a texture from there. */
    SDL_asprintf(&bmp_path, "%sdisk.bmp", SDL_GetBasePath());  /* allocate a string of the full file path */
    surface = SDL_LoadBMP(bmp_path);
    if (!surface) {
        SDL_Log("Couldn't load bitmap: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_free(bmp_path);  /* done with this, the file is loaded. */
    /* This is also expensive, but easier: convert the pixels to a format we want. */
    if (surface && (surface->format != SDL_PIXELFORMAT_RGBA8888) && (surface->format != SDL_PIXELFORMAT_BGRA8888)) {
        SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
        SDL_DestroySurface(surface);
        surface = converted;
    }
    int x, y, cx, cy;
    cx = surface->w / 2;
    cy = surface->h / 2;
    for (y = 0; y < surface->h; y++) {
        Uint32 *pixels = (Uint32 *)(((Uint8 *)surface->pixels) + (y * surface->pitch));
        for (x = 0; x < surface->w; x++) {
            Uint8 *p = (Uint8 *)(&pixels[x]);
            int dx = x - cx;
            int dy = y - cy;
            double distance = sqrt(dx * dx + dy * dy);
            if (distance > 180) {
                p[1] = p[2] = p[3] = 0x0;
            }
        }
    }
    texture_width = surface->w;
    texture_height = surface->h;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Couldn't create static texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface);  /* done with this, the texture has a copy of the pixels now. */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
        case SDLK_SPACE:
            if (IsPause) {
                SDL_ResumeAudioStreamDevice(stream);
                IsPause = false;
            }
            else {
                SDL_PauseAudioStreamDevice(stream);
                IsPause = true;
            }
            break;
        default:
            SDL_Log("Some other key pressed: %c", event->key.key);
            break;
        }
    }
    if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
            const SDL_Point pt = { event->button.x, event->button.y };
            if (SDL_PointInRect(&pt, &pause_rect)) {  // pressed the "pause" button
                SDL_PauseAudioStreamDevice(stream);
                IsPause = true;
            } else if (SDL_PointInRect(&pt, &stop_rect)) {  // pressed the "stop" button
                // Implement stop functionality
                if (stream != NULL) { // Ensure the stream exists before destroying
                    SDL_DestroyAudioStream(stream);
                    stream = NULL; // Set stream to NULL to avoid dangling pointer
                }
                // You might also want to reset playback position, clear queues, etc.
                IsPause = false; // If stopped, it's definitely not paused
            }
            else if (SDL_PointInRect(&pt, &resume_rect)) { // pressed the "resume" botton
                SDL_ResumeAudioStreamDevice(stream);
                IsPause = false;
            }
            else if (SDL_PointInRect(&pt, &next_rect)) 
            {
                // Next song
                SDL_AudioDeviceID devid = SDL_GetAudioStreamDevice(stream);
                SDL_DestroyAudioStream(stream);
                SDL_CloseAudioDevice(devid);
                if (!SDL_LoadWAV(files_path[++song_index], &spec, &wav_data, &wav_data_len)) { // Load first file
                    SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
                    return SDL_APP_FAILURE;
                }

                /* Create our audio stream in the same format as the .wav file. It'll convert to what the audio hardware wants. */
                stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
                if (!stream) {
                    SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
                    return SDL_APP_FAILURE;
                }
                /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
                SDL_ResumeAudioStreamDevice(stream);
            }
            else if (SDL_PointInRect(&pt, &pre_rect))
            {
                // Previous Song
                SDL_AudioDeviceID devid = SDL_GetAudioStreamDevice(stream);
                SDL_DestroyAudioStream(stream);
                SDL_CloseAudioDevice(devid);
                if (!SDL_LoadWAV(files_path[--song_index], &spec, &wav_data, &wav_data_len)) { // Load first file
                    SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
                    return SDL_APP_FAILURE;
                }

                /* Create our audio stream in the same format as the .wav file. It'll convert to what the audio hardware wants. */
                stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
                if (!stream) {
                    SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
                    return SDL_APP_FAILURE;
                }
                /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
                SDL_ResumeAudioStreamDevice(stream);
            }
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const Uint64 now = SDL_GetTicks();
    SDL_Surface *surface;
    SDL_FPoint center;
    SDL_FRect dst_rect;

    /* we'll have a texture rotate around over 2 seconds (2000 milliseconds). 360 degrees in a circle! */
    if (!IsPause)
    {
        rotation = (((float)((int)(now % 2000))) / 2000.0f) * 360.0f;
    }

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  /* black, full alpha */
    SDL_RenderClear(renderer);  /* start with a blank canvas. */

    /* Center this one, and draw it with some rotation so it spins! */
    // dst_rect.x = ((float) (WINDOW_WIDTH - texture_width)) / 2.0f;
    // dst_rect.y = ((float) (WINDOW_HEIGHT - texture_height)) / 2.0f;
    // dst_rect.w = (float) texture_width;
    // dst_rect.h = (float) texture_height;
    /* rotate it around the center of the texture; you can rotate it from a different point, too! */
    // center.x = texture_width / 2.0f;
    // center.y = texture_height / 2.0f;
    // SDL_RenderTextureRotated(renderer, texture, NULL, &dst_rect, rotation, &center, SDL_FLIP_NONE);

    /* see if we need to feed the audio stream more data yet.
       We're being lazy here, but if there's less than the entire wav file left to play,
       just shove a whole copy of it into the queue, so we always have _tons_ of
       data queued for playback. */
    if (SDL_GetAudioStreamAvailable(stream) < (int)wav_data_len) {
        /* feed more data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
        SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
    }
    char *skin_path = NULL;
    SDL_asprintf(&skin_path, "%sMAIN.bmp", SDL_GetBasePath());  
    SDL_Texture *skin_main = load_texture(skin_path);
    SDL_free(skin_path);  /* done with this string. */
    if (!skin_main) {
        SDL_Log("Failed to load skin main.bmp", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_RenderTexture(renderer, skin_main, NULL, NULL);
    SDL_RenderPresent(renderer);  /* put it all on the screen! */
    SDL_DestroyTexture(skin_main);
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_free(wav_data);  /* strictly speaking, this isn't necessary because the process is ending, but it's good policy. */
    SDL_DestroyTexture(converted_texture);
    SDL_DestroyTexture(texture);
    /* SDL will clean up the window/renderer for us. */
}

