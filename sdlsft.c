#include <SDL.h>

#include "sdlsft.h"
#include "schrift.h"

struct sdlsft
{
	struct SFT sft;
	int ascent;
	int descent;
	Uint32 em;
	Uint32 line_space;
};

static const SDL_Colour gray2argb[256] = {
	{ 0xFF, 0xFF, 0xFF, 0x00 }, { 0xFF, 0xFF, 0xFF, 0x01 }, { 0xFF, 0xFF, 0xFF, 0x02 },
	{ 0xFF, 0xFF, 0xFF, 0x03 }, { 0xFF, 0xFF, 0xFF, 0x04 }, { 0xFF, 0xFF, 0xFF, 0x05 },
	{ 0xFF, 0xFF, 0xFF, 0x06 }, { 0xFF, 0xFF, 0xFF, 0x07 }, { 0xFF, 0xFF, 0xFF, 0x08 },
	{ 0xFF, 0xFF, 0xFF, 0x09 }, { 0xFF, 0xFF, 0xFF, 0x0A }, { 0xFF, 0xFF, 0xFF, 0x0B },
	{ 0xFF, 0xFF, 0xFF, 0x0C }, { 0xFF, 0xFF, 0xFF, 0x0D }, { 0xFF, 0xFF, 0xFF, 0x0E },
	{ 0xFF, 0xFF, 0xFF, 0x0F }, { 0xFF, 0xFF, 0xFF, 0x10 }, { 0xFF, 0xFF, 0xFF, 0x11 },
	{ 0xFF, 0xFF, 0xFF, 0x12 }, { 0xFF, 0xFF, 0xFF, 0x13 }, { 0xFF, 0xFF, 0xFF, 0x14 },
	{ 0xFF, 0xFF, 0xFF, 0x15 }, { 0xFF, 0xFF, 0xFF, 0x16 }, { 0xFF, 0xFF, 0xFF, 0x17 },
	{ 0xFF, 0xFF, 0xFF, 0x18 }, { 0xFF, 0xFF, 0xFF, 0x19 }, { 0xFF, 0xFF, 0xFF, 0x1A },
	{ 0xFF, 0xFF, 0xFF, 0x1B }, { 0xFF, 0xFF, 0xFF, 0x1C }, { 0xFF, 0xFF, 0xFF, 0x1D },
	{ 0xFF, 0xFF, 0xFF, 0x1E }, { 0xFF, 0xFF, 0xFF, 0x1F }, { 0xFF, 0xFF, 0xFF, 0x20 },
	{ 0xFF, 0xFF, 0xFF, 0x21 }, { 0xFF, 0xFF, 0xFF, 0x22 }, { 0xFF, 0xFF, 0xFF, 0x23 },
	{ 0xFF, 0xFF, 0xFF, 0x24 }, { 0xFF, 0xFF, 0xFF, 0x25 }, { 0xFF, 0xFF, 0xFF, 0x26 },
	{ 0xFF, 0xFF, 0xFF, 0x27 }, { 0xFF, 0xFF, 0xFF, 0x28 }, { 0xFF, 0xFF, 0xFF, 0x29 },
	{ 0xFF, 0xFF, 0xFF, 0x2A }, { 0xFF, 0xFF, 0xFF, 0x2B }, { 0xFF, 0xFF, 0xFF, 0x2C },
	{ 0xFF, 0xFF, 0xFF, 0x2D }, { 0xFF, 0xFF, 0xFF, 0x2E }, { 0xFF, 0xFF, 0xFF, 0x2F },
	{ 0xFF, 0xFF, 0xFF, 0x30 }, { 0xFF, 0xFF, 0xFF, 0x31 }, { 0xFF, 0xFF, 0xFF, 0x32 },
	{ 0xFF, 0xFF, 0xFF, 0x33 }, { 0xFF, 0xFF, 0xFF, 0x34 }, { 0xFF, 0xFF, 0xFF, 0x35 },
	{ 0xFF, 0xFF, 0xFF, 0x36 }, { 0xFF, 0xFF, 0xFF, 0x37 }, { 0xFF, 0xFF, 0xFF, 0x38 },
	{ 0xFF, 0xFF, 0xFF, 0x39 }, { 0xFF, 0xFF, 0xFF, 0x3A }, { 0xFF, 0xFF, 0xFF, 0x3B },
	{ 0xFF, 0xFF, 0xFF, 0x3C }, { 0xFF, 0xFF, 0xFF, 0x3D }, { 0xFF, 0xFF, 0xFF, 0x3E },
	{ 0xFF, 0xFF, 0xFF, 0x3F }, { 0xFF, 0xFF, 0xFF, 0x40 }, { 0xFF, 0xFF, 0xFF, 0x41 },
	{ 0xFF, 0xFF, 0xFF, 0x42 }, { 0xFF, 0xFF, 0xFF, 0x43 }, { 0xFF, 0xFF, 0xFF, 0x44 },
	{ 0xFF, 0xFF, 0xFF, 0x45 }, { 0xFF, 0xFF, 0xFF, 0x46 }, { 0xFF, 0xFF, 0xFF, 0x47 },
	{ 0xFF, 0xFF, 0xFF, 0x48 }, { 0xFF, 0xFF, 0xFF, 0x49 }, { 0xFF, 0xFF, 0xFF, 0x4A },
	{ 0xFF, 0xFF, 0xFF, 0x4B }, { 0xFF, 0xFF, 0xFF, 0x4C }, { 0xFF, 0xFF, 0xFF, 0x4D },
	{ 0xFF, 0xFF, 0xFF, 0x4E }, { 0xFF, 0xFF, 0xFF, 0x4F }, { 0xFF, 0xFF, 0xFF, 0x50 },
	{ 0xFF, 0xFF, 0xFF, 0x51 }, { 0xFF, 0xFF, 0xFF, 0x52 }, { 0xFF, 0xFF, 0xFF, 0x53 },
	{ 0xFF, 0xFF, 0xFF, 0x54 }, { 0xFF, 0xFF, 0xFF, 0x55 }, { 0xFF, 0xFF, 0xFF, 0x56 },
	{ 0xFF, 0xFF, 0xFF, 0x57 }, { 0xFF, 0xFF, 0xFF, 0x58 }, { 0xFF, 0xFF, 0xFF, 0x59 },
	{ 0xFF, 0xFF, 0xFF, 0x5A }, { 0xFF, 0xFF, 0xFF, 0x5B }, { 0xFF, 0xFF, 0xFF, 0x5C },
	{ 0xFF, 0xFF, 0xFF, 0x5D }, { 0xFF, 0xFF, 0xFF, 0x5E }, { 0xFF, 0xFF, 0xFF, 0x5F },
	{ 0xFF, 0xFF, 0xFF, 0x60 }, { 0xFF, 0xFF, 0xFF, 0x61 }, { 0xFF, 0xFF, 0xFF, 0x62 },
	{ 0xFF, 0xFF, 0xFF, 0x63 }, { 0xFF, 0xFF, 0xFF, 0x64 }, { 0xFF, 0xFF, 0xFF, 0x65 },
	{ 0xFF, 0xFF, 0xFF, 0x66 }, { 0xFF, 0xFF, 0xFF, 0x67 }, { 0xFF, 0xFF, 0xFF, 0x68 },
	{ 0xFF, 0xFF, 0xFF, 0x69 }, { 0xFF, 0xFF, 0xFF, 0x6A }, { 0xFF, 0xFF, 0xFF, 0x6B },
	{ 0xFF, 0xFF, 0xFF, 0x6C }, { 0xFF, 0xFF, 0xFF, 0x6D }, { 0xFF, 0xFF, 0xFF, 0x6E },
	{ 0xFF, 0xFF, 0xFF, 0x6F }, { 0xFF, 0xFF, 0xFF, 0x70 }, { 0xFF, 0xFF, 0xFF, 0x71 },
	{ 0xFF, 0xFF, 0xFF, 0x72 }, { 0xFF, 0xFF, 0xFF, 0x73 }, { 0xFF, 0xFF, 0xFF, 0x74 },
	{ 0xFF, 0xFF, 0xFF, 0x75 }, { 0xFF, 0xFF, 0xFF, 0x76 }, { 0xFF, 0xFF, 0xFF, 0x77 },
	{ 0xFF, 0xFF, 0xFF, 0x78 }, { 0xFF, 0xFF, 0xFF, 0x79 }, { 0xFF, 0xFF, 0xFF, 0x7A },
	{ 0xFF, 0xFF, 0xFF, 0x7B }, { 0xFF, 0xFF, 0xFF, 0x7C }, { 0xFF, 0xFF, 0xFF, 0x7D },
	{ 0xFF, 0xFF, 0xFF, 0x7E }, { 0xFF, 0xFF, 0xFF, 0x7F }, { 0xFF, 0xFF, 0xFF, 0x80 },
	{ 0xFF, 0xFF, 0xFF, 0x81 }, { 0xFF, 0xFF, 0xFF, 0x82 }, { 0xFF, 0xFF, 0xFF, 0x83 },
	{ 0xFF, 0xFF, 0xFF, 0x84 }, { 0xFF, 0xFF, 0xFF, 0x85 }, { 0xFF, 0xFF, 0xFF, 0x86 },
	{ 0xFF, 0xFF, 0xFF, 0x87 }, { 0xFF, 0xFF, 0xFF, 0x88 }, { 0xFF, 0xFF, 0xFF, 0x89 },
	{ 0xFF, 0xFF, 0xFF, 0x8A }, { 0xFF, 0xFF, 0xFF, 0x8B }, { 0xFF, 0xFF, 0xFF, 0x8C },
	{ 0xFF, 0xFF, 0xFF, 0x8D }, { 0xFF, 0xFF, 0xFF, 0x8E }, { 0xFF, 0xFF, 0xFF, 0x8F },
	{ 0xFF, 0xFF, 0xFF, 0x90 }, { 0xFF, 0xFF, 0xFF, 0x91 }, { 0xFF, 0xFF, 0xFF, 0x92 },
	{ 0xFF, 0xFF, 0xFF, 0x93 }, { 0xFF, 0xFF, 0xFF, 0x94 }, { 0xFF, 0xFF, 0xFF, 0x95 },
	{ 0xFF, 0xFF, 0xFF, 0x96 }, { 0xFF, 0xFF, 0xFF, 0x97 }, { 0xFF, 0xFF, 0xFF, 0x98 },
	{ 0xFF, 0xFF, 0xFF, 0x99 }, { 0xFF, 0xFF, 0xFF, 0x9A }, { 0xFF, 0xFF, 0xFF, 0x9B },
	{ 0xFF, 0xFF, 0xFF, 0x9C }, { 0xFF, 0xFF, 0xFF, 0x9D }, { 0xFF, 0xFF, 0xFF, 0x9E },
	{ 0xFF, 0xFF, 0xFF, 0x9F }, { 0xFF, 0xFF, 0xFF, 0xA0 }, { 0xFF, 0xFF, 0xFF, 0xA1 },
	{ 0xFF, 0xFF, 0xFF, 0xA2 }, { 0xFF, 0xFF, 0xFF, 0xA3 }, { 0xFF, 0xFF, 0xFF, 0xA4 },
	{ 0xFF, 0xFF, 0xFF, 0xA5 }, { 0xFF, 0xFF, 0xFF, 0xA6 }, { 0xFF, 0xFF, 0xFF, 0xA7 },
	{ 0xFF, 0xFF, 0xFF, 0xA8 }, { 0xFF, 0xFF, 0xFF, 0xA9 }, { 0xFF, 0xFF, 0xFF, 0xAA },
	{ 0xFF, 0xFF, 0xFF, 0xAB }, { 0xFF, 0xFF, 0xFF, 0xAC }, { 0xFF, 0xFF, 0xFF, 0xAD },
	{ 0xFF, 0xFF, 0xFF, 0xAE }, { 0xFF, 0xFF, 0xFF, 0xAF }, { 0xFF, 0xFF, 0xFF, 0xB0 },
	{ 0xFF, 0xFF, 0xFF, 0xB1 }, { 0xFF, 0xFF, 0xFF, 0xB2 }, { 0xFF, 0xFF, 0xFF, 0xB3 },
	{ 0xFF, 0xFF, 0xFF, 0xB4 }, { 0xFF, 0xFF, 0xFF, 0xB5 }, { 0xFF, 0xFF, 0xFF, 0xB6 },
	{ 0xFF, 0xFF, 0xFF, 0xB7 }, { 0xFF, 0xFF, 0xFF, 0xB8 }, { 0xFF, 0xFF, 0xFF, 0xB9 },
	{ 0xFF, 0xFF, 0xFF, 0xBA }, { 0xFF, 0xFF, 0xFF, 0xBB }, { 0xFF, 0xFF, 0xFF, 0xBC },
	{ 0xFF, 0xFF, 0xFF, 0xBD }, { 0xFF, 0xFF, 0xFF, 0xBE }, { 0xFF, 0xFF, 0xFF, 0xBF },
	{ 0xFF, 0xFF, 0xFF, 0xC0 }, { 0xFF, 0xFF, 0xFF, 0xC1 }, { 0xFF, 0xFF, 0xFF, 0xC2 },
	{ 0xFF, 0xFF, 0xFF, 0xC3 }, { 0xFF, 0xFF, 0xFF, 0xC4 }, { 0xFF, 0xFF, 0xFF, 0xC5 },
	{ 0xFF, 0xFF, 0xFF, 0xC6 }, { 0xFF, 0xFF, 0xFF, 0xC7 }, { 0xFF, 0xFF, 0xFF, 0xC8 },
	{ 0xFF, 0xFF, 0xFF, 0xC9 }, { 0xFF, 0xFF, 0xFF, 0xCA }, { 0xFF, 0xFF, 0xFF, 0xCB },
	{ 0xFF, 0xFF, 0xFF, 0xCC }, { 0xFF, 0xFF, 0xFF, 0xCD }, { 0xFF, 0xFF, 0xFF, 0xCE },
	{ 0xFF, 0xFF, 0xFF, 0xCF }, { 0xFF, 0xFF, 0xFF, 0xD0 }, { 0xFF, 0xFF, 0xFF, 0xD1 },
	{ 0xFF, 0xFF, 0xFF, 0xD2 }, { 0xFF, 0xFF, 0xFF, 0xD3 }, { 0xFF, 0xFF, 0xFF, 0xD4 },
	{ 0xFF, 0xFF, 0xFF, 0xD5 }, { 0xFF, 0xFF, 0xFF, 0xD6 }, { 0xFF, 0xFF, 0xFF, 0xD7 },
	{ 0xFF, 0xFF, 0xFF, 0xD8 }, { 0xFF, 0xFF, 0xFF, 0xD9 }, { 0xFF, 0xFF, 0xFF, 0xDA },
	{ 0xFF, 0xFF, 0xFF, 0xDB }, { 0xFF, 0xFF, 0xFF, 0xDC }, { 0xFF, 0xFF, 0xFF, 0xDD },
	{ 0xFF, 0xFF, 0xFF, 0xDE }, { 0xFF, 0xFF, 0xFF, 0xDF }, { 0xFF, 0xFF, 0xFF, 0xE0 },
	{ 0xFF, 0xFF, 0xFF, 0xE1 }, { 0xFF, 0xFF, 0xFF, 0xE2 }, { 0xFF, 0xFF, 0xFF, 0xE3 },
	{ 0xFF, 0xFF, 0xFF, 0xE4 }, { 0xFF, 0xFF, 0xFF, 0xE5 }, { 0xFF, 0xFF, 0xFF, 0xE6 },
	{ 0xFF, 0xFF, 0xFF, 0xE7 }, { 0xFF, 0xFF, 0xFF, 0xE8 }, { 0xFF, 0xFF, 0xFF, 0xE9 },
	{ 0xFF, 0xFF, 0xFF, 0xEA }, { 0xFF, 0xFF, 0xFF, 0xEB }, { 0xFF, 0xFF, 0xFF, 0xEC },
	{ 0xFF, 0xFF, 0xFF, 0xED }, { 0xFF, 0xFF, 0xFF, 0xEE }, { 0xFF, 0xFF, 0xFF, 0xEF },
	{ 0xFF, 0xFF, 0xFF, 0xF0 }, { 0xFF, 0xFF, 0xFF, 0xF1 }, { 0xFF, 0xFF, 0xFF, 0xF2 },
	{ 0xFF, 0xFF, 0xFF, 0xF3 }, { 0xFF, 0xFF, 0xFF, 0xF4 }, { 0xFF, 0xFF, 0xFF, 0xF5 },
	{ 0xFF, 0xFF, 0xFF, 0xF6 }, { 0xFF, 0xFF, 0xFF, 0xF7 }, { 0xFF, 0xFF, 0xFF, 0xF8 },
	{ 0xFF, 0xFF, 0xFF, 0xF9 }, { 0xFF, 0xFF, 0xFF, 0xFA }, { 0xFF, 0xFF, 0xFF, 0xFB },
	{ 0xFF, 0xFF, 0xFF, 0xFC }, { 0xFF, 0xFF, 0xFF, 0xFD }, { 0xFF, 0xFF, 0xFF, 0xFE },
	{ 0xFF, 0xFF, 0xFF, 0xFF }
};

static Uint32 get_image_width(struct SFT *sft, const Uint32 *w)
{
	Uint32 width = 0;
	Uint32 line_width = 0;
	struct SFT_Char chr;

	SDL_assert(w != NULL);
	SDL_assert(sft != NULL);
	SDL_assert(sft->flags == 0);

	for(; *w != L'\0'; w++)
	{
		if(sft_char(sft, *w, &chr) >= 0)
		{
			line_width += (unsigned)SDL_ceil(chr.advance);
			if(*w == '\n')
			{
				if(line_width > width)
					width = line_width;

				line_width = 0;
			}
		}
	}

	if(line_width > width)
		width = line_width;

	return width;
}

static unsigned get_number_of_lines(const uint32_t *wstr)
{
	unsigned lines = 1;

	SDL_assert(wstr != NULL);

	for(; *wstr; wstr++)
	{
		if(*wstr != L'\n')
			continue;

		lines++;
	}

	return lines;
}

sdlsft *sdlsft_init(Uint32 em, const void *font_mem, unsigned long size)
{
	sdlsft *ctx = SDL_calloc(1, sizeof(sdlsft));
	double ascent_d, descent_d;

	SDL_assert(em >= 4);
	SDL_assert(font_mem != NULL);
	SDL_assert(size > 0);

	if(ctx == NULL)
		goto err;

	ctx->sft.xScale = em;
	ctx->sft.yScale = em;

	ctx->sft.font = sft_loadmem(font_mem, size);
	if(ctx->sft.font == NULL)
	{
		SDL_SetError("Unable to initialise font.");
		goto err;
	}

	if(sft_linemetrics(&ctx->sft, &ascent_d, &descent_d, NULL) != 0)
	{
		SDL_SetError("Unable to obtain font line metrics");
		goto err;
	}

	ctx->ascent = SDL_floor(ascent_d);
	ctx->descent = SDL_floor(descent_d);
	ctx->em = em;
	ctx->line_space = em / 4;

out:
	return ctx;

err:
	SDL_free(ctx);
	ctx = NULL;
	goto out;
}

/**
 * Rendered surfaces are upside-down, and must therefore be flipped before
 * drawing to screen.
*
 * \param ctx	Private context.
 * \param str	String to render. '\n' Create a new line.
 * \return	Rendered surface.
 */
SDL_Surface *sdlsft_render(sdlsft *ctx, const Uint32 *wstr)
{
	Uint32 width, height, lines;
	SDL_Surface *ret = NULL;
	SDL_Rect dst_rect = {0};

	SDL_assert(ctx != NULL);
	SDL_assert(wstr != NULL);

	width = get_image_width(&ctx->sft, wstr);
	lines = get_number_of_lines(wstr);
	SDL_assert(width > 0);
	SDL_assert(lines > 0);

	ctx->sft.flags |= SFT_RENDER_IMAGE;
	height = (lines * ctx->em) + ((lines - 1) * ctx->line_space);

	ret = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
					     SDL_PIXELFORMAT_ARGB32);
	if(ret == NULL)
		goto err;

	for(; *wstr != L'\0'; wstr++)
	{
		struct SFT_Char chr;
		SDL_Surface *src;

		/* Move to a new line*/
		if(*wstr == L'\n')
		{
			lines--;
			dst_rect.x = 0;
			if(lines > 0)
				continue;

			break;
		}

		/* Handle other non-printing characters. */
		if(*wstr < L' ')
			continue;

		if(sft_char(&ctx->sft, *wstr, &chr) < 0)
		{
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
				     "Couldn't load character 0x%04X.\n",
				     *wstr);
			continue;
		}

		src = SDL_CreateRGBSurfaceWithFormatFrom(chr.image,
							 chr.width, chr.height,
							 8, chr.width,
							 SDL_PIXELFORMAT_INDEX8);
		if(src == NULL)
		{
			SDL_free(chr.image);
			goto err;
		}

		SDL_SetPaletteColors(src->format->palette, gray2argb, 0,
				     SDL_arraysize(gray2argb));

		dst_rect.h = chr.height;
		dst_rect.w = chr.width;
		dst_rect.x += chr.x;
		dst_rect.y = ((lines - 1) * (ctx->em + ctx->line_space)) +
			chr.y - ctx->descent;

		if(SDL_BlitSurface(src, NULL, ret, &dst_rect) != 0)
		{
			SDL_free(chr.image);
			SDL_FreeSurface(src);
			goto err;
		}

		dst_rect.x += chr.advance - chr.x;

		SDL_free(chr.image);
		SDL_FreeSurface(src);
	}

out:
	return ret;

err:
	if(ret != NULL)
	{
		SDL_FreeSurface(ret);
		ret = NULL;
	}

	goto out;
}

void sdlsft_exit(sdlsft *ctx)
{
	if(ctx == NULL)
		return;

	sft_freefont(ctx->sft.font);
	SDL_free(ctx);
	ctx = NULL;
}
