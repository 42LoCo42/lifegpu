#include <SDL3/SDL.h>
#include <wayland-client.h>

void initLayerShell(
	SDL_Window*        window,
	struct wl_display* display,
	struct wl_surface* surface
);

#if __INCLUDE_LEVEL__ == 0 /////////////////////////////////////////////////////

#include "util.c"

#include <wlr-layer-shell-unstable-v1.h>

#define WL_OBJECTS               \
	X(wl_compositor, compositor) \
	X(zwlr_layer_shell_v1, layerShell)

#define X(obj, var) static struct obj* var;
WL_OBJECTS
#undef X

static void noop(void) {}

static void registryGlobal(
	void*,
	struct wl_registry* registry,
	uint32_t            name,
	const char*         interface,
	uint32_t            version
) {
#define X(obj, var)                                                        \
	if(strcmp(interface, obj##_interface.name) == 0) {                     \
		var = wl_registry_bind(registry, name, &obj##_interface, version); \
		return;                                                            \
	}
	WL_OBJECTS
#undef X
}

static void layerSurfaceConfigure(
	void*                         data,
	struct zwlr_layer_surface_v1* layerSurface,
	uint32_t                      serial,
	uint32_t                      width,
	uint32_t                      height
) {
	zwlr_layer_surface_v1_ack_configure(layerSurface, serial);
	SDL_SetWindowSize(data, width, height);
}

void initLayerShell(
	SDL_Window*        window,
	struct wl_display* display,
	struct wl_surface* surface
) {
	struct wl_registry* registry = wl_display_get_registry(display);
	if(!registry)
		die("failed to get wayland registry");

	struct wl_registry_listener registryListener = {
		.global        = registryGlobal,
		.global_remove = (void (*)(void*, struct wl_registry*, uint32_t)) noop,
	};

	wl_registry_add_listener(registry, &registryListener, NULL);
	wl_display_roundtrip(display);

	struct wl_region* region = wl_compositor_create_region(compositor);
	wl_region_add(region, 0, 0, 0, 0);
	wl_surface_set_input_region(surface, region);
	wl_region_destroy(region);

	struct zwlr_layer_surface_v1* layerSurface = zwlr_layer_shell_v1_get_layer_surface(
		layerShell,
		surface,
		NULL,
		ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
		"lifegpu"
	);
	if(!layerSurface) die("failed to get layer surface");

	zwlr_layer_surface_v1_set_anchor(
		layerSurface,
		0                                         //
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   //
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT  //
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    //
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM //
	);

	struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
		.configure = layerSurfaceConfigure,
	};

	zwlr_layer_surface_v1_add_listener(layerSurface, &layerSurfaceListener, window);

	wl_surface_commit(surface);
}

#endif
