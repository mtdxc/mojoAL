/**
 * MojoAL; a simple drop-in OpenAL implementation.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

/* This is just test code, you don't need to compile this with MojoAL. */

#include <stdio.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "SDL.h"
#include <vector>
#include <Windows.h>
//#pragma comment(lib, "OpenAL32.lib")
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
bool setfile(int idx, const char* fname);
bool addfile(const char* fname);
static int check_openal_error(const char *where)
{
    const ALenum err = alGetError();
    if (err != AL_NONE) {
        printf("OpenAL Error at %s! %s (%u)\n", where, alGetString(err), (unsigned int) err);
        return 1;
    }
    return 0;
}

static ALenum get_openal_format(const SDL_AudioSpec *spec)
{
    if ((spec->channels == 1) && (spec->format == AUDIO_U8)) {
        return AL_FORMAT_MONO8;
    } else if ((spec->channels == 1) && (spec->format == AUDIO_S16SYS)) {
        return AL_FORMAT_MONO16;
    } else if ((spec->channels == 2) && (spec->format == AUDIO_U8)) {
        return AL_FORMAT_STEREO8;
    } else if ((spec->channels == 2) && (spec->format == AUDIO_S16SYS)) {
        return AL_FORMAT_STEREO16;
    } else if ((spec->channels == 1) && (spec->format == AUDIO_F32SYS)) {
        return alIsExtensionPresent("AL_EXT_FLOAT32") ? alGetEnumValue("AL_FORMAT_MONO_FLOAT32") : AL_NONE;
    } else if ((spec->channels == 2) && (spec->format == AUDIO_F32SYS)) {
        return alIsExtensionPresent("AL_EXT_FLOAT32") ? alGetEnumValue("AL_FORMAT_STEREO_FLOAT32") : AL_NONE;
    }
    return AL_NONE;
}

struct obj
{
    ALuint sid = 0;
    ALuint bid = 0;
    char fname[20] = { 0 };
    int x = 400;
    int y = 50;
};

/* !!! FIXME: eventually, add more sources and sounds. */
static std::vector<obj> objects;  /* one listener, one source. */
static int selobj = -1;
static bool draging = false;
static int obj_under_mouse(const int x, const int y)
{
    const SDL_Point p = { x, y };
    int i;
    for (i = 0; i < objects.size(); i++) {
        const obj& o = objects[i];
        const SDL_Rect r = { o.x - 25, o.y - 25, 50, 50 };
        if (SDL_PointInRect(&p, &r)) {
            return i;
        }
    }
    return -1;
}

static int mainloop(SDL_Renderer *renderer)
{
    int i;
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                return 0;

            case SDL_KEYDOWN:
                switch (e.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    return 0;
                case SDLK_BACKSPACE:
                    if (selobj > 0) {
                        obj o = objects[selobj];
                        alDeleteSources(1, &o.sid);
                        check_openal_error("alDeleteSources");
                        alDeleteBuffers(1, &o.bid);
                        check_openal_error("alDeleteBuffers");
                        objects.erase(objects.begin() + selobj);
                    }
                    break;
                default:
                    printf("user press %c\n", e.key.keysym.sym);
                    break;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                draging = false;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    if (e.button.state == SDL_RELEASED) {
                        //if (palTraceMessage) palTraceMessage("Mouse button released");
                        selobj = -1;
                    } else {
                        //if (palTraceMessage) palTraceMessage("Mouse button pressed");
                        selobj = obj_under_mouse(e.button.x, e.button.y);
                        draging = selobj != -1;
                    }
                    break;
                }
                if (e.button.button == SDL_BUTTON_RIGHT && e.type == SDL_MOUSEBUTTONDOWN) {
                    selobj = obj_under_mouse(e.button.x, e.button.y);
                    if (selobj == -1) break;
                    OPENFILENAMEA ofn;       // common dialog box structure
                    char szFile[260];       // buffer for file name
                    HWND hwnd;              // owner window
                    HANDLE hf;              // file handle

                    // Initialize OPENFILENAME
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = NULL;
                    ofn.lpstrFile = szFile;
                    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
                    // use the contents of szFile to initialize itself.
                    ofn.lpstrFile[0] = '\0';
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "All\0*.*\0Wav\0*.wav\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    // Display the Open dialog box. 

                    if (GetOpenFileNameA(&ofn) == TRUE) {
                        //::MessageBoxA(NULL, szFile, "select file", MB_OK);
                        if (selobj)
                            setfile(selobj, szFile);
                        else
                            addfile(szFile);
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                if (draging) {
                    obj *o = &objects[selobj];
                    o->x = SDL_min(800, SDL_max(0, e.motion.x));
                    o->y = SDL_min(600, SDL_max(0, e.motion.y));
                    /* we are treating the 2D view as the X and Z coordinate, as if we're looking at it from above.
                       From this configuration, the Y coordinate would be depth, and we leave that at zero.
                       the listener's default "at" orientation is towards the north in this configuration, with its
                       "up" pointing at the camera. Since we are rendering the audio in relation to a listener we
                       move around in 2D space in the camera's view, it's obviously detached from the camera itself. */
                    if (o->sid == 0) {  /* it's the listener. */
                        alListener3f(AL_POSITION, ((o->x / 400.0f) - 1.0f) * 10.0f, 0.0f, ((o->y / 300.0f) - 1.0f) * 10.0f);
                    } else {
                        alSource3f(o->sid, AL_POSITION, ((o->x / 400.0f) - 1.0f) * 10.0f, 0.0f, ((o->y / 300.0f) - 1.0f) * 10.0f);
                    }
                }
                break;
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_RenderClear(renderer);

    for (i = 0; i < objects.size(); i++) {
        const obj *o = &objects[i];
        const SDL_Rect r = { o->x - 25, o->y - 25, 50, 50 };
        if (o->sid == 0) {
            SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
        } else {
            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xFF, 0xFF);
        }
        if (selobj == i) {
            SDL_RenderDrawRect(renderer, &r);
        }
        else {
            SDL_RenderFillRect(renderer, &r);
        }
    }

    SDL_RenderPresent(renderer);

    return 1;
}


#ifdef __EMSCRIPTEN__
static void emscriptenMainloop(void *arg)
{
    (void) mainloop((SDL_Renderer *) arg);
}
#endif

bool loadwav(ALuint bid, const char* fname) {
    SDL_AudioSpec spec;
    ALenum alfmt = AL_NONE;
    Uint8* buf = NULL;
    Uint32 buflen = 0;

    if (!SDL_LoadWAV(fname, &spec, &buf, &buflen)) {
        printf("Loading '%s' failed! %s\n", fname, SDL_GetError());
        return false;
    }
    else if ((alfmt = get_openal_format(&spec)) == AL_NONE) {
        printf("Can't queue '%s', format not supported by the AL.\n", fname);
        SDL_FreeWAV(buf);
        return false;
    }
    check_openal_error("loadwav");
    alBufferData(bid, alfmt, buf, buflen, spec.freq);
    SDL_FreeWAV(buf);
    check_openal_error("alBufferData");
    return true;
}

bool setfile(int idx, const char* fname) {
    obj& o = objects[idx];
    if (o.bid) {
        alSourceStop(o.sid);
    }

    if (!loadwav(o.bid, fname))
        return false;
    // set filename
    const char* p = strrchr(fname, '/');
    if (!p) {
        p = strrchr(fname, '\\');
    }
    if (p) strncpy_s(o.fname, p + 1, sizeof(o.fname));

    alSourcei(o.sid, AL_BUFFER, o.bid);
    check_openal_error("alSourcei");
    alSourcei(o.sid, AL_LOOPING, AL_TRUE);
    check_openal_error("alSourcei");
    alSourcePlay(o.sid);
    check_openal_error("alSourcePlay");

    alSource3f(o.sid, AL_POSITION, ((o.x / 400.0f) - 1.0f) * 10.0f, 0.0f, ((o.y / 300.0f) - 1.0f) * 10.0f);
    return true;
}

static bool addfile(const char* fname) {
    obj o;
    printf("Now queueing '%s'...\n", fname);
    alGenSources(1, &o.sid);
    if (check_openal_error("alGenSources")) {
        return false;
    }

    alGenBuffers(1, &o.bid);
    if (check_openal_error("alGenBuffers")) {
        alDeleteSources(1, &o.sid);
        check_openal_error("alDeleteSources");
        return false;
    }
    int idx = objects.size();
    objects.push_back(o);
    if (!setfile(idx, fname)) {
        alDeleteSources(1, &o.sid);
        check_openal_error("alDeleteSources");
        objects.pop_back();
        return false;
    }
    return true;
}

static void spatialize(SDL_Renderer *renderer)
{

    //if (palTracePopScope) palTracePopScope();

    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(emscriptenMainloop, renderer, 0, 1);
    #else
    while (mainloop(renderer)) { /* go again */ }
    #endif

    for (auto o : objects) {
        //alSourcei(sid, AL_BUFFER, 0);  /* force unqueueing */
        alDeleteSources(1, &o.sid);
        check_openal_error("alDeleteSources");
        alDeleteBuffers(1, &o.bid);
        check_openal_error("alDeleteBuffers");
    }
    objects.clear();
}

#undef main
int main(int argc, char **argv)
{
    ALCdevice *device;
    ALCcontext *context;
    SDL_Window *window;
    SDL_Renderer *renderer;
/*
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s [wavfile]\n", argv[0]);
        return 1;
    }
*/
    if (SDL_Init(SDL_INIT_VIDEO) == -1) {
        fprintf(stderr, "SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
        return 2;
    }

    window = SDL_CreateWindow(argv[0], SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 800, 600,
                              SDL_WINDOW_RESIZABLE);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow() failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 3;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer() failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }
    SDL_RenderSetLogicalSize(renderer, 800, 600);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    device = alcOpenDevice(NULL);
    if (!device)
    {
        printf("Couldn't open OpenAL default device.\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 5;
    }
    context = alcCreateContext(device, NULL);
    if (!context) {
        printf("Couldn't create OpenAL context.\n");
        alcCloseDevice(device);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 6;
    }

    alcMakeContextCurrent(context);

    obj o;
    /* the listener. */
    o.sid = 0;
    o.x = 400;
    o.y = 300;
    alListener3f(AL_POSITION, ((o.x / 400.0f) - 1.0f) * 10.0f, 0.0f, ((o.y / 300.0f) - 1.0f) * 10.0f);
    objects.push_back(o);
    for (size_t i = 1; i < argc; i++) {
        addfile(argv[i]);
    }

    spatialize(renderer);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Done!\n");
    return 0;
}

/* end of testposition.c ... */

