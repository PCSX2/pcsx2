#include <plutosvg.h>

#include <stdio.h>
#include <time.h>

static double elapsed_time(clock_t start, clock_t end)
{
    return ((double)(end - start)) / CLOCKS_PER_SEC;
}

int main(int argc, char* argv[])
{
    if(argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: svg2png input output [id]\n");
        return -1;
    }

    const char* input = argv[1];
    const char* output = argv[2];
    const char* id = NULL;
    if(argc == 4) {
        id = argv[3];
    }

    plutosvg_document_t* document = NULL;
    plutovg_surface_t* surface = NULL;
    clock_t start, end;

    start = clock();
    document = plutosvg_document_load_from_file(input, -1, -1);
    end = clock();

    if(document == NULL) {
        fprintf(stderr, "Unable to load '%s'\n", input);
        goto cleanup;
    }

    fprintf(stdout, "Finished loading '%s' in %.3f seconds\n", input, elapsed_time(start, end));

    start = clock();
    surface = plutosvg_document_render_to_surface(document, id, -1, -1, NULL, NULL, NULL);
    end = clock();

    if(surface == NULL) {
        fprintf(stderr, "Unable to render '%s'\n", input);
        goto cleanup;
    }

    fprintf(stdout, "Finished rendering '%s' in %.3f seconds\n", input, elapsed_time(start, end));

    start = clock();
    if(!plutovg_surface_write_to_png(surface, output)) {
        fprintf(stderr, "Unable to write '%s'\n", output);
        goto cleanup;
    }
    end = clock();

    fprintf(stdout, "Finished writing '%s' in %.3f seconds\n", output, elapsed_time(start, end));
cleanup:
    plutovg_surface_destroy(surface);
    plutosvg_document_destroy(document);
    return 0;
}
