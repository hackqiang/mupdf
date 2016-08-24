#include <string.h>

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#define MAX_KEYWORD_LEN 256

extern int *utf8_to_unicode(char *utf8, int *uni, int uni_len);
extern void print_unicode(int *unicode, int len);

static int write_unicode_as_UTF_16BE(int fd, int *unicode)
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

int parse_obj(char *buf)
{
    char *p = buf;
    if(strstr(buf, "obj"))
    {
        return atoi(buf);
    }
    return 0;
}

int get_outline_totals(fz_outline *outline)
{
    if(!outline)
        return 0;
    
    int total_outlines = 0;
    fz_outline *p = outline;
    while(p)
    {
        total_outlines++;
        fz_outline *q = p->down;
        while(q) {
            total_outlines++;
            q = q->next;
        }
        p = p->next;
    }
    
    return total_outlines;
}


int print_outline(fz_outline *outline)
{
    if(!outline)
        return 0;

    printf("outline:\n");
    
    fz_outline *p = outline;
    while(p)
    {
        printf("(%d)%s\n", p->refs, p->title);
        fz_outline *q = p->down;
        while(q) {
            printf("\t(%d)%s\n", q->refs,q->title);
            q = q->next;
            
        }
        p = p->next;
    }
    
    return 0;
}


int make_outline_index(fz_outline *outline)
{
    //reuse p->refs for store the obj index

    //printf("outline:\n");
    
    int index = 1;
    fz_outline *p = outline;
    while(p)
    {
        //printf("%s\n", p->title);
        p->refs = index;
        fz_outline *q = p->down;
        index++;
        while(q) {
            //printf("\t%s\n", q->title);
            q->refs = index;
            q = q->next;
            index++;
        }
        p = p->next;
    }
    
    return 0;
}

int get_last_index(fz_outline *outline)
{
    fz_outline *p = outline;
    while(p && p->next) 
    {
        p = p->next;
    }
    
    return p->refs;
}

int get_pre_index(fz_outline *outline, fz_outline *p)
{
    fz_outline *q = outline;
    fz_outline *pre = outline;
    while(q && q!=p) 
    {
        pre = q;
        q = q->next;
    }
    printf("pre%d\n", pre->refs);
    return pre->refs;
}

int get_next_index(fz_outline *outline, fz_outline *p)
{
    if(p->next)
        return p->next->refs;
    
    return 0;
}

int get_children_total(fz_outline *outline)
{
    fz_outline *p = outline->down;
    
    int n = 0;
    while(p) 
    {
        n++;
        p = p->next;
    }
    
    return n;
}

int get_first_child_index(fz_outline *outlines)
{
    return outlines->down->refs;
}

int get_last_child_index(fz_outline *outlines)
{
    fz_outline *p = outlines->down;

    while(p && p->next) 
    {
        p = p->next;
    }
    
    return p->refs;
}


fz_outline *load_outline(fz_context *ctx, const char *contentfile)
{
    

    FILE *f = fopen(contentfile, "r");
	if(!f)
	{
		printf("open %s error\n", contentfile);
		return -1;
	}
    
    char line[1024] = {0};
    char title[1024] = {0};
    int pagen;
    
    fz_outline *first_node = NULL;
    
    
    
    while(fgets(line, 1024, f))
    {

        //printf("%s", line);
        
        fz_outline *node = fz_new_outline(ctx);
        if(!node) {
            return NULL;
        }

        int children = 0;
        if(line[0] == ' ')
        {
            children = 1;
            //printf("children\n");
        }
        
        char *p = strstr(line, "<->");
        if(p)
        {
            memcpy(title, line, strlen(line) - strlen(p));
            pagen = atoi(p+3)-1;
            
            //printf("%s(%d)\n", title, pagen);
            
            node->title = (char *)malloc(strlen(title)+1);
            if(!node->title) {
                return NULL;
            }
            memset(node->title, 0, strlen(title)+1);
            
            memcpy(node->title, title, strlen(title)+1);
            node->dest.kind = FZ_LINK_GOTO;
            node->dest.ld.gotor.flags = fz_link_flag_fit_h | fz_link_flag_fit_v;
            node->dest.ld.gotor.page = pagen;
            node->dest.ld.gotor.dest = NULL;
            node->next = NULL;
            node->down = NULL;
            
            
            if(!first_node)
            {
                first_node = node;
            }
            else
            {
                fz_outline *p = first_node;
                while(p->next) p = p->next;
                //printf("x%s\n", p->title);
                if(children)
                {
                    if(p->down)
                    {
                        fz_outline *q = p->down;
                        while(q->next) q = q->next;
                        q->next = node;
                    }
                    else
                    {
                        p->down = node;
                    }
                    
                }
                else
                {
                    p->next = node;
                }
                
            }
        }
        
        memset(line, 0, 1024);
        memset(title, 0, 1024);
    }
        
    return first_node;
}



//TODO: fix 222.pdf
int write_new_pdf(char *inputfile, char *outputfile, fz_context *ctx, fz_document *doc, fz_outline *outline, int infoobj, char *title, char *author)
{
    char c;
    int outlinesflag = 0; //0: no outlines; 1:has outlines; 2:add outline objects finish;
    int infoflag = 0;   //0: no info; 1:has info; 2:add info objects finish;
    
    int objn = pdf_count_objects(ctx, (pdf_document *)doc);
    
    int outlineobj;

    int uni[MAX_KEYWORD_LEN] = {0};
    
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

    int total_parents = 0;
    int total_outlines = get_outline_totals(outline);
    fz_outline *p = outline;
    while(p)
    {
        total_parents++;
        p = p->next;
        outlinesflag = 1;
        outlineobj = objn;
    }
    
    
    if(*title && *author)
    {
        if(!infoobj)
        {
            infoobj = objn;
            outlineobj = objn + 1;
        }
        infoflag = 1;
    }

    printf("outlinesflag %d\ninfoflag %d\ninfoobj %d\n", outlinesflag, infoflag, infoobj);
    
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
        else if (c == 't' && infoflag && infoobj==objn)
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
            char buf[5] = {0};
            if(read(fd_in, buf, 5)!=5)
            {
                printf("read error\n");
                return -1;
            }

            if(!memcmp(buf, "ndobj", 5))
            {
                write(fd_out, buf, 5);
                
                //check next obj num
                int readn = read(fd_in, buf, 32);
                int currentobj = parse_obj(buf);
                lseek(fd_in, -readn, SEEK_CUR);
                //printf("n%d\n", currentobj);
                
                if (outlinesflag == 1)
                {
                    //printf("outlinesflag\n");
                    //2
                    memset(wbuf,0,WBUF_SIZE);
                    sprintf(wbuf, "\n%d 0 obj \n", outlineobj);
                    write(fd_out, wbuf, strlen(wbuf));

                    memset(wbuf,0,WBUF_SIZE);
                    sprintf(wbuf, "<<\n/Count %d \n/First %d 0 R \n/Last %d 0 R\n>>\nendobj \n", total_outlines, outlineobj+1, outlineobj+get_last_index(outline));
                    //printf("%s",wbuf);
                    write(fd_out, wbuf, strlen(wbuf));

                    //3
                    int index = 1;
                    int parent_index = 1;
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
                        write_unicode_as_UTF_16BE(fd_out, utf8_to_unicode(p->title, uni, MAX_KEYWORD_LEN));
                        
                        write(fd_out, ")\n", strlen(")\n"));
                        
                        int child_n = get_children_total(p);
                        if(child_n)
                        {
                            memset(wbuf,0,WBUF_SIZE);
                            sprintf(wbuf, "/Count %d \n/First %d 0 R \n/Last %d 0 R\n", child_n, outlineobj+get_first_child_index(p), outlineobj+get_last_child_index(p));
                            //printf("%s",wbuf);
                            write(fd_out, wbuf, strlen(wbuf));
                        }

                        memset(wbuf,0,WBUF_SIZE);
                        if(parent_index == 1)
                        {
                            sprintf(wbuf, "/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n>>\nendobj\n", 
                                            p->dest.ld.gotor.page, outlineobj, outlineobj+get_next_index(outline, p));
                        }
                        else if(parent_index == total_parents)
                        {
                            sprintf(wbuf, "/Dest [%d /Fit]\n/Parent %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                            p->dest.ld.gotor.page, outlineobj, outlineobj+get_pre_index(outline, p));
                        }
                        else
                        {
                            sprintf(wbuf, "/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                            p->dest.ld.gotor.page, outlineobj, outlineobj+get_next_index(outline, p), outlineobj+get_pre_index(outline, p));
                        }
                        //printf("%s",wbuf);
                        write(fd_out, wbuf, strlen(wbuf));
                        int parent_objn = outlineobj+index;
                        
                        index++;
                        
                        fz_outline *q = p->down;
                        int children_index = 1;
                        while(q)
                        {
                            memset(wbuf,0,WBUF_SIZE);
                            sprintf(wbuf, "%d 0 obj \n", outlineobj+index);
                            write(fd_out, wbuf, strlen(wbuf));

                            write(fd_out, "<<\n/Title (", strlen("<<\n/Title ("));

                            write_unicode_as_UTF_16BE(fd_out, utf8_to_unicode(q->title, uni, MAX_KEYWORD_LEN));
                            
                            //printf("1%s\n", q->title);
                            memset(wbuf,0,WBUF_SIZE);
                            if(children_index == 1)
                            {
                                sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n>>\nendobj\n", 
                                                q->dest.ld.gotor.page, parent_objn, outlineobj+index+1);
                            }
                            else if(children_index == child_n)
                            {
                                sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                                q->dest.ld.gotor.page, parent_objn, outlineobj+index-1);
                            }
                            else
                            {
                                sprintf(wbuf, ")\n/Dest [%d /Fit]\n/Parent %d 0 R \n/Next %d 0 R \n/Prev %d 0 R \n>>\nendobj\n", 
                                                q->dest.ld.gotor.page, parent_objn, outlineobj+index+1, outlineobj+index-1);
                            }
                            //printf("%s\n", wbuf);
                            write(fd_out, wbuf, strlen(wbuf));
                            children_index++;
                            index++;
                            q = q->next;
                        }
                        
                        
                        p = p->next;
                        parent_index++;
                    }

                    outlinesflag = 2;
                }
                
                if (infoflag == 1)
                {
                                     
                    //add an new obj
                    if(infoobj == objn)
                    {
                        printf("infoflag %s %s\n", author, title);
                        
                        memset(wbuf,0,WBUF_SIZE);
                        sprintf(wbuf, "\n%d 0 obj \n<<\n/Author (", infoobj);
                        write(fd_out, wbuf, strlen(wbuf));

                        write_unicode_as_UTF_16BE(fd_out, utf8_to_unicode(author, uni, MAX_KEYWORD_LEN));
                        
                        write(fd_out, ")\n/Title (", strlen(")\n/Title ("));

                        write_unicode_as_UTF_16BE(fd_out, utf8_to_unicode(title, uni, MAX_KEYWORD_LEN));
                        
                        write(fd_out, ")\n>>\nendobj \n", strlen(")\n>>\nendobj \n"));

                        infoflag = 2;
                    }
                    else 
                    {
                        //modify current info object
                        int has_title = 0;
                        int has_author = 0;
                        
                        if(infoobj == currentobj)
                        {
                            printf("infoflag %s %s\n", author, title);
                            printf("need to modify obj %d for add title and author\n", infoobj);
                            
                            while(read(fd_in, &c, 1)==1)
                            {
                                if(c=='/')
                                {
                                    write(fd_out, &c, 1);
                                    char buf[5] = {0};
                                    if(read(fd_in, buf, 5)!=5)
                                    {
                                        printf("read error\n");
                                        return -1;
                                    }

                                    if(!memcmp(buf, "Title", 5))
                                    {
                                        has_title = 1;
                                    }
                                    if(!memcmp(buf, "Autho", 5))
                                    {
                                        has_author = 1;
                                    }
                                    write(fd_out, buf, 5);
                                }
                                else if(c=='e')
                                {
                                    
                                    char buf[5] = {0};
                                    if(read(fd_in, buf, 5)!=5)
                                    {
                                        printf("read error\n");
                                        return -1;
                                    }

                                    if(!memcmp(buf, "ndobj", 5))
                                    {
                                        write(fd_out, "endobj", strlen("endobj"));
                                        break;
                                    }
                                    else
                                    {
                                        write(fd_out, &c, 1);
                                        lseek(fd_in, -5, SEEK_CUR);
                                    }
                                }
                                else if(c=='>')
                                {
                                    if(read(fd_in, &c, 1)!=1)
                                    {
                                        printf("read error\n");
                                        return -1;
                                    }

                                    if(c=='>')
                                    {
                                        if(!has_title)
                                        {                                           
                                            write(fd_out, "\n/Title (", strlen("\n/Title ("));
                                            write_unicode_as_UTF_16BE(fd_out, utf8_to_unicode(title, uni, MAX_KEYWORD_LEN));
                                            write(fd_out, ")\n", strlen(")\n"));
                                        }
                                        if(!has_author)
                                        {
                                            write(fd_out, "\n/Author (", strlen("\n/Author ("));
                                            write_unicode_as_UTF_16BE(fd_out, utf8_to_unicode(author, uni, MAX_KEYWORD_LEN));
                                            write(fd_out, ")\n", strlen(")\n"));
                                        }
                                        write(fd_out, ">>", strlen(">>"));
                                    }
                                    else
                                    {
                                        c = '>';
                                        write(fd_out, &c, 1);
                                        lseek(fd_in, -1, SEEK_CUR);
                                    }
                                }
                                else
                                {
                                    write(fd_out, &c, 1);
                                }
                            }
                        }
                    }
                }
                
            }
            else
            {
                lseek(fd_in, -5, SEEK_CUR);
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

int parse_from_json(char *infofile, char *title, char *author)
{
    FILE *f = fopen(infofile, "r");
	if(!f)
	{
		printf("open %s error\n", infofile);
		return -1;
	}
    
    char line[1024] = {0};
    while(fgets(line, 1024, f))
    {
        printf("%s", line);
        
        
        int name_start = 0;
        while(line[name_start]&& line[name_start] != '"') name_start++;
        name_start++;
        
        int name_end = name_start+1;
        while(line[name_end]&& line[name_end] != '"') name_end++;
        
        int value_start = name_end+1;
        while(line[value_start]&& line[value_start] != '"') value_start++;
        value_start++;
    
        int value_end = value_start+1;
        while(line[value_end]&& line[value_end] != '"') value_end++;    
        
        char name[128] = {0};
        char value[128] = {0};
        
        memcpy(name, line + name_start, name_end - name_start);
        memcpy(value, line + value_start, value_end - value_start);
        
        printf("<%s><%s>\n", name, value);
        
        if(!memcmp(name, "Title", strlen("Title")))
            memcpy(title, value, strlen(value));
        
        if(!memcmp(name, "Author", strlen("Author")))
            memcpy(author, value, strlen(value));

        memset(line,0, 1024);
    }
    
    if(*author && *title)
        return 0;
    
    return -1;
}


int main(int argc, char **argv)
{
	char *inputfile, *outputfile, *contentfile = NULL;
	fz_context *ctx;
	fz_document *doc;
    pdf_obj *obj;

	if(argc < 3)
	{
		fprintf(stderr, "usage: build/debug/pdf-fix input_pdf output_pdf [contentfile]\n");
		return EXIT_FAILURE;
	}
	inputfile = argv[1];
    outputfile = argv[2];
    if(argc == 4)
        contentfile = argv[3];
    
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
    
    char title[128] = {0}, author[128] = {0};
    int infoobj = 0;
	obj = pdf_dict_get(ctx, pdf_trailer(ctx, (pdf_document *)doc), PDF_NAME_Info);
	if (obj)
	{
		printf("Info object (%d %d R)\n", pdf_to_num(ctx, obj), pdf_to_gen(ctx, obj));
        infoobj = pdf_to_num(ctx, obj);
	}

    //parse form pdf file
    pdf_parse_title_author(ctx, doc, title, author);
    
    fz_outline *outline = pdf_load_outline_origin(ctx, doc);
    if(outline)
    {
        printf("outline already exist in pdf\n");
        outline = NULL;
    }
    else
    {
        if(contentfile)
        {
            printf("load outline from %s\n", contentfile);
            outline = load_outline(ctx, contentfile);
        }
        else
        {
            printf("parse outline from pdf\n");
            outline = pdf_load_outline_fixed(ctx, doc);
        }
        make_outline_index(outline);
        print_outline(outline);
    }

    write_new_pdf(inputfile, outputfile, ctx, doc, outline, infoobj, title, author);
    
    //fz_drop_outline(ctx, outline);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
	return 0;
}
