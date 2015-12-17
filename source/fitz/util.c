#include "mupdf/fitz.h"

fz_display_list *
fz_new_display_list_from_page(fz_context *ctx, fz_page *page)
{
	fz_display_list *list;
	fz_device *dev;

	list = fz_new_display_list(ctx);

	fz_try(ctx)
	{
		dev = fz_new_list_device(ctx, list);
		fz_run_page(ctx, page, dev, &fz_identity, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_rethrow(ctx);
	}

	return list;
}

fz_display_list *
fz_new_display_list_from_page_number(fz_context *ctx, fz_document *doc, int number)
{
	fz_page *page;
	fz_display_list *list;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		list = fz_new_display_list_from_page(ctx, page);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return list;
}

fz_pixmap *
fz_new_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, const fz_matrix *ctm, fz_colorspace *cs, int background)
{
	fz_rect rect;
	fz_irect irect;
	fz_pixmap *pix;
	fz_device *dev;

	fz_bound_display_list(ctx, list, &rect);
	fz_transform_rect(&rect, ctm);
	fz_round_rect(&irect, &rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, &irect);
	if (background)
		fz_clear_pixmap_with_value(ctx, pix, 0xFF);
	else
		fz_clear_pixmap(ctx, pix);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, pix);
		fz_run_display_list(ctx, list, dev, ctm, NULL, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page_contents(fz_context *ctx, fz_page *page, const fz_matrix *ctm, fz_colorspace *cs)
{
	fz_rect rect;
	fz_irect irect;
	fz_pixmap *pix;
	fz_device *dev;

	fz_bound_page(ctx, page, &rect);
	fz_transform_rect(&rect, ctm);
	fz_round_rect(&irect, &rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, &irect);
	fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, pix);
		fz_run_page_contents(ctx, page, dev, ctm, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_annot(fz_context *ctx, fz_page *page, fz_annot *annot, const fz_matrix *ctm, fz_colorspace *cs)
{
	fz_rect rect;
	fz_irect irect;
	fz_pixmap *pix;
	fz_device *dev;

	fz_bound_annot(ctx, page, annot, &rect);
	fz_transform_rect(&rect, ctm);
	fz_round_rect(&irect, &rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, &irect);
	fz_clear_pixmap(ctx, pix);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, pix);
		fz_run_annot(ctx, page, annot, dev, ctm, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page(fz_context *ctx, fz_page *page, const fz_matrix *ctm, fz_colorspace *cs)
{
	fz_rect rect;
	fz_irect irect;
	fz_pixmap *pix;
	fz_device *dev;

	fz_bound_page(ctx, page, &rect);
	fz_transform_rect(&rect, ctm);
	fz_round_rect(&irect, &rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, &irect);
	fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, pix);
		fz_run_page(ctx, page, dev, ctm, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_matrix *ctm, fz_colorspace *cs)
{
	fz_page *page;
	fz_pixmap *pix;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		pix = fz_new_pixmap_from_page(ctx, page, ctm, cs);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return pix;
}

fz_text_page *
fz_new_text_page_from_display_list(fz_context *ctx, fz_display_list *list, fz_text_sheet *sheet)
{
	fz_text_page *text;
	fz_device *dev;

	text = fz_new_text_page(ctx);
	fz_try(ctx)
	{
		dev = fz_new_text_device(ctx, sheet, text);
		fz_run_display_list(ctx, list, dev, &fz_identity, NULL, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_text_page(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

fz_text_page *
fz_new_text_page_from_page(fz_context *ctx, fz_page *page, fz_text_sheet *sheet)
{
	fz_text_page *text;
	fz_device *dev;

	text = fz_new_text_page(ctx);
	fz_try(ctx)
	{
		dev = fz_new_text_device(ctx, sheet, text);
		fz_run_page(ctx, page, dev, &fz_identity, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_text_page(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

fz_text_page *
fz_new_text_page_from_page_number(fz_context *ctx, fz_document *doc, int number, fz_text_sheet *sheet)
{
	fz_page *page;
	fz_text_page *text;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		text = fz_new_text_page_from_page(ctx, page, sheet);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return text;
}

int
fz_search_display_list(fz_context *ctx, fz_display_list *list, const char *needle, fz_rect *hit_bbox, int hit_max)
{
	fz_text_sheet *sheet;
	fz_text_page *text;
	int count;

	sheet = fz_new_text_sheet(ctx);
	fz_try(ctx)
	{
		text = fz_new_text_page_from_display_list(ctx, list, sheet);
		count = fz_search_text_page(ctx, text, needle, hit_bbox, hit_max);
	}
	fz_always(ctx)
		fz_drop_text_sheet(ctx, sheet);
	fz_catch(ctx)
		fz_rethrow(ctx);
	fz_drop_text_page(ctx, text);
	return count;
}

int
fz_search_page(fz_context *ctx, fz_page *page, const char *needle, fz_rect *hit_bbox, int hit_max)
{
	fz_text_sheet *sheet;
	fz_text_page *text;
	int count;

	sheet = fz_new_text_sheet(ctx);
	fz_try(ctx)
	{
		text = fz_new_text_page_from_page(ctx, page, sheet);
		count = fz_search_text_page(ctx, text, needle, hit_bbox, hit_max);
	}
	fz_always(ctx)
		fz_drop_text_sheet(ctx, sheet);
	fz_catch(ctx)
		fz_rethrow(ctx);
	fz_drop_text_page(ctx, text);
	return count;
}

int
fz_search_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, fz_rect *hit_bbox, int hit_max)
{
	fz_page *page;
	int count;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		count = fz_search_page(ctx, page, needle, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

fz_buffer *
fz_new_buffer_from_text_page(fz_context *ctx, fz_text_page *text, const fz_rect *sel, int crlf)
{
	fz_buffer *buf;
	fz_rect hitbox;
	float x0, y0, x1, y1;
	int block_num;
	int need_newline;
	int i;

	need_newline = 0;

	if (fz_is_infinite_rect(sel))
	{
		x0 = y0 = -FLT_MAX;
		x1 = y1 = FLT_MAX;
	}
	else
	{
		x0 = sel->x0;
		y0 = sel->y0;
		x1 = sel->x1;
		y1 = sel->y1;
	}

	buf = fz_new_buffer(ctx, 256);
	fz_try(ctx)
	{
		for (block_num = 0; block_num < text->len; block_num++)
		{
			fz_text_line *line;
			fz_text_block *block;
			fz_text_span *span;

			if (text->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
				continue;

			block = text->blocks[block_num].u.text;
			for (line = block->lines; line < block->lines + block->len; line++)
			{
				int saw_text = 0;
				for (span = line->first_span; span; span = span->next)
				{
					for (i = 0; i < span->len; i++)
					{
						int c;
						fz_text_char_bbox(ctx, &hitbox, span, i);
						c = span->text[i].c;
						if (c < 32)
							c = '?';
						if (hitbox.x1 >= x0 && hitbox.x0 <= x1 && hitbox.y1 >= y0 && hitbox.y0 <= y1)
						{
							saw_text = 1;
							if (need_newline)
							{
								if (crlf)
									fz_write_buffer_rune(ctx, buf, '\r');
								fz_write_buffer_rune(ctx, buf, '\n');
								need_newline = 0;
							}
							fz_write_buffer_rune(ctx, buf, c);
						}
					}
				}

				if (saw_text)
					need_newline = 1;
			}
		}
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}

	return buf;
}

fz_buffer *
fz_new_buffer_from_display_list(fz_context *ctx, fz_display_list *list, const fz_rect *sel, int crlf)
{
	fz_text_sheet *sheet;
	fz_text_page *text;
	fz_buffer *buf;

	sheet = fz_new_text_sheet(ctx);
	fz_try(ctx)
	{
		text = fz_new_text_page_from_display_list(ctx, list, sheet);
		buf = fz_new_buffer_from_text_page(ctx, text, sel, crlf);
	}
	fz_always(ctx)
		fz_drop_text_sheet(ctx, sheet);
	fz_catch(ctx)
		fz_rethrow(ctx);
	fz_drop_text_page(ctx, text);
	return buf;
}

fz_buffer *
fz_new_buffer_from_page(fz_context *ctx, fz_page *page, const fz_rect *sel, int crlf)
{
	fz_text_sheet *sheet;
	fz_text_page *text;
	fz_buffer *buf;

	sheet = fz_new_text_sheet(ctx);
	fz_try(ctx)
	{
		text = fz_new_text_page_from_page(ctx, page, sheet);
		buf = fz_new_buffer_from_text_page(ctx, text, sel, crlf);
	}
	fz_always(ctx)
		fz_drop_text_sheet(ctx, sheet);
	fz_catch(ctx)
		fz_rethrow(ctx);
	fz_drop_text_page(ctx, text);
	return buf;
}

fz_buffer *
fz_new_buffer_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_rect *sel, int crlf)
{
	fz_page *page;
	fz_buffer *buf;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		buf = fz_new_buffer_from_page(ctx, page, sel, crlf);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return buf;
}

int fz_gen_thumb_image(fz_context *ctx, fz_document *doc, int pagenum, int width, char *outfile)
{
    fz_page *page;
    float zoom;
    fz_matrix ctm;
    fz_rect bounds, tbounds;
    fz_irect ibounds;
	fz_colorspace *colorspace;
    fz_output *output_file = NULL;
    fz_png_output_context *poc = NULL;
    fz_device *dev = NULL;
    fz_pixmap *pix = NULL;
    fz_cookie cookie = {0};

    int resolution = 72;
    int alphabits = 8;

	fz_var(dev);
    fz_var(pix);
    fz_var(poc);

    fz_set_aa_level(ctx, alphabits);
	colorspace = fz_device_rgb(ctx);

	fz_try(ctx)
		page = fz_load_page(ctx, doc, pagenum - 1);
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot load page %d", pagenum);
		return -1;
	}

    fz_bound_page(ctx, page, &bounds);

    zoom = resolution / 72;
    fz_pre_scale(fz_rotate(&ctm, 0), zoom, zoom);
    tbounds = bounds;
    fz_round_rect(&ibounds, fz_transform_rect(&tbounds, &ctm));

    if(width)
    {
        float scalex = width / (tbounds.x1 - tbounds.x0);
        float scaley = scalex;
        fz_matrix scale_mat;

        fz_scale(&scale_mat, scalex, scaley);
        fz_concat(&ctm, &ctm, &scale_mat);
        tbounds = bounds;
        fz_transform_rect(&tbounds, &ctm);
    }
    fz_round_rect(&ibounds, &tbounds);
    fz_rect_from_irect(&tbounds, &ibounds);


    fz_try(ctx)
    {
        int totalheight = ibounds.y1 - ibounds.y0;
        fz_irect band_ibounds = ibounds;
        int drawheight = totalheight;

        pix = fz_new_pixmap_with_bbox(ctx, colorspace, &band_ibounds);
        fz_pixmap_set_resolution(pix, resolution);

        output_file = fz_new_output_to_filename(ctx, outfile);
        poc = fz_output_png_header(ctx, output_file, pix->w, totalheight, pix->n, 1);

        fz_clear_pixmap(ctx, pix);
        dev = fz_new_draw_device(ctx, pix);
        fz_run_page(ctx, page, dev, &ctm, &cookie);

        fz_unmultiply_pixmap(ctx, pix);
        fz_output_png_band(ctx, output_file, pix->w, totalheight, pix->n, 0, drawheight, pix->samples, 1, poc);
    }
    fz_always(ctx)
    {
        fz_output_png_trailer(ctx, output_file, poc);
        fz_drop_device(ctx, dev);
        fz_drop_pixmap(ctx, pix);
        fz_drop_output(ctx, output_file);
		fz_drop_page(ctx, page);
    }
    fz_catch(ctx)
    {
        fz_drop_page(ctx, page);
        fz_rethrow(ctx);
		return -1;
    }

    return 0;
}
