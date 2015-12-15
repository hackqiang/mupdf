#include <mupdf/fitz.h>


int main(int argc, char **argv)
{
	char *inputfile;
	fz_context *ctx;
	fz_document *doc;

	if (argc < 2)
	{
		fprintf(stderr, "usage: ./cg inputfile_pdf_file\n");
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
    
    fz_outline *outline = fz_load_outline(ctx, doc);
    if(outline)
    {
        printf("get outline success\n");
        
        //for debug only, should not use.
        write_outline_to_new_pdf(inputfile, ctx, doc, outline);
        
        fz_drop_outline(ctx, outline);
    }

	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return 0;
}