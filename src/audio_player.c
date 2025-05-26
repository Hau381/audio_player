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
#include "SDL3_ttf/SDL_ttf.h"
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
static Uint8* wav_data = NULL;
static Uint32 wav_data_len = 0;
static bool IsPause = false;
static char* wav_path = NULL;
float rotation = 0;
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Surface *surface = NULL;
    SDL_AudioSpec spec;
    char *bmp_path = NULL;

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
    SDL_asprintf(&wav_path, "%sfile_example_WAV_1MG.wav", SDL_GetBasePath());  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &spec, &wav_data, &wav_data_len)) {
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
    
    SDL_Log("Audio path: %s", wav_path);
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
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const Uint64 now = SDL_GetTicks();
    SDL_Surface *surface;
    SDL_FPoint center;
    SDL_FRect dst_rect, dst_rect1;
    TTF_Init();
    /* we'll have a texture rotate around over 2 seconds (2000 milliseconds). 360 degrees in a circle! */
    if (!IsPause)
    {
        rotation = (((float)((int)(now % 2000))) / 2000.0f) * 360.0f;
    }

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  /* black, full alpha */
    SDL_RenderClear(renderer);  /* start with a blank canvas. */

    /* Center this one, and draw it with some rotation so it spins! */
    dst_rect.x = ((float) (WINDOW_WIDTH - texture_width)) / 2.0f;
    dst_rect.y = ((float) (WINDOW_HEIGHT - texture_height)) / 2.0f;
    dst_rect.w = (float) texture_width;
    dst_rect.h = (float) texture_height;
    /* rotate it around the center of the texture; you can rotate it from a different point, too! */
    center.x = texture_width / 2.0f;
    center.y = texture_height / 2.0f;
    SDL_RenderTextureRotated(renderer, texture, NULL, &dst_rect, rotation, &center, SDL_FLIP_NONE);
    
    // Load font
    char* font_path = NULL;
    SDL_asprintf(&font_path, "%sarial.ttf", SDL_GetBasePath());  /* allocate a string of the full file path */
        // Load Font
    TTF_Font* font = TTF_OpenFont(font_path, 16);
    if (!font) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return -1;
    }
    const char text[13] = "Hello SDL3 !";
    SDL_Color fg = { 255, 255, 255, 255 };
    SDL_Color bg = { 0, 0, 0, 255 };
    SDL_Surface* textSurface = TTF_RenderText_LCD(font, text, strlen(text), fg, bg);

    // Convert the combined surface to a texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_DestroySurface(textSurface); // Free the surface after conversion

    // Render the combined text texture
    dst_rect1.x = ((float)(WINDOW_WIDTH - 10)) / 2.0f;
    dst_rect1.y = 0;
    dst_rect1.w = 100;
    dst_rect1.h = 50;
    SDL_RenderTexture(renderer, texture, NULL, &dst_rect1);
    SDL_free(font_path);  /* done with this, the file is loaded. */


    /* see if we need to feed the audio stream more data yet.
       We're being lazy here, but if there's less than the entire wav file left to play,
       just shove a whole copy of it into the queue, so we always have _tons_ of
       data queued for playback. */
    if (SDL_GetAudioStreamAvailable(stream) < (int)wav_data_len) {
        /* feed more data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
        SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
    }

    SDL_RenderPresent(renderer);  /* put it all on the screen! */
    TTF_Quit();
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_free(wav_data);  /* strictly speaking, this isn't necessary because the process is ending, but it's good policy. */
    SDL_free(wav_path);  /* done with this string. */
    SDL_DestroyTexture(converted_texture);
    SDL_DestroyTexture(texture);
    /* SDL will clean up the window/renderer for us. */
}

