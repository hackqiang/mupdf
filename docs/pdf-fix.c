#include <mupdf/fitz.h>
#include <mupdf/pdf.h>


static int write_unicode(int fd, int *unicode)
{
    char t;
    int pos;
    int len = 0;
    while(*(unicode+len)) len++;
    
    //spec: Text String Type need UTF-16BE
    t = 0xfe;
    write(fd, &t, 1);
    t = 0xff;
    write(fd, &t, 1);
    
    for (pos = 0; pos < len; pos++)
    {
        if(unicode[pos]<0x10000)
        {
            t = (unicode[pos] >> 8) & 0xff;
            write(fd, &t, 1);
            t = (unicode[pos]) & 0xff;
            write(fd, &t, 1);
        }
        else
        {
            //not verify yet
            int vh = ((unicode[pos] - 0x10000) & 0xFFC00) >> 10 ;
            int vl =  (unicode[pos] - 0x10000) & 0x3ff;
            short h  =  0xD800 | vh;
            short l =  0xDC00 | vl;
            write(fd, &h, 2); 
            write(fd, &l, 2); 
        }
    }
    
    return 0;
}

static void print_unicode(int *unicode, int len)
{
    int pos;
    if(len==0)
        while(*(unicode+len)) len++;
    
    for (pos = 0; pos < len && unicode[pos]; pos++)
    {
        char temp[4] = {0};
        fz_runetochar(temp,unicode[pos]);
        printf("%s", temp);
    }
}

//todo: fix later
// size < MAX_KEYWORD_LEN
static int uni[128] = {0};
static int *utf8_to_unicode(char *utf8)
{
    int *unicode = uni;
    memset(unicode, 0, 128);
    while (*utf8)
    {
        utf8 += fz_chartorune(unicode++, (char *)utf8);
    }
    return uni;
}

//TODO: fix 222.pdf
int write_new_pdf(char *inputfile, char *outputfile, fz_context *ctx, fz_document *doc, fz_outline *outline, char *title, char *author)
{
    char c;
    int outlinesflag = 0; //0: no outlines; 1:has outlines; 2:add outline objects finish;
    int infoflag = 0;   //0: no info; 1:has info; 2:add info objects finish;
    
    int objn = pdf_count_objects(ctx, (pdf_document *)doc);
    
    int infoobj, outlineobj;

    
	int fd_in = open(inputfile, O_RDONLY);
	if(fd_in < 0)
	{
		printf("open %s error\n", inputfile);
		return -1;
	}

	int fd_out = open(outputfile, O_RDWR | O_CREAT);
	if(fd_out < 0)
	{
		printf("open %s error\n", outputfile);
		return -1;
	} 

    int total_outlines = 0;
    fz_outline *p = outline;
    while(p)
    {
        total_outlines++;
        p = p->next;
        outlinesflag = 1;
        outlineobj = objn;
    }
    
    if(title && author)
    {
        infoflag = 1;
        infoobj = objn;
        outlineobj = objn + 1;
    }

    printf("outlinesflag %d\ninfoflag %d\n", outlinesflag, infoflag);
    
    #define WBUF_SIZE 2014
	char wbuf[WBUF_SIZE];
	while(read(fd_in, &c, 1)==1)
	{
        //get /Catalog 0x20 [0x0d]
        if(c == 'C' && outlinesflag)
        {
            write(fd_out, &c, 1);
            
            char buf[6] = {0};
            if(read(fd_in, buf, 6)!=6)
            {
                printf("read error\n");
                return -1;
            }
            if(!memcmp(buf, "atalog", 6))
            {
                write(fd_out, buf, 6);
                //1
                printf("got Catalog\n");
                memset(wbuf,0,WBUF_SIZE);
                sprintf(wbuf, "\n/Outlines %d 0 R \n", outlineobj);
                write(fd_out, wbuf, strlen(wbuf));
                write(fd_out, "/PageMode /UseOutlines\n", strlen("/PageMode /UseOutlines\n"));
            }
            else
            {
                lseek(fd_in, -6, SEEK_CUR);
            }
        }
        else if (c == 't' && infoflag)
        {
            write(fd_out, &c, 1);
            
            char buf[6] = {0};
            if(read(fd_in, buf, 6)!=6)
            {
                printf("read error\n");
                return -1;
            }
            if(!memcmp(buf, "railer", 6))
            {
                write(fd_out, buf, 6);
                //1
                printf("got trailer\n");
                
                while(read(fd_in, &c, 1)==1)
                {
                    if (c == '<')
                    {
                        if(read(fd_in, &c, 1) == 1)
                        {
                            if(c == '<')
                            {
                                memset(wbuf,0,WBUF_SIZE);
                                sprintf(wbuf, "<<\n/Info %d 0 R \n", infoobj);
                                write(fd_out, wbuf, strlen(wbuf));
                                break;
                            }
                            else
                            {
                                write(fd_out, &c, 1);
                            }
                        }
                        else
                        {
                            printf("read error\n");
                            return -1;
                        }
                    }
                    else
                    {
                        write(fd_out, &c, 1);
                    }
                    
                }
            }
            else
            {
                lseek(fd_in, -6, SEEK_CUR);
            }
        }
        else if (c == 'e')
        {              
            write(fd_out, &c, 1);
            char buf[6] = {0};
            if(read(fd_in, buf, 6)!=6)
            {
                printf("read error\n");
                return -1;
            }

            if(!memcmp(buf, "ndobj", 5))
            {
                write(fd_out, buf, 6);
                
                if (outlinesflag == 1)
                {
                    printf("outlinesflag\n");
                    //2
                    memset(wbuf,0,WBUF_SIZE);
                    sprintf(wbuf, "\n%d 0 obj \n", outlineobj);
                    write(fd_out, wbuf, strlen(wbuf));

                    memset(wbuf,0,WBUF_SIZE);
                    sprintf(wbuf, "<<\n/Count %d \n/First %d 0 R \n/Last %d 0 R\n>>\nendobj \n", total_outlines, outlineobj+1, outlineobj+total_outlines);
                    printf("%s",wbuf);
                    write(fd_out, wbuf, strlen(wbuf));

                    //3
                    int index = 1;
                    p = outline;
                    while(p) 
                    {
                        memset(wbuf,0,WBUF_SIZE);
                        sprintf(wbuf, "%d 0 obj \n", outlineobj+index);
                        write(fd_out, wbuf, strlen(wbuf));

                        write(fd_out, "<<\n/Title (", strlen("<<\n/Title ("));

                        //printf("add (");
                        //print_unicode(utf8_to_unicode(p->title),0);
                        //printf("<-->%d)\n", p->dest.ld.gotor.page);
                        //write unicode of text
                        write_unicode(fd_out, utf8_to_unicode(p->title));

                        memset(wbuf,0,WBUF_SIZE);
                        if(index == 1)
                        {
                            sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n>>\nendobj\n", 
                                            p->dest.ld.gotor.page, outlineobj, outlineobj+index+1);
                        }
                        else if(index == total_outlines)
                        {
                            sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                            p->dest.ld.gotor.page, outlineobj, outlineobj+index-1);
                        }
                        else
                        {
                            sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                            p->dest.ld.gotor.page, outlineobj, outlineobj+index+1, outlineobj+index-1);
                        }
                        write(fd_out, wbuf, strlen(wbuf));

                        p = p->next;
                        index++;
                    }
                    outlinesflag = 2;
                }
                
                if (infoflag == 1)
                {
                    printf("infoflag\n");
                   
                    memset(wbuf,0,WBUF_SIZE);
                    sprintf(wbuf, "\n%d 0 obj \n<<\n/Author (%s)\n/Title (%s)\n>>\nendobj \n", infoobj, author, title);
                    write(fd_out, wbuf, strlen(wbuf));
                    infoflag = 2;
                }
                
            }
            else
            {
                lseek(fd_in, -6, SEEK_CUR);
            }
        }
        else
        {
            write(fd_out, &c, 1);
        }
	}

	close(fd_in);
	close(fd_out);
    return 0;
}



int main(int argc, char **argv)
{
	char *inputfile, *outputfile, *infofile = NULL;
	fz_context *ctx;
	fz_document *doc;
    pdf_obj *obj;

	if(argc < 3)
	{
		fprintf(stderr, "usage: build/debug/pdf-fix input_pdf output_pdf [info.json]\n");
		return EXIT_FAILURE;
	}
	inputfile = argv[1];
    outputfile = argv[2];
    if(argc == 4)
        infofile = argv[3];
    
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
    
    char *title = NULL, *author = NULL;
	obj = pdf_dict_get(ctx, pdf_trailer(ctx, (pdf_document *)doc), PDF_NAME_Info);
	if (obj)
	{
		printf("Info object (%d %d R):\n", pdf_to_num(ctx, obj), pdf_to_gen(ctx, obj));
	}
    else
    {
        //no Info
        //1. read form info.txt
        title = "test title";
        author = "test author";
        //2. parse form pdf file
        
    }
    
    fz_outline *outline = pdf_load_outline_origin(ctx, doc);
    if(!outline)
    {
        outline = pdf_load_outline_fixed(ctx, doc);
    }
    else
    {
        //already has outline objects, no need fix
        outline = NULL;
        
        //alread has /Info
        if(obj)
        {
            printf("both outline and /Info exist, skip fix\n");
            return 0;
        }
    }
    
    write_new_pdf(inputfile, outputfile, ctx, doc, outline, title, author);
    
    fz_drop_outline(ctx, outline);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return 0;
}