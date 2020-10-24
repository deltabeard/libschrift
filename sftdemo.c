#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>

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
	struct SFT_Char chr;

	SDL_assert(w != NULL);

	for(; *w; w++)
	{
		if(sft_char(sft, *w, &chr) >= 0)
			width += chr.advance;
	}

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

	/* Render image after we get the destination width. */
	sft.flags |= SFT_RENDER_IMAGE;

	image = SDL_CreateRGBSurfaceWithFormat(0, width,
			lines * 64, 32,
			SDL_PIXELFORMAT_ARGB32);

	SDL_Color colors[256];
	for(int i = 0; i < 256; i++)
	{
		colors[i].r = colors[i].g = colors[i].b = i;
		colors[i].a = i;
	}

	SDL_SetPaletteColors(image->format->palette, colors, 0, 256);

	for(uint32_t *wide = wstr; *wide != L'\0'; wide++)
	{
		struct SFT_Char chr;
		static SDL_Rect dstr = { 0 };
		SDL_Surface *src;

		if(*wide == L'\n')
		{
			dstr.y += 64;
			continue;
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
		SDL_SetPaletteColors(src->format->palette, colors, 0, 256);

		dstr.y = chr.y;
		dstr.w = chr.width;
		dstr.h = chr.height;

		fprintf(stdout, "%c (%d,%d) (%d, %d) adv: %f\n",
				*wide, chr.x, chr.y, chr.width, chr.height, chr.advance);

		SDL_BlitSurface(src, NULL, image, &dstr);
		dstr.x += chr.advance;

		SDL_FreeSurface(src);
		SDL_free(chr.image);
		fflush(stdout);
	}

	SDL_SaveBMP(image, arg_bmp);
	SDL_FreeSurface(image);
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
