/*
 * File:    pngread.h
 * Purpose:    PNG file reader
 * Author:    Alejandro Aguilar Sierra/Julian Smart
 * Created:    1995
 * Copyright:    (c) 1995, Alejandro Aguilar Sierra <asierra@servidor.unam.mx>
 *
 *
 */

#ifndef _WX_PNGREAD__
#define _WX_PNGREAD__

#ifndef byte
typedef unsigned char byte;
#endif

#define WXIMA_COLORS DIB_PAL_COLORS

typedef byte * ImagePointerType;

typedef struct
{
    byte red;
    byte green;
   byte blue;
} rgb_color_struct;


#define COLORTYPE_PALETTE    1
#define COLORTYPE_COLOR        2
#define COLORTYPE_ALPHA        4

class wxPNGReader
{
protected:
  int filetype;
  char filename[255];
  ImagePointerType RawImage;      //  Image data

  int Width, Height;               //  Dimensions
  int Depth;             // (bits x pixel)
  int ColorType;        // Bit 1 = Palette used
                // Bit 2 = Color used
                // Bit 3 = Alpha used

  long EfeWidth;         // Efective Width

  void *lpbi;
  int bgindex;
  wxPalette* m_palette;
  bool imageOK;
friend class wxPNGReaderIter;
public:
  wxPNGReader(void);
  wxPNGReader (char* ImageFileName);     // Read an image file
  virtual ~wxPNGReader ();

  void Create(int width, int height, int deep, int colortype=-1);

  bool ReadFile( char* ImageFileName=0 );
  bool SaveFile( char* ImageFileName=0 );
  bool SaveXPM(char *filename, char *name = 0);
  int  GetWidth( void ) const { return Width; }
  int  GetHeight( void ) const { return Height; }
  int  GetDepth( void ) const { return Depth; }
  int  GetColorType( void ) const { return ColorType; }

  int  GetIndex(int x, int y);
  bool GetRGB(int x, int y, byte* r, byte* g, byte* b);

  bool SetIndex(int x, int y, int index);
  bool SetRGB(int x, int y, byte r, byte g, byte b);

  // ColorMap settings
  bool SetPalette(wxPalette* colourmap);
  bool SetPalette(int n, rgb_color_struct *rgb_struct);
  bool SetPalette(int n, byte *r, byte *g=0, byte *b=0);
  wxPalette* GetPalette() const { return m_palette; }

  void NullData();
  inline int GetBGIndex(void) { return bgindex; }

  inline bool Inside(int x, int y)
      { return (0<=y && y<Height && 0<=x && x<Width); }

  virtual wxBitmap *GetBitmap(void);
  virtual bool InstantiateBitmap(wxBitmap *bitmap);
  wxMask *CreateMask(void);

  inline bool Ok() const { return IsOk(); }
  inline bool IsOk(void) { return imageOK; }
};

class wxPNGReaderIter
{
protected:
  int Itx, Ity;                // Counters
  int Stepx, Stepy;
  ImagePointerType IterImage;          //  Image pointer
  wxPNGReader *ima;
public:
// Constructors
  wxPNGReaderIter ( void );
  wxPNGReaderIter ( wxPNGReader *imax );
  operator wxPNGReader* ();

// Iterators
  bool ItOK ();
  void reset ();
  void upset ();
  void SetRow(byte *buf, int n);
  void GetRow(byte *buf, int n);
  byte GetByte( ) { return IterImage[Itx]; }
  void SetByte(byte b) { IterImage[Itx] = b; }
  ImagePointerType GetRow(void);
  bool NextRow();
  bool PrevRow();
  bool NextByte();
  bool PrevByte();

  void SetSteps(int x, int y=0) {  Stepx = x; Stepy = y; }
  void GetSteps(int *x, int *y) {  *x = Stepx; *y = Stepy; }
  bool NextStep();
  bool PrevStep();

////////////////////////// AD - for interlace ///////////////////////////////
  void SetY(int y);
/////////////////////////////////////////////////////////////////////////////
};


inline
wxPNGReaderIter::wxPNGReaderIter(void)
{
  ima = 0;
  IterImage = 0;
  Itx = Ity = 0;
  Stepx = Stepy = 0;
}

inline
wxPNGReaderIter::wxPNGReaderIter(wxPNGReader *imax): ima(imax)
{
  if (ima)
     IterImage = ima->RawImage;
  Itx = Ity = 0;
  Stepx = Stepy = 0;
}

inline
wxPNGReaderIter::operator wxPNGReader* ()
{
  return ima;
}

inline
bool wxPNGReaderIter::ItOK ()
{
  if (ima)
     return ima->Inside(Itx, Ity);
  else
     return FALSE;
}


inline void wxPNGReaderIter::reset()
{
  IterImage = ima->RawImage;
  Itx = Ity = 0;
}

inline void wxPNGReaderIter::upset()
{
  Itx = 0;
  Ity = ima->Height-1;
  IterImage = ima->RawImage + ima->EfeWidth*(ima->Height-1);
}

inline bool wxPNGReaderIter::NextRow()
{
  if (++Ity >= ima->Height) return 0;
  IterImage += ima->EfeWidth;
  return 1;
}

inline bool wxPNGReaderIter::PrevRow()
{
  if (--Ity < 0) return 0;
  IterImage -= ima->EfeWidth;
  return 1;
}

////////////////////////// AD - for interlace ///////////////////////////////
inline void wxPNGReaderIter::SetY(int y)
{
  if ((y < 0) || (y > ima->Height)) return;
  Ity = y;
  IterImage = ima->RawImage + ima->EfeWidth*y;
}

/////////////////////////////////////////////////////////////////////////////

inline void wxPNGReaderIter::SetRow(byte *buf, int n)
{
// Here should be bcopy or memcpy
  //_fmemcpy(IterImage, (void far *)buf, n);
  if (n<0)
     n = ima->GetWidth();

  for (int i=0; i<n; i++) IterImage[i] = buf[i];
}

inline void wxPNGReaderIter::GetRow(byte *buf, int n)
{
  for (int i=0; i<n; i++) buf[i] = IterImage[i];
}

inline ImagePointerType wxPNGReaderIter::GetRow()
{
  return IterImage;
}

inline bool wxPNGReaderIter::NextByte()
{
  if (++Itx < ima->EfeWidth)
     return 1;
  else
     if (++Ity < ima->Height)
     {
        IterImage += ima->EfeWidth;
        Itx = 0;
        return 1;
     } else
        return 0;
}

inline bool wxPNGReaderIter::PrevByte()
{
  if (--Itx >= 0)
     return 1;
  else
     if (--Ity >= 0)
     {
        IterImage -= ima->EfeWidth;
        Itx = 0;
        return 1;
     } else
        return 0;
}

inline bool wxPNGReaderIter::NextStep()
{
  Itx += Stepx;
  if (Itx < ima->EfeWidth)
     return 1;
  else {
     Ity += Stepy;
     if (Ity < ima->Height)
     {
        IterImage += ima->EfeWidth;
        Itx = 0;
        return 1;
     } else
        return 0;
  }
}

inline bool wxPNGReaderIter::PrevStep()
{
  Itx -= Stepx;
  if (Itx >= 0)
     return 1;
  else {
     Ity -= Stepy;
     if (Ity >= 0 && Ity < ima->Height)
     {
        IterImage -= ima->EfeWidth;
        Itx = 0;
        return 1;
     } else
        return 0;
  }
}

#endif

