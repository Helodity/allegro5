/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      Grabber datafile -> asm converter for the Allegro library.
 *
 *      By Shawn Hargreaves.
 *
 *      See readme.txt for copyright information.
 */


#define USE_CONSOLE

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "allegro.h"
#include "allegro/aintern.h"
#include "datedit.h"


#ifndef ALLEGRO_ASM_PREFIX
   #define ALLEGRO_ASM_PREFIX    ""
#endif


static int err = 0;
static int truecolor = FALSE;

static char prefix[80] = "";

static char *infilename = NULL;
static char *outfilename = NULL;
static char *outfilenameheader = NULL;
static char *password = NULL;

static DATAFILE *data = NULL;

static FILE *outfile = NULL;
static FILE *outfileheader = NULL;

static void output_object(DATAFILE *object, char *name);


/* unused callbacks for datedit.c */
void datedit_msg(AL_CONST char *fmt, ...) { }
void datedit_startmsg(AL_CONST char *fmt, ...) { }
void datedit_endmsg(AL_CONST char *fmt, ...) { }
void datedit_error(AL_CONST char *fmt, ...) { }
int datedit_ask(AL_CONST char *fmt, ...) { return 0; }



static void usage()
{
   printf("\nDatafile -> asm conversion utility for Allegro " ALLEGRO_VERSION_STR ", " ALLEGRO_PLATFORM_STR "\n");
   printf("By Shawn Hargreaves, " ALLEGRO_DATE_STR "\n\n");
   printf("Usage: dat2s [options] inputfile.dat\n\n");
   printf("Options:\n");
   printf("\t'-o outputfile.s' sets the output file (default stdout)\n");
   printf("\t'-h outputfile.h' sets the output header file (default none)\n");
   printf("\t'-p prefix' sets the object name prefix string\n");
   printf("\t'-007 password' sets the datafile password\n");
}



static void write_data(unsigned char *data, int size)
{
   int c;

   for (c=0; c<size; c++) {
      if ((c & 7) == 0)
	 fprintf(outfile, "\t.byte ");

      fprintf(outfile, "0x%02X", data[c]);

      if ((c<size-1) && ((c & 7) != 7))
	 fprintf(outfile, ", ");
      else
	 fprintf(outfile, "\n");
   }
}



static void output_data(unsigned char *data, int size, char *name, char *type, int global)
{
   fprintf(outfile, "# %s (%d bytes)\n", type, size);

   if (global)
      fprintf(outfile, ".globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, name);

   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, name);

   write_data(data, size);

   fprintf(outfile, "\n");
}



static void output_bitmap(BITMAP *bmp, char *name, int global)
{
   int bpp = bitmap_color_depth(bmp);
   int bypp = (bpp + 7) / 8;
   char buf[160];
   int c;

   if (bpp > 8)
      truecolor = TRUE;

   strcpy(buf, name);
   strcat(buf, "_data");

   output_data(bmp->line[0], bmp->w*bmp->h*bypp, buf, "bitmap data", FALSE);

   fprintf(outfile, "# bitmap\n");

   if (global)
      fprintf(outfile, ".globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, name);

   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, name);

   fprintf(outfile, "\t.long %-16d# w\n", bmp->w);
   fprintf(outfile, "\t.long %-16d# h\n", bmp->h);
   fprintf(outfile, "\t.long %-16d# clip\n", -1);
   fprintf(outfile, "\t.long %-16d# cl\n", 0);
   fprintf(outfile, "\t.long %-16d# cr\n", bmp->w);
   fprintf(outfile, "\t.long %-16d# ct\n", 0);
   fprintf(outfile, "\t.long %-16d# cb\n", bmp->h);
   fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "__linear_vtable%d\n", bpp);
   fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "_stub_bank_switch\n");
   fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "_stub_bank_switch\n");
   fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_data\n", prefix, name);
   fprintf(outfile, "\t.long %-16d# bitmap_id\n", 0);
   fprintf(outfile, "\t.long %-16d# extra\n", 0);
   fprintf(outfile, "\t.long %-16d# x_ofs\n", 0);
   fprintf(outfile, "\t.long %-16d# y_ofs\n", 0);
   fprintf(outfile, "\t.long %-16d# seg\n", 0);

   for (c=0; c<bmp->h; c++)
      fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_data + %d\n", prefix, name, bmp->w*c*bypp);

   fprintf(outfile, "\n");
}



static void output_sample(SAMPLE *spl, char *name)
{
   char buf[160];

   strcpy(buf, name);
   strcat(buf, "_data");

   output_data(spl->data, spl->len * ((spl->bits==8) ? 1 : sizeof(short)) * ((spl->stereo) ? 2 : 1), buf, "waveform data", FALSE);

   fprintf(outfile, "# sample\n.globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, name);
   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, name);
   fprintf(outfile, "\t.long %-16d# bits\n", spl->bits);
   fprintf(outfile, "\t.long %-16d# stereo\n", spl->stereo);
   fprintf(outfile, "\t.long %-16d# freq\n", spl->freq);
   fprintf(outfile, "\t.long %-16d# priority\n", spl->priority);
   fprintf(outfile, "\t.long %-16ld# length\n", spl->len);
   fprintf(outfile, "\t.long %-16ld# loop_start\n", spl->loop_start);
   fprintf(outfile, "\t.long %-16ld# loop_end\n", spl->loop_end);
   fprintf(outfile, "\t.long %-16ld# param\n", spl->param);
   fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_data\n\n", prefix, name);
}



static void output_midi(MIDI *midi, char *name)
{
   char buf[160];
   int c;

   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi->track[c].data) {
	 sprintf(buf, "%s_track_%d", name, c);
	 output_data(midi->track[c].data, midi->track[c].len, buf, "midi track", FALSE);
      }
   }

   fprintf(outfile, "# midi file\n.globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, name);
   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, name);
   fprintf(outfile, "\t.long %-16d# divisions\n", midi->divisions);

   for (c=0; c<MIDI_TRACKS; c++)
      if (midi->track[c].data)
	 fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_track_%d, %d\n", prefix, name, c, midi->track[c].len);
      else
	 fprintf(outfile, "\t.long 0, 0\n");

   fprintf(outfile, "\n");
}



static void output_font(FONT *f, char *name, int depth)
{
/*   FONT_GLYPH *g;
   char buf[160], goodname[160];
   int c;

   if (f->next)
      output_font(f->next, name, depth+1);

   if (depth > 0)
      sprintf(goodname, "%s_r%d", name, depth+1);
   else
      strcpy(goodname, name);

   for (c=f->start; c<=f->end; c++) {
      sprintf(buf, "%s_char_%04X", goodname, c);

      if (f->mono) {
	 g = f->glyphs[c-f->start];

	 fprintf(outfile, "# glyph\n");
	 fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, buf);
	 fprintf(outfile, "\t.short %-15d# w\n", g->w);
	 fprintf(outfile, "\t.short %-15d# h\n", g->h);

	 write_data(g->dat, ((g->w+7)/8) * g->h);

	 fprintf(outfile, "\n");
      }
      else
	 output_bitmap(f->glyphs[c-f->start], buf, FALSE);
   }

   fprintf(outfile, "# glyph list\n");
   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s_glyphs:\n", prefix, goodname);

   for (c=f->start; c<=f->end; c++)
      fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_char_%04X\n", prefix, goodname, c);

   fprintf(outfile, "\n");

   fprintf(outfile, "# font\n");

   if (depth == 0)
      fprintf(outfile, ".globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, goodname);

   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, goodname);
   fprintf(outfile, "\t.long %-16d# mono\n", f->mono);
   fprintf(outfile, "\t.long 0x%04X%10c# start\n", f->start, ' ');
   fprintf(outfile, "\t.long 0x%04X%10c# end\n", f->end, ' ');
   fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_glyphs\n", prefix, goodname);

   if (f->next)
      fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_r%d\n", prefix, name, depth+2);
   else
      fprintf(outfile, "\t.long %-16d# next\n", 0);

   fprintf(outfile, "\t.long %-16d# renderhook\n", 0);
   fprintf(outfile, "\t.long %-16d# widthhook\n", 0);
   fprintf(outfile, "\t.long %-16d# heighthook\n", 0);
   fprintf(outfile, "\t.long %-16d# destroyhook\n", 0);
   fprintf(outfile, "\n");*/
}



static void output_rle_sprite(RLE_SPRITE *sprite, char *name)
{
   int bpp = sprite->color_depth;

   if (bpp > 8)
      truecolor = TRUE;

   fprintf(outfile, "# RLE sprite\n.globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, name);
   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, name);
   fprintf(outfile, "\t.long %-16d# w\n", sprite->w);
   fprintf(outfile, "\t.long %-16d# h\n", sprite->h);
   fprintf(outfile, "\t.long %-16d# color depth\n", bpp);
   fprintf(outfile, "\t.long %-16d# size\n", sprite->size);

   write_data(sprite->dat, sprite->size);

   fprintf(outfile, "\n");
}



#ifndef ALLEGRO_USE_C

static void output_compiled_sprite(COMPILED_SPRITE *sprite, char *name)
{
   char buf[160];
   int c;

   if (sprite->color_depth != 8) {
      fprintf(stderr, "Error: truecolor compiled sprites not supported (%s)\n", name);
      err = 1;
      return;
   }

   for (c=0; c<4; c++) {
      if (sprite->proc[c].draw) {
	 sprintf(buf, "%s_plane_%d", name, c);
	 output_data(sprite->proc[c].draw, sprite->proc[c].len, buf, "compiled sprite code", FALSE);
      }
   } 

   fprintf(outfile, "# compiled sprite\n.globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, name);
   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, name);
   fprintf(outfile, "\t.short %-15d# planar\n", sprite->planar);
   fprintf(outfile, "\t.short %-15d# color depth\n", sprite->color_depth);
   fprintf(outfile, "\t.short %-15d# w\n", sprite->w);
   fprintf(outfile, "\t.short %-15d# h\n", sprite->h);

   for (c=0; c<4; c++) {
      if (sprite->proc[c].draw) {
	 fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s_plane_%d\n", prefix, name, c);
	 fprintf(outfile, "\t.long %-16d# len\n", sprite->proc[c].len);
      }
      else {
	 fprintf(outfile, "\t.long 0\n");
	 fprintf(outfile, "\t.long %-16d# len\n", 0);
      }
   }

   fprintf(outfile, "\n");
}

#endif      /* ifndef ALLEGRO_USE_C */



static void get_object_name(char *buf, char *name, DATAFILE *dat, int root)
{
   if (!root) {
      strcpy(buf, name);
      strcat(buf, "_");
   }
   else
      buf[0] = 0;

   strcat(buf, get_datafile_property(dat, DAT_NAME));
   strlwr(buf);
}



static void output_datafile(DATAFILE *dat, char *name, int root)
{
   char buf[160];
   int c;

   for (c=0; (dat[c].type != DAT_END) && (!err); c++) {
      get_object_name(buf, name, dat+c, root);
      output_object(dat+c, buf);
   }

   fprintf(outfile, "# datafile\n.globl " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, name);
   fprintf(outfile, ".balign 4\n" ALLEGRO_ASM_PREFIX "%s%s:\n", prefix, name);

   if (outfileheader)
      fprintf(outfileheader, "extern DATAFILE %s%s[];\n", prefix, name);

   for (c=0; dat[c].type != DAT_END; c++) {
      get_object_name(buf, name, dat+c, root);
      fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "%s%s\n", prefix, buf);
      fprintf(outfile, "\t.long %-16d# %c%c%c%c\n", dat[c].type, (dat[c].type>>24) & 0xFF, (dat[c].type>>16) & 0xFF, (dat[c].type>>8) & 0xFF, dat[c].type & 0xFF);
      fprintf(outfile, "\t.long %-16ld# size\n", dat[c].size);
      fprintf(outfile, "\t.long %-16d# properties\n", 0);
   }

   fprintf(outfile, "\t.long 0\n\t.long -1\n\t.long 0\n\t.long 0\n\n");
}



static void output_object(DATAFILE *object, char *name)
{
   char buf[160];
   int i;

   switch (object->type) {

      case DAT_FONT:
	 if (outfileheader)
	    fprintf(outfileheader, "extern FONT %s%s;\n", prefix, name);

	 output_font((FONT *)object->dat, name, 0);
	 break;

      case DAT_BITMAP:
	 if (outfileheader)
	    fprintf(outfileheader, "extern BITMAP %s%s;\n", prefix, name);

	 output_bitmap((BITMAP *)object->dat, name, TRUE);
	 break;

      case DAT_PALETTE:
	 if (outfileheader)
	    fprintf(outfileheader, "extern PALETTE %s%s;\n", prefix, name);

	 output_data(object->dat, sizeof(PALETTE), name, "palette", TRUE);
	 break;

      case DAT_SAMPLE:
	 if (outfileheader)
	    fprintf(outfileheader, "extern SAMPLE %s%s;\n", prefix, name);

	 output_sample((SAMPLE *)object->dat, name);
	 break;

      case DAT_MIDI:
	 if (outfileheader)
	    fprintf(outfileheader, "extern MIDI %s%s;\n", prefix, name);

	 output_midi((MIDI *)object->dat, name);
	 break;

      case DAT_PATCH:
	 printf("Compiled GUS patch objects are not supported!\n");
	 break;

      case DAT_RLE_SPRITE:
	 if (outfileheader)
	    fprintf(outfileheader, "extern RLE_SPRITE %s%s;\n", prefix, name);

	 output_rle_sprite((RLE_SPRITE *)object->dat, name);
	 break;

      case DAT_FLI:
	 if (outfileheader)
	    fprintf(outfileheader, "extern unsigned char %s%s[];\n", prefix, name);

	 output_data(object->dat, object->size, name, "FLI/FLC animation", TRUE);
	 break;

      case DAT_C_SPRITE:
      case DAT_XC_SPRITE:
	 if (outfileheader)
	    fprintf(outfileheader, "extern COMPILED_SPRITE %s%s;\n", prefix, name);

	 #ifdef ALLEGRO_USE_C
	    output_rle_sprite((RLE_SPRITE *)object->dat, name);
	 #else
	    output_compiled_sprite((COMPILED_SPRITE *)object->dat, name);
	 #endif
	 break;

      case DAT_FILE:
	 output_datafile((DATAFILE *)object->dat, name, FALSE);
	 break;

      default:
	 for (i=0; datedit_object_info[i]->type != DAT_END; i++) {
	    if ((datedit_object_info[i]->type == object->type) && (datedit_object_info[i]->dat2s)) {
	       strcpy(buf, prefix);
	       strcat(buf, name);
	       datedit_object_info[i]->dat2s(object, buf, outfile, outfileheader);
	       return;
	    }
	 }

	 if (outfileheader)
	    fprintf(outfileheader, "extern unsigned char %s%s[];\n", prefix, name);

	 output_data(object->dat, object->size, name, "binary data", TRUE);
	 break;
   }
}



int main(int argc, char *argv[])
{
   int c;
   char tm[80];
   time_t now;

   install_allegro(SYSTEM_NONE, &errno, atexit);
   datedit_init();

   time(&now);
   strcpy(tm, asctime(localtime(&now)));
   for (c=0; tm[c]; c++)
      if ((tm[c] == '\r') || (tm[c] == '\n'))
	 tm[c] = 0;

   for (c=1; c<argc; c++) {
      if (stricmp(argv[c], "-o") == 0) {
	 if ((outfilename) || (c >= argc-1)) {
	    usage();
	    return 1;
	 }
	 outfilename = argv[++c];
      }
      else if (stricmp(argv[c], "-h") == 0) {
	 if ((outfilenameheader) || (c >= argc-1)) {
	    usage();
	    return 1;
	 }
	 outfilenameheader = argv[++c];
      }
      else if (stricmp(argv[c], "-p") == 0) {
	 if ((prefix[0]) || (c >= argc-1)) {
	    usage();
	    return 1;
	 }
	 strcpy(prefix, argv[++c]);
      }
      else if (stricmp(argv[c], "-007") == 0) {
	 if ((password) || (c >= argc-1)) {
	    usage();
	    return 1;
	 }
	 password = argv[++c];
      }
      else {
	 if ((argv[c][0] == '-') || (infilename)) {
	    usage();
	    return 1;
	 }
	 infilename = argv[c];
      }
   }

   if (!infilename) {
      usage();
      return 1;
   }

   if ((prefix[0]) && (prefix[strlen(prefix)-1] != '_'))
      strcat(prefix, "_");

   set_color_conversion(COLORCONV_NONE);

   data = datedit_load_datafile(infilename, TRUE, password);
   if (!data) {
      fprintf(stderr, "Error reading %s\n", infilename);
      err = 1; 
      goto ohshit;
   }

   if (outfilename) {
      outfile = fopen(outfilename, "w");
      if (!outfile) {
	 fprintf(stderr, "Error writing %s\n", outfilename);
	 err = 1; 
	 goto ohshit;
      }
   }
   else
      outfile = stdout;

   fprintf(outfile, "/* Compiled Allegro data file, produced by dat2s v" ALLEGRO_VERSION_STR ", " ALLEGRO_PLATFORM_STR " */\n");
   fprintf(outfile, "/* Input file: %s */\n", infilename);
   fprintf(outfile, "/* Date: %s */\n", tm);
   fprintf(outfile, "/* Do not hand edit! */\n\n.data\n\n");

   if (outfilenameheader) {
      outfileheader = fopen(outfilenameheader, "w");
      if (!outfileheader) {
	 fprintf(stderr, "Error writing %s\n", outfilenameheader);
	 err = 1; 
	 goto ohshit;
      }
      fprintf(outfileheader, "/* Allegro data file definitions, produced by dat2s v" ALLEGRO_VERSION_STR ", " ALLEGRO_PLATFORM_STR " */\n");
      fprintf(outfileheader, "/* Input file: %s */\n", infilename);
      fprintf(outfileheader, "/* Date: %s */\n", tm);
      fprintf(outfileheader, "/* Do not hand edit! */\n\n");
   }

   if (outfilename)
      printf("Converting %s to %s...\n", infilename, outfilename);

   output_datafile(data, "data", TRUE);

   #ifdef ALLEGRO_DJGPP

      fprintf(outfile, ".text\n");
      fprintf(outfile, ".balign 4\n");
      fprintf(outfile, ALLEGRO_ASM_PREFIX "_construct_me:\n");
      fprintf(outfile, "\tpushl %%ebp\n");
      fprintf(outfile, "\tmovl %%esp, %%ebp\n");
      fprintf(outfile, "\tpushl $" ALLEGRO_ASM_PREFIX "%sdata\n", prefix);
      fprintf(outfile, "\tcall " ALLEGRO_ASM_PREFIX "_construct_datafile\n");
      fprintf(outfile, "\taddl $4, %%esp\n");
      fprintf(outfile, "\tleave\n");
      fprintf(outfile, "\tret\n\n");
      fprintf(outfile, ".section .ctor\n");
      fprintf(outfile, "\t.long " ALLEGRO_ASM_PREFIX "_construct_me\n");

   #endif

   if ((outfile && ferror(outfile)) || (outfileheader && ferror(outfileheader)))
      err = 1;

   ohshit:

   if ((outfile) && (outfile != stdout))
      fclose(outfile);

   if (outfileheader)
      fclose(outfileheader);

   if (data)
      unload_datafile(data);

   if (err) {
      if (outfilename)
	 delete_file(outfilename);

      if (outfilenameheader)
	 delete_file(outfilenameheader);
   }
   else {
      #ifdef ALLEGRO_DJGPP
	 if (truecolor) {
	    printf("\nI noticed some truecolor images, so you must call fixup_datafile()\n");
	    printf("before using this data! (after setting a video mode).\n");
	 }
      #else
	 printf("\nI don't know how to do constructor functions on this platform, so you must\n");
	 printf("call fixup_datafile() before using this data! (after setting a video mode).\n");
      #endif
   }

   return err;
}

END_OF_MAIN();
