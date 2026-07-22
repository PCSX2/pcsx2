/******************************************************************************
 * pixma_parse.c parser for Canon BJL printjobs
 * Copyright (c) 2005 - 2007 Sascha Sommer <saschasommer@freenet.de>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>



#define DEBUG 0 /* 1 for debugging only: all output goes to stderr */

#include "pixma_parse.h"

#include "common/Console.h"


/*TODO:
  1. change color loops to search for each named color rather than using a predefined order.
  2. keep iP6700 workaround, but check what happens with the real printer.
*/

/* redirection for debug output */
FILE* fout = stdout;

/* nextcmd(): find a command in a printjob
 * commands in the printjob start with either ESC[ or ESC(
 * ESC@ (go to neutral mode) and 0xc (form feed) are handled directly
 * params:
 *  infile: handle to the printjob
 *  cmd: the command type
 *  buf: the buffer to hold the arguments, has to be at least 0xFFFF bytes
 *  cnt: len of the arguments
 *
 * return values:
 *   0 when a command has been successfully read
 *   1 when EOF has been reached
 *  -1 when an error occurred
 */
static int nextcmd( FILE *infile,unsigned char* cmd,unsigned char *buf, unsigned int *cnt, unsigned int *xml_read,unsigned int startxmllen,unsigned int endxmllen)
{
	unsigned char c1,c2;
	unsigned int startxml, endxml;
	if (feof(infile))
		return -1;
	while (!feof(infile)){
		c1 = fgetc(infile);
		if(feof(infile)) /* NORMAL EOF */
			return 1;
		/* add skip for XML header and footer */
		if (c1 == 60 ){  /* "<" for XML start */
		  if (*xml_read==0){
		    /* start */
		    startxml=startxmllen-1;
		    fread(buf,1,startxml,infile); /* 1 less than startxmllen */
		    fprintf(fout,"nextcmd: read starting XML %d %d\n", *xml_read, startxml);
		    *xml_read=1;
		  }else if (*xml_read==1) {
		    /* end */
		    endxml=endxmllen-1;
		    fread(buf,1,endxml,infile); /* 1 less than endxmllen */
		    fprintf(fout,"nextcmd: read ending XML %d %d\n", *xml_read, endxml);
		    *xml_read=2;
		  }
		  /* no alternatives yet */
		}else if (c1 == 27 ){  /* A new ESC command */
			c2 = fgetc(infile);
			if(feof(infile))
				return 1;
			if (c2=='[' || c2=='(' ){   /* ESC[ or ESC( command */
				*cmd = fgetc(infile);    /* read command type  (1 byte) */
				if(feof(infile))
					return 1;
				c1 = fgetc(infile); /* read size 16 bit little endian */
				c2 = fgetc(infile);
				*cnt = c1 + (c2<<8);
				if (*cnt){  /* read arguments */
					unsigned int read;
					if((read=fread(buf,1,*cnt,infile)) != *cnt){
						fprintf(fout,"nextcmd: read error - not enough data %d %d\n", read, *cnt);
						return -1;
					}
				}
				return 0;
			}else if(c2 == '@'){ /* ESC@ */
				fprintf(fout,"ESC @ Return to neutral mode\n");
			} else {
				fprintf(fout,"unknown byte following ESC %x \n",c2);
			}
		}else if(c1==0x0c){ /* Form Feed */
			fprintf(fout,"-->Form Feed\n");
		}else{
			fprintf(fout,"UNKNOWN BYTE 0x%x @ %lu\n",c1,ftell(infile));
		}
	}
	return -1;
}


/* return pointer to color info structure matching name */
static color_t* get_color(image_t* img,char name){
	int i;
	for(i=0;i<MAX_COLORS;i++)
		if(img->color[i].name==name)
			return &(img->color[i]);
	return NULL;
}

#if 0
/* return pointer to color info structure matching name less 0x80 */
static color_t* get_color2(image_t* img,char name){
	int i;
	for(i=0;i<MAX_COLORS;i++) {
	  /*printf("get_color2: %i -- name=%c\n",i,img->color[i].name);*/
	  if(img->color[i].name==(name)) { /* add 0x80 to get the hex value in the inkset */
	    /*printf("get_color2: %i returning for %c\n",i,img->color[i].name);*/
	    return &(img->color[i]);
	  }
	}
	return NULL;
}
#endif

static int valid_color(unsigned char color){
	int i;
	for(i=0;i<sizeof(valid_colors) / sizeof(valid_colors[0]);i++)
		if(valid_colors[i] == color)
			return 1;
	fprintf(fout," [valid_color] unknown color 0x%x\n",color);
	return 0;
}


/* eight2ten()
 * decompression routine for canons 10to8 compression that stores 5 3-valued pixels in 8-bit
 */
static int eight2ten(unsigned char* inbuffer,unsigned char* outbuffer,int num_bytes,int outbuffer_size){
	PutBitContext s;
	int read_pos=0;
	init_put_bits(&s, outbuffer,outbuffer_size);
	while(read_pos < num_bytes){
		unsigned short value=Table8[inbuffer[read_pos]];
		++read_pos;
		put_bits(&s,10,value);
	}
	return s.buf_ptr-s.buf;
}

/* decompression routine for 3 4-bit pixels of 5 levels each */
static int eight2twelve(unsigned char* inbuffer,unsigned char* outbuffer,int num_bytes,int outbuffer_size){
	PutBitContext s;
	int read_pos=0;
	init_put_bits(&s, outbuffer,outbuffer_size);
	while(read_pos < num_bytes){
		unsigned short value=Table5Level[inbuffer[read_pos]];
		++read_pos;
		put_bits(&s,12,value);
	}
	return s.buf_ptr-s.buf;
}

#if 0
static int analysiseight2twelve(unsigned char* inbuffer,unsigned char* outbuffer,int num_bytes,int outbuffer_size){
	PutBitContext s;
	int maxlevels;
	int maxnum;
	int read_pos=0;
	init_put_bits(&s, outbuffer,outbuffer_size);
	while(read_pos < num_bytes){
		unsigned short value=Table5Level[inbuffer[read_pos]];
		++read_pos;
		/*put_bits(&s,12,value);*/
		if (value>125) {
		  maxlevels+=1;
		  if (value>maxnum) {
		    maxnum=value;
		  }
		}
	}
	/*return s.buf_ptr-s.buf;*/
	return maxnum;
}
#endif

/* decompression routine for 3 4-bit pixels of 6 levels each */
static int eight2twelve2(unsigned char* inbuffer,unsigned char* outbuffer,int num_bytes,int outbuffer_size){
	PutBitContext s;
	int read_pos=0;
	init_put_bits(&s, outbuffer,outbuffer_size);
	while(read_pos < num_bytes){
	  unsigned short value=Table6Level[inbuffer[read_pos]];
		++read_pos;
		put_bits(&s,12,value);
	}
	return s.buf_ptr-s.buf;
}

#if 0
static int analysiseight2twelve2(unsigned char* inbuffer,unsigned char* outbuffer,int num_bytes,int outbuffer_size){
	PutBitContext s;
	int maxlevels;
	int maxnum;
	int read_pos=0;
	init_put_bits(&s, outbuffer,outbuffer_size);
	while(read_pos < num_bytes){
		unsigned short value=Table6Level[inbuffer[read_pos]];
		++read_pos;
		/*put_bits(&s,12,value);*/
		if (value>216) {
		  maxlevels+=1;
		  if (value>maxnum) {
		    maxnum=value;
		  }
		}
	}
	/*return s.buf_ptr-s.buf;*/
	return maxnum;
}
#endif

/* reads a run length encoded block of raster data, decodes and uncompresses it */
static int Raster(image_t* img,unsigned char* buffer,unsigned int len,unsigned char color_name,unsigned int maxw){
	color_t* color=get_color(img,color_name);
        char* buf = (char*)buffer;
	int size=0; /* size of unpacked buffer */
	int cur_line=0; /* line relative to block begin */
	unsigned char* dst=(unsigned char*)malloc(len*256); /* the destination buffer */
	unsigned char* dstr=dst;

#if 0
	/*int numbigvals;*/ /* number of values greater than number of decompression table max index value */
	int maxtablevalue; /* try to catch the range of table values needed for decompression table */
	maxtablevalue=0;
#endif

	/* if(!color){
	   printf("no matching color for %c (0x%x, %i) in the database => ignoring %i bytes\n",color_name,color_name,color_name, len);
	   } */
	if (DEBUG) {
	  fprintf(fout,"DEBUG enter Raster len=%i,color=%c\n",len,color_name);
	}

	/* decode pack bits */
	while( len > 0){ /* why does this not work: because unsigned integer wraps! */
		int c = *buf;
		++buf;
		--len;

		/*printf("DEBUG top of while loop len=%i\n",len);*/

		if(c >= 128)
			c -=256;
		if(c== -128){ /* end of line => decode and copy things here */
		  /*printf("DEBUG end of line---decode and copy things here\n");*/
			/* create new list entry */
			if(color && size){
				if(!color->tail)
					color->head = color->tail = color->pos = (rasterline_t *)calloc(1,sizeof(rasterline_t));
				else {
					color->head->next = (rasterline_t *)calloc(1,sizeof(rasterline_t));
					color->head=color->head->next;
				}
				color->head->line = img->height + cur_line;
				if(!color->compression){
					color->head->buf=(unsigned char *)calloc(1,size+8); /* allocate slightly bigger buffer for get_bits */
					memcpy(color->head->buf,dstr,size);
					color->head->len=size;
					if (DEBUG) {
					  printf("DEBUG color not compressed\n");
					}
				}else{
				  if (img->color->bpp==2) {/* handle 5pixel in 8 bits compression --- this is pixel-packing rather than compression, just not wasting space */
				    color->head->buf=(unsigned char *)calloc(1,size*2+8);
				    size=color->head->len=eight2ten(dstr,color->head->buf,size,size*2);
				    /*printf("DEBUG 3-level color compressed\n");*/
				  } else if(img->color->bpp==4){ /* handle 4-bit ink compression */
				    if (img->color->level==5) {/* 5-level compression --- this is pixel-packing rather than compression, just not wasting space */
				      color->head->buf=(unsigned char *)calloc(1,size*2+8);
				      size=color->head->len=eight2twelve(dstr,color->head->buf,size,size*2);
				      if (DEBUG) {
					printf("DEBUG 5-level color compressed\n");
				      }
				      /*maxtablevalue=analysiseight2twelve(dstr,color->head->buf,size,size*2);
				      if (maxtablevalue!=0) {
					printf("maxtablevalue: %x",maxtablevalue);
					}*/
				    } else if (img->color->level==6) { /* 6-level compression --- this is pixel-packing rather than compression, just not wasting space */
				      color->head->buf=(unsigned char *)calloc(1,size*2+8);
				      size=color->head->len=eight2twelve2(dstr,color->head->buf,size,size*2);
				      if (DEBUG) {
					printf("DEBUG 6-level color compressed\n");
				      }
				      /*maxtablevalue=analysiseight2twelve2(dstr,color->head->buf,size,size*2);
				      if (maxtablevalue!=0) {
					printf("maxtablevalue: %x",maxtablevalue);
					}*/
				    }
				  }
				}
			}
			/* adjust the maximum image width */
			if(color && color->bpp && img->width < size*8/color->bpp){
				unsigned int newwidth = size * 8 / color->bpp;
				if(maxw && newwidth > maxw)
					newwidth = maxw;
				img->width = newwidth;
			}
			/* reset output buffer */
			size=0;
			dst=dstr;
			++cur_line;
		}else {
			int i;
			if(c < 0){ /* repeat char */
				i= - c + 1;
				c=*buf;
				++buf;
				--len;
				/*printf("DEBUG repeat character %i\n",len);*/
				memset(dst,c,i);
				dst +=i;
				size+=i;
			}else{ /* normal code */
			        /*printf("DEBUG normal code\n");*/
				i=c+1;
				len-=i;
				/*printf("DEBUG before memcpy dst,buf,i. %d, %i\n",i,len);*/
				memcpy( dst, buf, i);
				buf +=i;
				dst +=i;
				size+=i;
			}
		}
	}
	free(dstr);
	return 0;
}


/* checks if the buffer contains a pixel definition at the given x and y position */
static inline int inside_range(color_t* c,int x,int y){
  /* debug*/
  /*  if ((c->name=='C' || c->name=='M' || c->name=='Y' || c->name=='c' || c->name=='m') && x==0 && y==0){
    printf("%c: bpp: %d\n",c->name,c->bpp);
    printf("%c: y: %d\n",c->name,y);
    printf("%c: c->pos->line: %d\n",c->name,c->pos->line);
    printf("%c: c->pos->len: %d\n",c->name,c->pos->len);
    printf("%c: x: %d\n",c->name,x);
    printf("%c: c->bpp*x: %d\n",c->name,c->bpp*x);
    printf("%c: c->pos->len *8: %d\n",c->name,c->pos->len *8);
    printf("---\n");
    }*/
	if(c->bpp && c->pos &&  c->pos->line==y && c->pos->len && (c->pos->len *8 >= c->bpp*x))
		return 1;
	return 0;
}


/* moves all iterators in the rasterline list */
static void advance(image_t* img,unsigned int to){
	int i;
	for(i=0;i<MAX_COLORS;i++){
		while(img->color[i].pos && img->color[i].pos->line < to && img->color[i].pos->next && img->color[i].pos->next->line <= to){
			img->color[i].pos = img->color[i].pos->next;
			if(!img->color[i].pos->next)
				break;
		}
	}
}

/* write 1 line of decoded raster data */
static void write_line(image_t*img,FILE* fp,int pos_y){
	int i;
	unsigned int x;
	unsigned int written;
	unsigned char* line=(unsigned char *)malloc(img->width*3);
	color_t* C=get_color(img,'C');
	color_t* M=get_color(img,'M');
	color_t* Y=get_color(img,'Y');
	color_t* K=get_color(img,'K');
	color_t* c=get_color(img,'c');
	color_t* m=get_color(img,'m');
	color_t* y=get_color(img,'y');
	color_t* k=get_color(img,'k');
	/* color_t* H=get_color(img,'H'); */
	/*color_t* R=get_color(img,'R');*/
	/*color_t* G=get_color(img,'G');*/
	/* experimenting with strange colors */
	/* color_t* P=get_color2(img,'P');
	color_t* Q=get_color2(img,'Q');
	color_t* R=get_color2(img,'R');
	color_t* S=get_color2(img,'S');
	color_t* T=get_color2(img,'T'); */

	/* color_t* A=get_color(img,'A'); */
	/* color_t* B=get_color(img,'B'); */
	/* color_t* D=get_color(img,'D'); */
	/* color_t* E=get_color(img,'E'); */
	/* color_t* F=get_color(img,'F'); */
	/* color_t* I=get_color(img,'I'); */
	/* color_t* J=get_color(img,'J'); */
	/* color_t* L=get_color(img,'L'); */
	/* color_t* N=get_color(img,'N'); */
	/* color_t* O=get_color(img,'O'); */
	/* color_t* P=get_color(img,'P'); */
	/* color_t* Q=get_color(img,'Q'); */
	/* color_t* S=get_color(img,'S'); */
	/* color_t* T=get_color(img,'T'); */
	/* color_t* U=get_color(img,'U'); */
	/* color_t* V=get_color(img,'V'); */
	/* color_t* W=get_color(img,'W'); */
	/* color_t* X=get_color(img,'X'); */
	/* color_t* Z=get_color(img,'Z'); */
	/* color_t* a=get_color(img,'a'); */
	/* color_t* b=get_color(img,'b'); */
	/* color_t* d=get_color(img,'d'); */
	/* color_t* e=get_color(img,'e'); */
	/* color_t* f=get_color(img,'f'); */
	GetBitContext gb[MAX_COLORS];
	/* move iterator */
	advance(img,pos_y);
	/* init get bits */
	for(i=0;i<MAX_COLORS;i++){
		if(inside_range(&(img->color[i]),0,pos_y)){
			init_get_bits(&(gb[i]),img->color[i].pos->buf,img->color[i].pos->len);
		}
	}
	for(x=0;x<img->width;x++){
		int lK=0,lM=0,lY=0,lC=0;
		/* initialize so can add same colors together later  */
		for(i=0;i<MAX_COLORS;i++){
		  img->color[i].value=0;
		  }
		for(i=0;i<MAX_COLORS;i++){
		  if(inside_range(&img->color[i],x,pos_y)) {
		    /*img->color[i].value = get_bits(&gb[i],img->color[i].bpp);*/
		    img->color[i].value += get_bits(&gb[i],img->color[i].bpp);
		    /*		    printf("getting pixel values for color %d\n",i);*/
		    /*if (img->color[i].value != 0)
		      printf("what pixel values for color %d: %x\n",i,img->color[i].value);*/
		    if (i>7){
		      fprintf(fout,"getting pixel values for color %d\n",i);/* only going 0 1 2 4 5 --- missing i>7 bugger! */
		      /*img->color[i].value = 1;*/
		      fprintf(fout,"color %c has value %d\n",img->color[i].name,img->color[i].value);
		    }
		    /* add 0x80 to colors where 0x80 is seen added in inkset */
		  }
		  else if(i>7) {
		    /* can we force some results here? */
		    /*img->color[i].value = 1;*/
		    /*get_bits(&gb[i],img->color[i].bpp);*/
		  }
		  else {
		    /*printf(" NOT getting pixel values for color %d\n",i);*/
		    img->color[i].value = 0;
		  }
		  /* update statistics */
		  (img->color[i].dots)[img->color[i].value] += 1;
		  /* set to 1 if the level is used */
		  (img->color[i].usedlevels)[img->color[i].value]=1;
		}
		/* calculate CMYK values */
		/*
		lK=K->density * K->value/(K->level-1) + k->density * k->value/(k->level-1);
		lM=M->density * M->value/(M->level-1) + m->density * m->value/(m->level-1);
		lY=Y->density * Y->value/(Y->level-1) + y->density * y->value/(y->level-1);
		lC=C->density * C->value/(C->level-1) + c->density * c->value/(c->level-1);*/

		lK=K->density * K->value/(K->level-1) + k->density * k->value/(k->level-1);
		lM=M->density * M->value/(M->level-1) + m->density * m->value/(m->level-1);
		lY=Y->density * Y->value/(Y->level-1) + y->density * y->value/(y->level-1);
		lC=C->density * C->value/(C->level-1) + c->density * c->value/(c->level-1);


		/* detect image edges */
		if(lK || lM || lY || lC){
			if(!img->image_top)
				img->image_top = pos_y;
			img->image_bottom = pos_y;
			if(x < img->image_left)
				img->image_left = x;
			if(x > img->image_right)
				img->image_right = x;
		}

                /* clip values */
                if(lK > 255)
                   lK = 255;
                if(lM > 255)
                   lM = 255;
                if(lC > 255)
                   lC = 255;
                if(lY > 255)
                   lY = 255;
		/* convert to RGB */
		/* 0 == black, 255 == white */
		line[x*3]=255 - lC - lK;
		line[x*3+1]=255 - lM -lK;
		line[x*3+2]=255 - lY -lK;
		++img->dots;
	}

	/* output line */
	if((written = fwrite(line,img->width,3,fp)) != 3) {
		fprintf(fout,"fwrite failed %u vs %u\n",written,img->width*3);
	}
	free(line);
}


/* create a ppm image from the decoded raster data */
static void write_ppm(image_t* img,FILE* fp){
	int i;
	/* allocate buffers for dot statistics */
        for(i=0;i<MAX_COLORS;i++){
	  /*img->color[i].dots=calloc(1,sizeof(int)*(img->color[i].level+1));*/
	  img->color[i].dots=(unsigned int *)calloc(1,sizeof(int)*(1<<((img->color[i].bpp)+1)));
	}
	/* allocate buffers for levels used*/
        for(i=0;i<MAX_COLORS;i++){
	  img->color[i].usedlevels=(unsigned int *)calloc(1,sizeof(int)*(1<<((img->color[i].bpp)+1)));
	}

	/* write header */
	fputs("P6\n", fp);
	fprintf(fp, "%d\n%d\n255\n", img->width, img->height);

	/* set top most left value */
	img->image_left = img->width;

	/* write data line by line */
	for(i=0;i<img->height;i++){
		write_line(img,fp,i);
	}

	/* output some statistics */
	printf("statistics:\n");
	for(i=0;i<MAX_COLORS;i++){
	  int level;
	  if (img->color[i].bpp > 0) {
	    /*for(level=0;level < img->color[i].level;level++)*/
	    for(level=0;level < 1<<(img->color[i].bpp);level++)
	      printf("color %c level %i dots %i\n",img->color[i].name,level,img->color[i].dots[level]);
	  }
	}
	printf("Level values actually used:\n");
	for(i=0;i<MAX_COLORS;i++){
	  int level;
	  if (img->color[i].bpp > 0) {
	    printf("color %c bpp %i available levels %i declared levels %i --- actual level values used:\n",img->color[i].name,img->color[i].bpp,1<<(img->color[i].bpp),img->color[i].level);
	    for(level=0;level < 1<<(img->color[i].bpp);level++)
	      printf("%i",img->color[i].usedlevels[level]);
	    printf("\n");
	  }
	}
	/* translate area coordinates to 1/72 in (the gutenprint unit)*/
	img->image_top = img->image_top * 72.0 / img->yres ;
	img->image_bottom = img->image_bottom * 72.0 / img->yres ;
	img->image_left = img->image_left * 72.0 / img->xres ;
	img->image_right = img->image_right * 72.0 / img->xres ;
	printf("top %u bottom %u left %u right %u\n",img->image_top,img->image_bottom,img->image_left,img->image_right);
	printf("width %u height %u\n",img->image_right - img->image_left,img->image_bottom - img->image_top);

	/* clean up */
        for(i=0;i<MAX_COLORS;i++){
		if(img->color[i].dots)
			free(img->color[i].dots);
	}

}

static unsigned int read_uint32(unsigned char* a){
        unsigned int value = ( a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
        return value;
}


/* process a printjob command by command */
int process(FILE* in, FILE* out,int verbose,unsigned int maxw,unsigned int maxh,unsigned int startxmllen,unsigned int endxmllen){
	image_t* img=(image_t*)calloc(1,sizeof(image_t));
	unsigned char* buf=(unsigned char*)malloc(0xFFFF);
	int returnv=0;
	int i;
	int num_colors;
	unsigned int xml_read;
	xml_read=0;
	fprintf(fout,"------- parsing the printjob -------\n");
	while(!returnv && !feof(in)){
		unsigned char cmd;
		unsigned int cnt = 0;
		if((returnv = nextcmd(in,&cmd,buf,&cnt,&xml_read,startxmllen,endxmllen)))
			break;
		switch(cmd){
			case 'c':
				fprintf(fout,"ESC (c set media (len=%i):\n",cnt);
				fprintf(fout," model id %x bw %x",buf[0]>> 4,buf[0]&0xf);
				fprintf(fout," media %x",buf[1]);
				fprintf(fout," direction %x quality %x\n",(buf[2]>>4)&3 ,buf[2]&0xf);
				break;
			case 'K':
				if(buf[1]==0x1f){
				  fprintf(fout,"ESC [K go to command mode\n");
					do{
						fgets((char*)buf,0xFFFF,in);
						fprintf(fout," %s",buf);
					}while(strcmp((char*)buf,"BJLEND\n"));
				}else if(cnt == 2 && buf[1]==0x0f){
					fprintf(fout,"ESC [K reset printer\n");
					img->width=0;
					img->height=0;
				}else{
					fprintf(fout,"ESC [K unsupported param (len=%i): %x\n",cnt,buf[1]);
				}
				break;
			case 'b':
				fprintf(fout,"ESC (b set data compression (len=%i): %x\n",cnt,buf[0]);
				break;
			case 'I':
				fprintf(fout,"ESC (I select data transmission (len=%i): ",cnt);
				if(buf[0]==0)fprintf(fout,"default");
				else if(buf[0]==1)fprintf(fout,"multi raster");
				else fprintf(fout,"unknown 0x%x %i",buf[0],buf[0]);
				fprintf(fout,"\n");
				break;
			case 'l':
				fprintf(fout,"ESC (l select paper loading (len=%i):\n",cnt);
				fprintf(fout," model id 0x%x ",buf[0]>>4);
				fprintf(fout," source 0x%x",buf[0]&15);
				if ( cnt == 3 ) {
				  fprintf(fout," media: %x",buf[1]);
				  fprintf(fout," paper gap: %x\n",buf[2]);
				} else
				  fprintf(fout," media: %x\n",buf[1]);
				break;
			case 'd':
				img->xres = (buf[0]<<8)|buf[1];
				img->yres = (buf[2]<<8)|buf[3];
				fprintf(fout,"ESC (d set raster resolution (len=%i): %i x %i\n",cnt,img->xres,img->yres);
				break;
			case 't':
			  fprintf(fout,"ESC (t set image cnt %i\n",cnt);
				if(buf[0]>>7){
				        /* usual order */
				        char order[]="CMYKcmyk";
				        /*char order[]="CMYKcmykHRGABDEFIJLMNOPQSTUVWXZabdef";*/
				        /* iP3500 test */
				        /*char order[]="CMYKcmykHRGBCMYcmykabd";*/
				        /*char order[]="CMYKcmykHpnoPQRSTykabd";*/
				        /*char order[]="KCMYkcmyHpnoPQRSTykabd";*/
				        /* MP960 photo modes: k instead of K */
					/* char order[]="CMYkcmyKHRGABDEFIJLMNOPQSTUVWXZabdef";*/
					/* T-shirt transfer mode: y changed to k --- no y, no K */
					/*char order[]="CMYKcmkyHRGABDEFIJLMNOPQSTUVWXZabdef";*/
					/* MP990, MG6100, MG8100 plain modes */
				        /*char order[]="KCcMmYykRHGABDEFIJLMNOPQSTUVWXZabdef";*/
					/* MP990 etc. photo modes */
				        /* char order[]="KCcMmYykRHGABDEFIJLMNOPQSTUVWXZabdef"; */
					/* int black_found = 0; */
					num_colors = (cnt - 3)/3;
					fprintf(fout," bit_info: using detailed color settings for max %i colors\n",num_colors);
					if(buf[1]==0x80)
					  fprintf(fout," format: BJ indexed color image format\n");
                                        else if(buf[1]==0x00)
						fprintf(fout," format: iP8500 flag set, BJ indexed color image format\n");
                                        else if(buf[1]==0x90)
						fprintf(fout," format: Pro9500 flag set, BJ indexed color image format\n");
					else{
						fprintf(fout," format: settings not supported 0x%x\n",buf[1]);
						/* returnv = -2; */
					}
					if(buf[2]==0x1)
					        fprintf(fout," ink: BJ indexed setting, also for iP8500 flag\n");
					else if(buf[2]==0x4)
					        fprintf(fout," ink: Pro series setting \n");
					else{
						fprintf(fout," ink: settings not supported 0x%x\n",buf[2]);
						/* returnv = -2; */
					}

					for(i=0;i<num_colors;i++){
					  if(i<MAX_COLORS){
					    img->color[i].name=order[i];
					    img->color[i].compression=buf[3+i*3] >> 5;
					    img->color[i].bpp=buf[3+i*3] & 31;
					    img->color[i].level=(buf[3+i*3+1] << 8) + buf[3+i*3+2];/* check this carefully */

					    /* work around for levels not matching (bpp gives more) */
					    /*if ((img->color[i].level == 3) && (img->color[i].bpp == 2)) {
					      printf("WARNING: color %c bpp %i declared levels %i, setting to 4 for testing \n",img->color[i].name,img->color[i].bpp,img->color[i].level);
					      img->color[i].level = 4;
					      } */
					    /*else if ((img->color[i].level == 4) && (img->color[i].bpp == 4)) {*/
					    /* levels is 16 but only each 2nd level is used */
					    /*  printf("WARNING: color %c bpp %i declared levels %i, setting to 16 for testing \n",img->color[i].name,img->color[i].bpp,img->color[i].level);
						img->color[i].level = 16;
						} */

					    /* this is not supposed to give accurate images */
					    /* if(i<4) */ /* set to actual colors CMYK */
					    if((img->color[i].name =='K')||(img->color[i].name =='C')||(img->color[i].name =='M')||(img->color[i].name =='Y') ) {
					      img->color[i].density = 255;
					      /*if (i>7)*/
						/*img->color[i].density -= 128; */
					      /* see if can subtract something from CMYK where 0x80 involved */
					    }
					    else
					      img->color[i].density = 128; /*128+96;*/ /* try to add 0x80 to sub-channels for MP450 hi-quality mode */
                                            /*
					    if((order[i] == 'K' || order[i] == 'k') && img->color[i].bpp)
					      black_found = 1;
					    */
					    /*
					    if(order[i] == 'y' && !black_found && img->color[i].level){
					      printf("iP6700 hack: treating color definition at the y position as k\n");
					      img->color[i].name = 'k';
					      order[i] = 'k';
					      order[i+1] = 'y';
					      black_found = 1;
					      img->color[i].density = 255;
					    }
					    */
					    /* %c*/
					    fprintf(fout," Color %c Compression: %i bpp %i level %i\n",img->color[i].name,
						   img->color[i].compression,img->color[i].bpp,img->color[i].level);
					  }else{
					    fprintf(fout," Color %i out of bounds!\n", i);
					    /*printf(" Color ignoring setting %x %x %x\n",buf[3+i*3],buf[3+i*3+1],buf[3+i*3+2]);*/
					  }

					}


				}else if(buf[0]==0x1 && buf[1]==0x0 && buf[2]==0x1){
					fprintf(fout," 1bit-per pixel\n");
					num_colors = cnt*3; /*no idea yet! 3 for iP4000 */
					/*num_colors=9;*/
					/*for(i=0;i<MAX_COLORS;i++){*/
					for(i=0;i<num_colors;i++){
					  if(i<MAX_COLORS){
					        /* usual */
  					        char order[]="CMYKcmyk";
					        /* const char order[]="CMYKcmykHRGABDEFIJLMNOPQSTUVWXZabdef";*/
				                /* iP3500 test */
				                /* char order[]="CMYKcmykHRGBCMYcmykabd"; */
						/* MP990, MG6100, MG8100 plain modes */
						/*const char order[]="KCcMmYykRHGABDEFIJLMNOPQSTUVWXZabdef";*/
						img->color[i].name=order[i];
						img->color[i].compression=0;
						img->color[i].bpp=1;
						img->color[i].level=2;
						img->color[i].density = 255;
						/*add color printout for this type also %c */
						fprintf(fout," Color %c Compression: %i bpp %i level %i\n",img->color[i].name,
						       img->color[i].compression,img->color[i].bpp,img->color[i].level);
					  }else{
					    fprintf(fout," Color %i out of bounds!", i);
						/*printf(" Color ignoring setting %x %x %x\n",buf[3+i*3],buf[3+i*3+1],buf[3+i*3+2]);*/
					  }
					}
				}else{
					fprintf(fout," bit_info: unknown settings 0x%x 0x%x 0x%x\n",buf[0],buf[1],buf[2]);
					/* returnv=-2; */
				}
				break;
			case 'L':
				fprintf(fout,"ESC (L set component order for F raster command (len=%i): ",cnt);
				img->color_order=(char *)calloc(1,cnt+1);
				/* check if the colors are sane => the iP4000 driver appends invalid bytes in the highest resolution mode */
				for(i=0;i<cnt;i++){
				  if (!valid_color(buf[i]))
                                    {
				    /*if (!(valid_color(buf[i]-0x60))) {*/
				    /*  printf("invalid color char %c [failed on initial]\n", buf[i]);*/
				    /*  break; */
				    /*}*/
				    /*else {*/
				      buf[i]=buf[i]-0x60;
				      fprintf(fout,"subtracting 0x60 to give [corrected]: %c\n", buf[i]);
				    }
				  else
				    fprintf(fout,"found valid color char: %c\n",buf[i]);
				}
				cnt = i;
				memcpy(img->color_order,buf,cnt);
				fprintf(fout,"%s\n",img->color_order);
				img->num_colors = cnt;
				img->cur_color=0;
				break;
			case 'p':
				fprintf(fout,"ESC (p set extended margin (len=%i):\n",cnt);
                                fprintf(fout," printed length %i left %i\n",((buf[0]<<8 )+buf[1]) *6 / 5 - 1,(buf[2]<<8) + buf[3]);
                                fprintf(fout," printed width %i top %i\n",((buf[4]<<8 )+buf[5]) * 6 / 5 - 1,(buf[6]<<8) + buf[7]);

                                if(cnt > 8){
					int unit = (buf[12] << 8)| buf[13];
					int area_right = read_uint32(buf+14);
					int area_top = read_uint32(buf+18);
					unsigned int area_width = read_uint32(buf+22);
					unsigned int area_length = read_uint32(buf+26);
					int paper_right = read_uint32(buf+30);
					int paper_top = read_uint32(buf+34);
					unsigned int paper_width = read_uint32(buf+38);
					unsigned int paper_length = read_uint32(buf+42);
                                        fprintf(fout," unknown %i\n",read_uint32(buf+8));
                                        fprintf(fout," unit %i [1/in]\n",unit);
                                        fprintf(fout," area_right %i %.1f mm\n",area_right,area_right * 25.4 / unit);
                                        fprintf(fout," area_top %i %.1f mm\n",area_top,area_top * 25.4 / unit);
                                        fprintf(fout," area_width %u %.1f mm\n",area_width, area_width * 25.4 / unit);
                                        fprintf(fout," area_length %u %.1f mm\n",area_length,area_length * 25.4 / unit);
                                        fprintf(fout," paper_right %i %.1f mm\n",paper_right,paper_right * 25.4 / unit);
                                        fprintf(fout," paper_top %i %.1f mm\n",paper_top,paper_top * 25.4 / unit);
                                        fprintf(fout," paper_width %u %.1f mm\n",paper_width,paper_width * 25.4 / unit);
                                        fprintf(fout," paper_length %u %.1f mm\n",paper_length,paper_length * 25.4 / unit);
					img->top = (float)area_top / unit;
					img->left = (float)area_top / unit;
                                }
				break;
			case '$':
				fprintf(fout,"ESC ($ set duplex (len=%i)\n",cnt);
				break;
			case 'J':
				fprintf(fout,"ESC (J select number of raster lines per block (len=%i): %i\n",cnt,buf[0]);
				img->lines_per_block=buf[0];
				break;
			case 'F':
				if(verbose)
					fprintf(fout,"ESC (F raster block (len=%i):\n",cnt);
				if((returnv = Raster(img,buf,cnt,img->color_order[img->cur_color],maxw)))
					break;
				++img->cur_color;
				if(img->cur_color >= img->num_colors){
					img->cur_color=0;
					img->height+=img->lines_per_block;
				}
				break;
			case 'q':
				fprintf(fout,"ESC (q set page id (len=%i):%i\n",cnt,buf[0]);
				break;
			case 'r':
				fprintf(fout,"ESC (r printer specific command (len=%i): ",cnt);
				for(i=0;i<cnt;i++)
					fprintf(fout,"0x%x ",buf[i]);
				fprintf(fout,"\n");
				break;
			case 'A':
				if(verbose)
					fprintf(fout,"ESC (A raster line (len=%i): color %c\n",cnt,buf[0]);
				/* the single line rasters are not terminated by 0x80 => do it here
				 * instead of 0x80 every raster A command is followed by a 0x0d byte
				 * the selected color is stored in the first byte
				 */
				buf[cnt]=0x80;
				returnv = Raster(img,buf+1,cnt,buf[0],maxw);
				if (fgetc(in)!=0x0d){
					fprintf(fout,"Raster A not terminated by 0x0d\n");
					returnv=-4;
				}
				break;
			case 'e': /* Raster skip */
				if(verbose)
					fprintf(fout,"ESC (e advance (len=%i): %i\n",cnt,buf[0]*256+buf[1]);
				if(img->lines_per_block){
					img->height += (buf[0]*256+buf[1])*img->lines_per_block;
					img->cur_color=0;
				}else
					img->height += (buf[0]*256+buf[1]);
				break;
			default: /* Last but not least completely unknown commands */
				fprintf(fout,"ESC (%c UNKNOWN (len=%i)\n",cmd,cnt);
				for(i=0;i<cnt;i++)
					fprintf(fout," 0x%x",buf[i]);
				fprintf(fout,"\n");

		}
	}

	fprintf(fout,"-------- finished parsing   --------\n");
	if(returnv < -2){ /* was < 0 :  work around to see what we get */
		fprintf(fout,"error: parsing the printjob failed error %i\n",returnv);
	} else {

		fprintf(fout,"created bit image with width %i height %i\n",img->width,img->height);
		if(maxh > 0){
			fprintf(fout,"limiting height to %u\n",maxh);
			img->height=maxh;
		}

		/* now that we have a complete nice raster image
	 	* lets build a bitmap from it
         	*/
		if(out)
			write_ppm(img,out);
		fprintf(fout,"dots: %u\n",img->dots);

	}
	/* deallocate resources */
	free(buf);
	for(i=0;i<MAX_COLORS;i++){
		rasterline_t** r=&img->color[i].tail;
		while(*r){
			rasterline_t* tmp=(*r)->next;
			free((*r)->buf);
			free(*r);
			*r=tmp;
		}
	}
	if(img->color_order)
		free(img->color_order);
	free(img);

	return returnv;
}



static void display_usage(void){
	printf("usage: pixma_parse [options] infile [outfile]\n");
	printf("infile: the printjob to parse\n");
	printf("outfile: if specified a ppm file will be generated from the raster data\n");
	printf("options:\n");
	printf(" -v: verbose print ESC e),F) and A) commands\n");
	printf(" -x width: cut the output ppm to the given width\n");
	printf(" -y height: cut the output ppm to the given height\n");
	printf(" -s if XML prolog is present, define number of bytes (default 680) \n");
	printf(" -e if XML epilog is present, define number of bytes (default 263) \n");
	printf(" -h: display this help\n");
}

int process_bjl(char* filename_in, char* filename_out) {
	FILE *in,*out=NULL;

	/* open input file */
	if(!(in=fopen(filename_in,"rb"))){
		Console.WriteLn("pixma_parse: unable to open input file %s", filename_in);
	  printf("unable to open input file %s\n",filename_in);
		return 1;
	}

	/* open output file */
	if(filename_out && !(out=fopen(filename_out,"wb"))){
	  printf("can't create the output file %s\n",filename_out);
		fclose(in);
		return 1;
	}

	/* process the printjob */
	process(in,out,0,0,0,0,0);

	/* cleanup */
	fclose(in);
	if(out)
		fclose(out);

	Console.WriteLn("pixma_parse: ppm file created %s", filename_out);
}

int old_main(int argc,char* argv[]){
	int verbose = 0;
	unsigned int maxh=0;
	unsigned int maxw=0;
	unsigned int startxmllen, endxmllen;

	char* filename_in=NULL,*filename_out=NULL;
	FILE *in,*out=NULL;
	int i;

	startxmllen=680; // 1086; 732; 680; embedded parameters varies with driver -- TODO: parse XML separately
	endxmllen=263; // default value

	if (DEBUG)
	  fout = stderr; /* unbuffered */
	else
	  fout = stdout; /* buffered */

	printf("pixma_parse - parser for Canon BJL printjobs (c) 2005-2007 Sascha Sommer <saschasommer@freenet.de>\n");

	/* parse args */
	for(i=1;i<argc;i++){
		if(strlen(argv[i]) >= 2 && argv[i][0] == '-'){
			if(argv[i][1] == 'v'){
				verbose = 1;
			}else if(argv[i][1] == 'h'){
				display_usage();
				return 0;
			}else if(argv[i][1] == 'y'){
				if(argc > i+1){
					++i;
					maxh = atoi(argv[i]);
				}else{
					display_usage();
					return 1;
				}
			}else if(argv[i][1] == 'x'){
				if(argc > i+1){
					++i;
					maxw = atoi(argv[i]);
				}else{
					display_usage();
					return 1;
				}
			}else if(argv[i][1] == 's'){
				if(argc > i+1){
					++i;
					startxmllen = atoi(argv[i]);
				}else{
					display_usage();
					return 1;
				}
			}else if(argv[i][1] == 'e'){
				if(argc > i+1){
					++i;
					endxmllen = atoi(argv[i]);
				}else{
					display_usage();
					return 1;
				}
			}else {
			  printf("unknown parameter %s\n",argv[i]);
				return 1;
			}
		}else if(!filename_in){
			filename_in = argv[i];
		}else if(!filename_out){
			filename_out = argv[i];
		}else{
			display_usage();
			return 1;
		}
	}
	if(!filename_in){
		display_usage();
		return 1;
	}

	/* open input file */
	if(!(in=fopen(filename_in,"rb"))){
	  printf("unable to open input file %s\n",filename_in);
		return 1;
	}

	/* open output file */
	if(filename_out && !(out=fopen(filename_out,"wb"))){
	  printf("can't create the output file %s\n",filename_out);
		fclose(in);
		return 1;
	}

	/* process the printjob */
	process(in,out,verbose,maxw,maxh,startxmllen,endxmllen);

	/* cleanup */
	fclose(in);
	if(out)
		fclose(out);
	return 0;
}
