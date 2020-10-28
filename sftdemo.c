#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "schrift.h"
#include "utf8d.h"

int utf8_utf32(char *s, uint32_t **w)
{
	uint32_t state = UTF8_ACCEPT;
	uint32_t *i;

	SDL_assert(s != NULL);
	SDL_assert(w != NULL);

	/* If null, allocate enough memory for string. */
	if(*w == NULL)
	{
		size_t sz = strlen(s) + 1;
		*w = malloc(sz * sizeof(uint32_t));
		if(*w == NULL)
			return 1;
	}

	i = *w;
	for(; *s; s++)
	{
		decode(&state, i, *s);
		i++;
	}

	*i = '\0';

	return state;
}

unsigned get_image_width(struct SFT *sft, uint32_t *w)
{
	unsigned width = 0;
	unsigned line_width = 0;
	struct SFT_Char chr;

	SDL_assert(w != NULL);

	for(; *w; w++)
	{
		if(sft_char(sft, *w, &chr) >= 0)
		{
			line_width += (unsigned)ceil(chr.advance);
			if(*w == '\n' && line_width > width)
			{
				width = line_width;
				line_width = 0;
			}
		}
	}

	if(line_width > width)
		width = line_width;

	return width;
}

unsigned get_number_of_lines(uint32_t *w)
{
	unsigned lines = 1;

	SDL_assert(w != NULL);

	for(; *w; w++)
	{
		if(*w != L'\n')
			continue;

		lines++;
	}

	return lines;
}

char *open_and_read_file(const char *filename)
{
	FILE *f;
	size_t s;
	char *ret = NULL;

	SDL_assert(filename != NULL);

	f = fopen(filename, "rb");
	if(f == NULL)
		goto out;

	fseek(f, 0, SEEK_END);
	s = ftell(f);
	rewind(f);

	ret = malloc(s + 1);
	if(ret == NULL)
	{
		fclose(f);
		goto out;
	}

	s = fread(ret, 1, s, f);
	fclose(f);

	ret[s] = '\0';

out:
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	char *str = NULL;
	char *arg_text, *arg_bmp, *arg_font;
	struct SFT sft = {
		.xScale = 64, .yScale = 64
	};
	SDL_Surface *image;
	uint32_t *wstr = NULL;
	int width = 0;
	unsigned lines;
	double ascent, descent, gap;
	unsigned u_descent;
	const unsigned line_space = 64 / 4;

	if(argc != 4)
	{
		puts("Usage: sch2bmp TEXT.txt OUT.bmp FONT.ttf");
		return 1;
	}

	/* Assign arguments. */
	arg_text = argv[1];
	arg_bmp = argv[2];
	arg_font = argv[3];

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
		goto err;

	/* Prepare font. */
	sft.font = sft_loadfile(arg_font);
	if(sft.font == NULL)
	{
		SDL_SetError("Unable to initialise font");
		goto err;
	}

	if(sft_linemetrics(&sft, &ascent, &descent, &gap) != 0)
	{
		SDL_SetError("Unable to obtain font line metrics");
		goto err;
	}

	u_descent = round(descent);

	str = open_and_read_file(arg_text);
	if(str == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Unable to read text file %s", arg_text);
		goto err;
	}

	ret = utf8_utf32(str, &wstr);
	if(ret != 0)
		goto err;

	width = get_image_width(&sft, wstr);
	lines = get_number_of_lines(wstr);

	SDL_assert(width > 0);
	SDL_assert(lines > 0);

	/* Render image after we get the destination width. */
	sft.flags |= SFT_RENDER_IMAGE;

	unsigned image_height = (lines * 64) + ((lines - 1) * line_space);
	image = SDL_CreateRGBSurfaceWithFormat(0, width,
					       image_height, 32,
					       SDL_PIXELFORMAT_ARGB32);
	SDL_assert(image != NULL);

	SDL_Color colors[256];
	memset(colors, 0xFF, sizeof(colors));
	for(int i = 0; i < 256; i++)
		colors[i].a = i;

	for(uint32_t *wide = wstr; *wide != L'\0'; wide++)
	{
		struct SFT_Char chr;
		SDL_Surface *src;
		SDL_Rect src_rect;
		static SDL_Rect dst_rect = {0};

		if(*wide == L'\r')
			continue;

		if(*wide == L'\n')
		{
			lines--;
			dst_rect.x = 0;
			if(lines > 0)
				continue;

			break;
		}

		if(sft_char(&sft, *wide, &chr) < 0)
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
				    "Couldn't load character 0x%04X.\n", *wide);
			continue;
		}

		src = SDL_CreateRGBSurfaceWithFormatFrom(chr.image,
							 chr.width, chr.height,
							 8, chr.width,
							 SDL_PIXELFORMAT_INDEX8);
		SDL_assert(src != NULL);
		SDL_SetPaletteColors(src->format->palette, colors, 0, 256);

#if 0
		src_rect.h = chr.height;
		src_rect.w = chr.width;
		src_rect.x = chr.x;
		src_rect.y = chr.y;
#endif

		dst_rect.h = chr.height;
		dst_rect.w = chr.width;
		dst_rect.x += chr.x;
		dst_rect.y = chr.y + ((lines - 1) * (64 + line_space)) + u_descent;

		fprintf(stdout, "%c (%d,%d) (%d, %d) adv: %f\n",
			*wide, chr.x, chr.y, chr.width, chr.height, chr.advance);
		fflush(stdout);

		{
			int blitret = SDL_BlitSurface(src, NULL, image, &dst_rect);
			SDL_assert(blitret == 0);
		}

		dst_rect.x += chr.advance - chr.x;

		free(chr.image);
		SDL_FreeSurface(src);
	}

	SDL_SaveBMP(image, arg_bmp);
	{
		FILE *raw = fopen("output.data", "wb");
		SDL_LockSurface(image);
		fwrite(image->pixels, 1, image->h * image->pitch, raw);
		fclose(raw);
	}

	SDL_FreeSurface(image);

	sft_freefont(sft.font);
	ret = EXIT_SUCCESS;

out:
	free(wstr);
	free(str);
	SDL_Quit();
	return ret;

err:
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error: %s",
		     SDL_GetError());
	goto out;
}
