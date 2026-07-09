#include <plutosvg.h>

#include <stdio.h>

int main(void)
{
    plutosvg_document_t* document = NULL;
    plutovg_surface_t* surface = NULL;

    fprintf(stdout, "Loading 'camera.svg'\n");
    document = plutosvg_document_load_from_file("camera.svg", -1, -1);
    if(document == NULL) {
        fprintf(stderr, "Unable to load 'camera.svg'\n");
        goto cleanup;
    }

    fprintf(stdout, "Rendering 'camera.svg'\n");
    surface = plutosvg_document_render_to_surface(document, NULL, -1, -1, NULL, NULL, NULL);
    if(surface == NULL) {
        fprintf(stderr, "Unable to render 'camera.svg'\n");
        goto cleanup;
    }

    fprintf(stdout, "Writing 'camera.png'\n");
    if(!plutovg_surface_write_to_png(surface, "camera.png")) {
        fprintf(stderr, "Unable to write 'camera.png'\n");
        goto cleanup;
    }

cleanup:
    plutovg_surface_destroy(surface);
    plutosvg_document_destroy(document);
    return 0;
}
