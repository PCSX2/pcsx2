#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	JO_RGBX,
	JO_RGB24,
	JO_BGR24,
	JO_YUYV,
} jo_mpeg_format_t;

typedef enum {
	JO_NONE,
	JO_FLIP_X,
	JO_FLIP_Y,
} jo_mpeg_flip_t;

unsigned long jo_write_mpeg(unsigned char *mpeg_buf, const unsigned char *rgbx, int width, int height, int format, int flipx, int flipy);

#ifdef __cplusplus
}
#endif
