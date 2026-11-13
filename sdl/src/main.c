#include "renderer.c"
#include "wayland.c"

int main(int argc, char** argv) {
	Renderer r = {
		.zoom = 1,
		.ps   = PS_RUNNING,
	};

	uint64_t desiredFrameNS = 0;

	while(argc > 1) {
		const char* arg = shift();

		if(strcmp(arg, "-b") == 0 || strcmp(arg, "--background") == 0) {
			r.rm = RM_BACKGROUND;
		} else if(strcmp(arg, "-f") == 0 || strcmp(arg, "--frametime") == 0) {
			desiredFrameNS = atol(shift()) * 1e6;
		} else if(strcmp(arg, "-s") == 0 || strcmp(arg, "--screensaver") == 0) {
			r.rm = RM_SCREENSAVER;
		} else if(strcmp(arg, "-z") == 0 || strcmp(arg, "--zoom") == 0) {
			r.zoom = atof(shift());
		} else if(strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			printUsage(false);
		} else {
			fprintf(stderr, "Unknown option '%s'!\n", arg);
			printUsage(true);
		}
	}

	if(!SDL_Init(SDL_INIT_VIDEO)) dieSDL("failed to init SDL");

	if(!(r.device = SDL_CreateGPUDevice(shaderFormat, false, NULL))) dieSDL("failed to get GPU");

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
	if(r.rm == RM_BACKGROUND) SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN, true);

	if(!(r.window = SDL_CreateWindowWithProperties(props))) dieSDL("failed to create window");

	if(r.rm == RM_BACKGROUND) {
		props                      = SDL_GetWindowProperties(r.window);
		struct wl_display* display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
		struct wl_surface* surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
		initLayerShell(r.window, display, surface);
	}

	if(!SDL_ClaimWindowForGPUDevice(r.device, r.window)) dieSDL("failed to claim window for GPU");

	bool running = true;
	while(running) {
		uint64_t frameBeg = SDL_GetTicksNS();

		SDL_Event e = {0};
		while(SDL_PollEvent((&e))) {
			switch(e.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;

			case SDL_EVENT_WINDOW_RESIZED:
				rendererInit(&r);
				break;

			case SDL_EVENT_KEY_DOWN:
				if(r.rm == RM_SCREENSAVER) {
					running = false;
					break;
				}

				switch(e.key.key) {
				case SDLK_SPACE:
					r.ps = PS_STEP;
					break;

				case SDLK_P:
					r.ps = r.ps == PS_PAUSED ? PS_RUNNING : PS_PAUSED;
					break;

				case SDLK_R:
					rendererReset(&r);
					break;

				case SDLK_Q:
					running = false;
					break;
				}
				break;
			}
		}

		rendererRun(&r);

		uint64_t frameEnd  = SDL_GetTicksNS();
		uint64_t frameTime = frameEnd - frameBeg;
		if(desiredFrameNS > frameTime) SDL_DelayPrecise(desiredFrameNS - frameTime);
	}
}
