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

//em......
fz_outline *
pdf_load_outline_origin(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *root, *obj, *first;

	root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME_Root);
	obj = pdf_dict_get(ctx, root, PDF_NAME_Outlines);
	first = pdf_dict_get(ctx, obj, PDF_NAME_First);
	if (first)
		return pdf_load_outline_imp(ctx, doc, first);
}


/*
 *   add outlines by parse contects pages 
 */

//#define DUMP_DEBUG_PRINT
#define DEBUG_PRINT printf

#define SERACH_MAX_PAGE 24
#define MAX_KEYWORD_LEN 256
#define KEYWORKS_NUM 256
#define MAX_CHAPTER_LEN 256 //char *

//must encode as utf8
static char chapter_keywords[KEYWORKS_NUM][MAX_KEYWORD_LEN] = 
{
    "Chapter",
    "chapter",
    {0xE7, 0xAC, 0xAC}//"第",
};

static fz_outline *first_node = NULL;

//get form mupdf code
int textlen(fz_context *ctx, fz_text_page *page)
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

static int unicode_to_int(int *unicode)
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

char *unicode_to_utf8(int *unicode, int ulen, char *utf8, int utf8len)
{
    int pos;
    int ui = 0;
    if(ulen == 0)
        while(*(unicode+ulen)) ulen++;
    
    for (pos = 0; pos < ulen && unicode[pos]; pos++)
    {
        
        char temp[4] = {0};
        int ret = fz_runetochar(temp,unicode[pos]);
        if(ui+ret>utf8len)
        {
            break;
        }
        memcpy(utf8+ui, temp, ret);
        ui+=ret;
    }
    return utf8;
}

void print_unicode(int *unicode, int len)
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



int *utf8_to_unicode(char *utf8, int *uni, int uni_len)
{
    int *unicode = uni;
    memset(uni, 0, uni_len*4);
    while (*utf8)
    {
        utf8 += fz_chartorune(unicode++, (char *)utf8);
    }
    return uni;
}

static int *new_text_form_page_number(fz_context *ctx, fz_document *doc, int start_page, int total_page)
{
    int i;
    int pos;
    int len = 0;
    int *textbuf = NULL;
    
    for(i = start_page; i < start_page + total_page; i++)
    {
        DEBUG_PRINT("get page%d text\n", i);
        
        fz_text_sheet *sheet = fz_new_text_sheet(ctx);
        fz_text_page *text = fz_new_text_page_from_page_number(ctx, doc, i, sheet);
        int tlen = textlen(ctx, text);
        
        textbuf = (int *)realloc(textbuf, sizeof(int)*(len + tlen+1));
        if(!textbuf) {
            printf("malloc %d error\n", len+1);
            return NULL;
        }
        memset(textbuf+len,0,sizeof(int)*(tlen+1));

        for(pos = 0; pos < tlen; pos++)
        {
            fz_char_and_box cab;
            textbuf[len+pos] = fz_text_char_at(ctx, &cab, text, pos)->c;
#ifdef DUMP_DEBUG_PRINT
            if(!(pos%16)) printf("\n");
            printf("%04x ", textbuf[len+pos]);
#endif
        }
        fz_drop_text_page(ctx, text);
        len += tlen;
#ifdef DUMP_DEBUG_PRINT
        printf("\n");
#endif
    }
    
    return textbuf;
}

//free the text form new_text_form_page_number
static void free_text(int *text)
{
    if(text)
        free(text);
}

static void print_outline(fz_outline *outline)
{
    fz_outline *p = outline;
    while(p)
    {
        DEBUG_PRINT("%s<->%d\n", p->title, p->dest.ld.gotor.page);
        p = p->next;
    }
}


/*
 *分析一条章节信息，应该是类似于：
 *  第一章　连环奸杀案／003 
 *  第二章　设下诡局／018
 *  Chapter 1 .......................................... 1  
 *  Chapter 2 .......................................... 5
 *  只分析前1024个字(int)
 */
static int parse_chapter_info(fz_context *ctx, fz_document *doc, int *txt, int len)
{
    int pos, i, j;
    int chapter_buf[1024] = {0};
    
    if(len==0)
        while(*(txt+len)) len++;    
    
    if(len > 1024)
        len = 1024;
    
    //1. 去掉"." 
    i = 0;
    pos = 0;
    while(txt[pos] && pos<len)
    {
        if(txt[pos]!='.')
        {
            chapter_buf[i++] = txt[pos];
        }
        pos++;
    }

    //2. 去掉末尾非数字字符
    j = i - 1;
    while((chapter_buf[j]<'0' || chapter_buf[j]>'9') && j>=0)
    {
        chapter_buf[j] = 0;
        j--;
    }
    
    if(j == -1)
    {
        printf("ERROR: no page number found\n");
        return -1;
    }
    
    //3. 检测末尾的数字
    int num = j;
    while(chapter_buf[num] >= '0' && chapter_buf[num] <= '9')
    {
        num--;
    }

    //这样，这一段章节信息就可以被分成了两段：
    //part1 章节标题
    //part2 页数
    j = 0;
    int title[MAX_CHAPTER_LEN/4] = {0};
    while(j < num + 1 && j<  MAX_CHAPTER_LEN/4 - 1)
    {
        title[j] = chapter_buf[j];
        j++;
    }
    
    //4. 过滤掉章节标题末尾的符号(" ", "/", "／ 0xff0f")
    j--;
    while(j>0)
    {
        if(title[j] == ' ' || title[j] == '/' || title[j] == '\\' || title[j] == 0xff0f)
        {
            title[j] = 0;
            j--;
        }
        else
        {
            break;
        }
    }
    
    char *utf8_title = (char *)malloc(MAX_CHAPTER_LEN);
    if(!utf8_title)
    {
        printf("malloc %d error\n", MAX_CHAPTER_LEN);
        return -1;
    }
    memset(utf8_title, 0, MAX_CHAPTER_LEN);

    fz_outline *node = fz_new_outline(ctx); //already memset 0
    
    node->title = unicode_to_utf8(title, 0, utf8_title, MAX_CHAPTER_LEN);
    
    node->dest.kind = FZ_LINK_GOTO;
    node->dest.ld.gotor.flags = fz_link_flag_fit_h | fz_link_flag_fit_v;
    node->dest.ld.gotor.page = unicode_to_int(chapter_buf+num+1);
    node->dest.ld.gotor.dest = NULL;
    //DEBUG_PRINT("%s<->%d\n", first_node->title, first_node->dest.ld.gotor.page);    

    if(!first_node)
    {
        first_node = node;
    }
    else
    {
        fz_outline *p = first_node;
        while(p->next) p = p->next;
        p->next = node;
    }

    return 0;
}

static int search(int *text, int *match)
{
    int i;
    int text_len = 0;
    int match_len = 0;

    while(*(text+text_len)) text_len++;
    while(*(match+match_len)) match_len++;
    //DEBUG_PRINT("search %d %d\n", text_len, match_len);
    
    for(i = 0; i < text_len; i++)
    {
        if(text[i] == match[0])
        {
            int m = 1;
            while(m < match_len && text[i + m] == match[m]) m++;
            if(m == match_len)
            {
                return i;
            }
        }
    }
    return -1;
}

static int analyse_contents(fz_context *ctx, fz_document *doc, int *text)
{
    int j = 0;
    int uni[MAX_KEYWORD_LEN] = {0};
    while(j < KEYWORKS_NUM && chapter_keywords[j][0])
    {
        //DEBUG_PRINT("searching %s\n", chapter_keywords[j]);
        int pos = 0;
        int pre_pos = -1;
        int *tt = text;
        //根据关键字，分割得到一行行的章节信息
        while((pos = search(tt, utf8_to_unicode(chapter_keywords[j], uni, MAX_KEYWORD_LEN))) >= 0)
        {
            //DEBUG_PRINT("get %s %d\n", chapter_keywords[j], pos);
            if(pre_pos >= 0)
            {
                //DEBUG_PRINT("%d\n",pos);
                parse_chapter_info(ctx, doc, tt - 1, pos);
            }
            tt += pos;
            tt++;
            pre_pos = pos;
        }
        if(pre_pos>=0)
        {
            //最后一段
            parse_chapter_info(ctx, doc, tt - 1, 0);
            break;
        }
        j++;
    }

    return 0;
}

static int fix_page_offset(fz_context *ctx, fz_document *doc, fz_outline *outline, int content_start_page, int content_total_page)
{
    if(!outline)
    {
        DEBUG_PRINT("no outline, skip fix offset\n");
        return 0;
    }
    
    int page_offset;
    int current_page;
    fz_rect search_hit_bbox[32];
    int page_count;
    
    page_count = fz_count_pages(ctx, doc);
    
    page_offset = 0;
    for(current_page = content_start_page + content_total_page; current_page < SERACH_MAX_PAGE*2 && current_page < page_count; current_page++)
    {
        int count = fz_search_page_number(ctx, doc, current_page, outline->title, search_hit_bbox, 32);
        if(count)
        {
            page_offset = current_page - outline->dest.ld.gotor.page;
            DEBUG_PRINT("found page%d (%s) %d times, offset %d\n", current_page,outline->title,count,page_offset);
            break;
        }
    }
    
    //更新所有的outlines对象
    if(page_offset)
    {
        fz_outline *p = outline;
        while(p)
        {
            p->dest.ld.gotor.page += page_offset;
            p = p->next;
        }
    }
    return 0;
}

fz_outline *pdf_load_outline_fixed(fz_context *ctx, fz_document *doc)
{
    DEBUG_PRINT("begin pdf_load_outline_fixed\n");
    
    fz_rect search_hit_bbox[32];
    int count;
    int current_page;
    int index;
    int *text = NULL;
    int page_count;
    int content_start_page = -1, content_total_page = 0;
    
    page_count = fz_count_pages(ctx, doc);
    
    //直接使用章节关键字搜索
    count = 0;
    for(current_page = 0; current_page < SERACH_MAX_PAGE && current_page < page_count; current_page++)
    {
        index = 0;
        while(index < KEYWORKS_NUM && chapter_keywords[index][0])
        {
            //DEBUG_PRINT("serach page%d (%s)\n", current_page, chapter_keywords[index]);
            count = fz_search_page_number(ctx, doc, current_page, chapter_keywords[index], search_hit_bbox, 32);
            //简单的认为，关键字数量大于4，就是目录页
            if(count > 4)
            {
                //DEBUG_PRINT("found page%d (%s) %d times\n", current_page,chapter_keywords[index],count);
                if(content_start_page == -1)
                    content_start_page = current_page;
                content_total_page++;
                break;
            }
            index++;
        }
    }

    //find contents
    if(content_total_page)
    {
        text = new_text_form_page_number(ctx, doc, content_start_page, content_total_page);
        if(!text)
        {
            //DEBUG_PRINT("get page%d-%d error\n", content_start_page, content_start_page+content_total_page-1);
            return NULL;
        }
        //DEBUG_PRINT("<begin dump text>");
        //print_unicode(text, 0);
        //DEBUG_PRINT("<end dump text>\n");
        
        //begin to analyse
        analyse_contents(ctx, doc, text);
        print_outline(first_node);
        
        fix_page_offset(ctx, doc, first_node, content_start_page, content_total_page);
        print_outline(first_node);
        
        free_text(text);
        return first_node;
    }
    else
    {
        //debug, dump all text in pdf
        DEBUG_PRINT("no contents found, begin to dump all text\n");
        for(index = 0; index < page_count; index++)
        {
            text = new_text_form_page_number(ctx, doc, index, 1);
            if(!text)
            {
                printf("get page%d error\n", index);
                return NULL;
            }
            //DEBUG_PRINT("<begin dump text>");
            //print_unicode(text, 0);
            //DEBUG_PRINT("<end dump text>\n");
            free_text(text);
        }
    }
    
	return NULL;
}
