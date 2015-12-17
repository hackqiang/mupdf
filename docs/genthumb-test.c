#include <mupdf/fitz.h>


int main(int argc, char **argv)
{
	char *inputfile;
	fz_context *ctx;
	fz_document *doc;

	if (argc < 2)
	{
		fprintf(stderr, "usage: ./genthumb-test pdf_file\n");
		return EXIT_FAILURE;
	}
	inputfile = argv[1];
    
	/* Create a context to hold the exception stack and various caches. */
	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return EXIT_FAILURE;
	}
    
    //fz_set_aa_level(ctx, 8);

	/* Register the default file types to handle. */
	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	/* Open the document. */
	fz_try(ctx)
		doc = fz_open_document(ctx, inputfile);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot open document: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}
    
    int page_count = fz_count_pages(ctx, doc);
    int i;
    for(i=1; i<=page_count; i++)
    {
        char out[256] = {0};
        sprintf(out, "%s-%d.png", inputfile, i);
        fz_gen_thumb_image(ctx, doc, i, 400, out);
    }
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return 0;
}