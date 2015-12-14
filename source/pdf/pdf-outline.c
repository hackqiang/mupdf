#include "mupdf/pdf.h"
#include "mupdf/fitz.h"

static fz_outline *
pdf_load_outline_imp(fz_context *ctx, pdf_document *doc, pdf_obj *dict)
{
	fz_outline *node, **prev, *first;
	pdf_obj *obj;
	pdf_obj *odict = dict;

	fz_var(dict);
	fz_var(first);

	fz_try(ctx)
	{
		first = NULL;
		prev = &first;
		while (dict && pdf_is_dict(ctx, dict))
		{
			if (pdf_mark_obj(ctx, dict))
				break;
			node = fz_new_outline(ctx);
			*prev = node;
			prev = &node->next;

			obj = pdf_dict_get(ctx, dict, PDF_NAME_Title);
			if (obj)
				node->title = pdf_to_utf8(ctx, doc, obj);

			if ((obj = pdf_dict_get(ctx, dict, PDF_NAME_Dest)) != NULL)
				node->dest = pdf_parse_link_dest(ctx, doc, FZ_LINK_GOTO, obj);
			else if ((obj = pdf_dict_get(ctx, dict, PDF_NAME_A)) != NULL)
				node->dest = pdf_parse_action(ctx, doc, obj);

			obj = pdf_dict_get(ctx, dict, PDF_NAME_First);
			if (obj)
			{
				node->down = pdf_load_outline_imp(ctx, doc, obj);

				obj = pdf_dict_get(ctx, dict, PDF_NAME_Count);
				if (pdf_to_int(ctx, obj) > 0)
					node->is_open = 1;
			}

			dict = pdf_dict_get(ctx, dict, PDF_NAME_Next);
		}
	}
	fz_always(ctx)
	{
		for (dict = odict; dict && pdf_obj_marked(ctx, dict); dict = pdf_dict_get(ctx, dict, PDF_NAME_Next))
			pdf_unmark_obj(ctx, dict);
	}
	fz_catch(ctx)
	{
		fz_drop_outline(ctx, first);
		fz_rethrow(ctx);
	}

	return first;
}

fz_outline *
pdf_load_outline_fixed(fz_context *ctx, fz_document *doc);
fz_outline *
pdf_load_outline(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *root, *obj, *first;

	root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME_Root);
	obj = pdf_dict_get(ctx, root, PDF_NAME_Outlines);
	first = pdf_dict_get(ctx, obj, PDF_NAME_First);
	if (first)
		return pdf_load_outline_imp(ctx, doc, first);

	return pdf_load_outline_fixed(ctx, (fz_document *)doc);
}


//////////////////
#define SERACH_MAX_PAGE 20
#define MAX_KEYWORD_LEN 128
#define KEYWORKS_NUM 128
//must be utf8
static char content_keywords[KEYWORKS_NUM][MAX_KEYWORD_LEN] = 
{
    "目 录",
    "目  录",
    "目　录",
    "目!!录",
    "目录",
    "content",
    "CONTENT",
    "contents",
    "CONTENTS",
    "ｃｏｎｔｅｎｔ",
    "ＣＯＮＴＥＮＴ",
    "ｃｏｎｔｅｎｔｓ",
    "ＣＯＮＴＥＮＴＳ",
};

static char chapter_keywords[KEYWORKS_NUM][MAX_KEYWORD_LEN] = 
{
    "Chapter",
    "chapter",
    //"第一章", //do not care for now
    //"第1章",
    //"第 1 章",
    "第",
};

fz_outline *first_node = NULL;

//get form mupdf code
static int textlen(fz_context *ctx, fz_text_page *page)
{
	int len = 0;
	int block_num;

	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;
		fz_text_line *line;
		fz_text_span *span;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->first_span; span; span = span->next)
			{
				len += span->len;
			}
			len++; /* pseudo-newline */
		}
	}
	return len;
}

int unicode2int(int *unicode)
{
    int num = 0;
    int pos=0;
    while(unicode[pos]>='0' && unicode[pos]<='9')
    {
        num = num*10+(unicode[pos]-'0');
        pos++;
    }
    return num;
}

int print_unicode(int *unicode, int len)
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
    
    return 0;
}

char *unicode_to_utf8(int *unicode, char *utf8, int len)
{
    int pos;
    
    if(len==0)
        while(*(unicode+len)) len++;
    
    memset(utf8, 0, len);
    int off = 0;
    for (pos = 0; pos < len && unicode[pos]; pos++)
    {
        char temp[4] = {0};
        int ret = fz_runetochar(temp,unicode[pos]);
        //printf("%s", temp);
        memcpy(utf8+off, temp, ret);
        off += ret;
    }
    
    return utf8;
}

// size < MAX_KEYWORD_LEN
static int uni[MAX_KEYWORD_LEN/4] = {0};
int *tounicode(char *utf8)
{
    int *unicode = uni;
    memset(unicode, 0, MAX_KEYWORD_LEN);
    while (*utf8)
    {
        utf8 += fz_chartorune(unicode++, (char *)utf8);
    }
    return uni;
}

int *new_text_form_page_number(fz_context *ctx, fz_document *doc, int page)
{
    int pos;
    printf("begin page%d text\n", page);
    
    fz_text_sheet *sheet = fz_new_text_sheet(ctx);
    fz_text_page *text = fz_new_text_page_from_page_number(ctx, doc, page, sheet);
    
    int len = textlen(ctx, text);
    int *tbuf = (int *)malloc(sizeof(int)*(len+1));
    if(!tbuf) {
        fprintf(stderr, "malloc %d error\n", len+1);
        return NULL;
    }
    memset(tbuf,0,sizeof(int)*(len+1));

    for(pos = 0; pos < len; pos++)
    {
        fz_char_and_box cab;
        tbuf[pos] = fz_text_char_at(ctx, &cab, text, pos)->c;
        char ww[4];
        fz_runetochar(ww,tbuf[pos]);
#ifdef DUMP_DEBUG
        if(!(pos%16)) printf("\n");
        printf("%08x ", tbuf[pos]);
#endif
    }
#ifdef DUMP_DEBUG
    printf("\n");
#endif
    return tbuf;
}

void free_text(int *text)
{
    if(text)
        free(text);
}


char *unicode2utf8(int *unicode, char *utf8, int ulen)
{
    int pos;
    int ui = 0;
    int len=0;
    while(*(unicode+len)) len++;
    
    for (pos = 0; pos < len && unicode[pos]; pos++)
    {
        
        char temp[4] = {0};
        int ret = fz_runetochar(temp,unicode[pos]);
        if(ui+ret>ulen)
        {
            break;
        }
        memcpy(utf8+ui, temp, ret);
        ui+=ret;
    }
    return utf8;
}


/*分析一条章节信息，应该是类似于：
    第一章　连环奸杀案／003 
    第二章　设下诡局／018
    Chapter 1 .......................................... 1  
    Chapter 2 .......................................... 5
*/
int parse_chapter_info(fz_context *ctx, fz_document *doc, int *txt, int len)
{
    int pos = 0, i=0;
    
    if(len==0)
        while(*(txt+len)) len++;    
    
    int *new_chapter = (int *)malloc((len+1)*sizeof(int));
    if(!new_chapter)
    {
        fprintf(stderr, "malloc %lu error\n", (len+1)*sizeof(int));
        return -1;
    }
    memset(new_chapter, 0, (len+1)*sizeof(int));
    
    //1. 去掉"." 
    while(txt[pos] && pos<len)
    {
        if(txt[pos]!='.')
        {
            new_chapter[i++] = txt[pos];
        }
        pos++;
    }

    //2. 去掉末尾非数字字符
    int j=i-1;
    while((new_chapter[j]<'0' || new_chapter[j]>'9') && j>=0)
    {
        new_chapter[j] = 0;
        j--;
    }
    
    if(j==-1)
    {
        fprintf(stderr, "no page number found\n");
        return -1;
    }
    
    //3. 检测末尾的数字
    int num = j;
    while(new_chapter[num]>='0' && new_chapter[num]<='9')
    {
        num--;
    }

    //这样，这一段章节信息就可以被分成了两段：
    //part1
    j=0;
    int text[32] = {0};
    while(j<num+1 && j<31)
    {
        text[j] = new_chapter[j];
        j++;
    }
    
    char *utf8 = (char *)malloc(128);
    if(!utf8)
    {
        printf("malloc error\n");
        return -1;
    }
    
    if(!first_node)
    {
        first_node = fz_new_outline(ctx);
        first_node->title = unicode_to_utf8(text,utf8,128);
        //todo first_node->dest = 
        printf("%s<->", first_node->title);
    }
    else
    {
        fz_outline *p = first_node;
        while(p->next) p = p->next;
        fz_outline *node = fz_new_outline(ctx);
        p->next = node;
        node->title = unicode_to_utf8(text,utf8,128);
        printf("%s<->", node->title);
        //todo first_node->dest = 
    }
    
    //part2
    printf("%d\n", unicode2int(new_chapter+num+1));

    free(new_chapter);
    return 0;
}

int search(int *text, int *match)
{
    int text_len = 0;
    int match_len = 0;

    while(*(text+text_len)) text_len++;
    while(*(match+match_len)) match_len++;
    
    //printf("search %d %d\n", text_len, match_len);
    
    int p;
    for(p=0;p<text_len;p++)
    {
        if(text[p] == match[0])
        {
            int m = 1;
            while(m<match_len && text[p+m] == match[m]) m++;
            if(m==match_len)
            {
                return p;
            }
        }
    }
    return -1;
}

int analyse_contents(fz_context *ctx, fz_document *doc, int *text)
{
    int j = 0;
    while(j<KEYWORKS_NUM && chapter_keywords[j][0])
    {
        printf("searching %s\n", chapter_keywords[j]);
        int pos = 0;
        int pre_pos = -1;
        int *tt = text;
        //根据关键字，得到一行行的章节信息
        while((pos=search(tt, tounicode(chapter_keywords[j])))>=0)
        {
            //printf("get %s %d\n", chapter_keywords[j], pos);
            if(pre_pos>=0)
            {
                //printf("%d\n",pos);
                parse_chapter_info(ctx,doc,tt-1,pos);
            }
            tt+=pos;
            tt++;
            pre_pos = pos;
        }
        if(pre_pos>=0)
        {
            //最后一段
            parse_chapter_info(ctx,doc,tt-1,0);
            break;
        }
        j++;
    }

    return 0;
}

//todo
int fix_page_offset()
{
    return 0;
}

fz_outline *
pdf_load_outline_fixed(fz_context *ctx, fz_document *doc)
{
    fz_rect search_hit_bbox[32];
    int count = 0;
    int current_page;
    int j = 0;
    int *text;
    int page_count = fz_count_pages(ctx, doc);
    
    while(j<KEYWORKS_NUM && content_keywords[j][0] && !count)
    {
        
        for(current_page=0; current_page<SERACH_MAX_PAGE && current_page<page_count; current_page++)
        {
            //printf("serach page%d (%s)\n", current_page, content_keywords[j]);
            /*
             * 有时候，字符虽然是存在pdf中的，用reader也能看到，编码也是OK的，但是用这个API搜索不到，
             *  debug发现，字符串确实是找到了，但是charbox为空，导致mupdf认为找不到(1c2)... 先不care了...
             */
            count = fz_search_page_number(ctx, doc, current_page, content_keywords[j], search_hit_bbox, 32);
            if(count)
            {
                printf("found page%d (%s) %d times\n", current_page,content_keywords[j],count);
                break;
            }
        }
        j++;
    }

    if(count)
    {
        text = new_text_form_page_number(ctx, doc, current_page);
        if(!text)
        {
            fprintf(stderr, "get page%d error\n", current_page);
            return NULL;
        }
        
        // get contents success, begin to analyse
        analyse_contents(ctx, doc, text);
        //write_new_pdf(ctx, doc);
        free_text(text);
        return first_node;
    }
    
	return NULL;
}



