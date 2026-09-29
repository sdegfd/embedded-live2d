/** @file PX_Typedef.c
 *  @brief PainterEngine 基础类型与工具函数实现
 *  包含字符串转换、数学运算（sqrt、三角函数、对数、指数）、
 *  矩阵运算、颜色空间转换、FFT/DCT信号处理、随机数生成、
 *  内存操作、文件路径解析等核心工具函数的实现。
 */

#include "PX_Typedef.h"

#include <string.h>
#include <stddef.h>

/* ── 字节序检测 ──────────────────────────────────────── */

static px_bool PX_isBigEndianCPU()
{
	union{
		px_dword i;
		px_uchar s[4];
	}c;
	c.i = 0x12345678;
	return (0x12 == c.s[0]);

}

static px_int32 PX_i32SwapEndian(px_int32 val){
	val = ((val << 8)&0xFF00FF00) | ((val >> 8)&0x00FF00FF);
	return (val << 16)|(val >> 16);
}

px_dword PX_SwapEndian(px_dword val)
{
	val = ((val << 8)&0xFF00FF00) | ((val >> 8)&0x00FF00FF);
	return (val << 16)|(val >> 16);
}

static px_int64 PX_i64SwapEndian(px_int64 val)
{
	px_int32 u32_host_h, u32_host_l;
	px_int64 u64_net;
	u32_host_l = val & 0xffffffff;
	u32_host_h = (val >> 32) & 0xffffffff;

	u64_net = PX_i32SwapEndian(u32_host_l);
	u64_net = ( u64_net << 32 ) | PX_i32SwapEndian(u32_host_h);
	return u64_net;
}

/* ── 平方根运算（快速牛顿迭代法） ──────────────────────── */
px_float PX_sqrt( px_float number )  
{  
	px_int32 i;  
	px_float x2, y;  
	const px_float threehalfs = 1.5F;  
	x2 = number * 0.5F;  
	y  = number;  
	i  = * ( px_int32 * ) &y;

	if (PX_isBigEndianCPU())
	{
		i=PX_i32SwapEndian(i);
	}
	
	i  = 0x5f375a86 - ( i >> 1 );
	y  = * ( px_float * ) &i;  
	y  = y * ( threehalfs - ( x2 * y * y ) );   
	y  = y * ( threehalfs - ( x2 * y * y ) );     
	y  = y * ( threehalfs - ( x2 * y * y ) );
	y  = y * ( threehalfs - ( x2 * y * y ) );   
	y  = y * ( threehalfs - ( x2 * y * y ) );     
	y  = y * ( threehalfs - ( x2 * y * y ) );      
	return number*y;  
} 

/* ── 快速指数运算 ──────────────────────────────────── */
//0x5fe6ec85e7de30da

px_double PX_sqrtd( px_double number )  
{  
	px_int64 i;  
/* 双精度浮点数平方根（使用牛顿迭代法逼近） */
	px_double x2, y;  
	const px_double threehalfs = 1.5;  
	x2 = number * 0.5;  
	y  = number;  
	i  = * ( px_int64 * ) &y;

	if (PX_isBigEndianCPU())
	{
		i=PX_i64SwapEndian(i);
	}

	i  = 0x5fe6ec85e7de30da - ( i / 2 ); 
	y  = * ( px_double * ) &i;  
	y  = y * ( threehalfs - ( x2 * y * y ) );   
	y  = y * ( threehalfs - ( x2 * y * y ) );     
	y  = y * ( threehalfs - ( x2 * y * y ) );
	y  = y * ( threehalfs - ( x2 * y * y ) );   
	y  = y * ( threehalfs - ( x2 * y * y ) );     
	y  = y * ( threehalfs - ( x2 * y * y ) );   
	y  = y * ( threehalfs - ( x2 * y * y ) );   
	y  = y * ( threehalfs - ( x2 * y * y ) );        
	return number*y;  
} 

#if L2D_CFG_PROFILE_VISUAL
static struct {
	int active;
	int scope;
	int stack[8];
	int sp;
	PX_VisualDiagTrig trig;
	px_dword angles[1024];
	int angle_n;
	int angle_overflow;
} s_visual_diag;

static int PX_VisualDiagScope(void)
{
	int scope=s_visual_diag.scope;
	if (scope<0||scope>=PX_VISUAL_SCOPE_COUNT) return PX_VISUAL_SCOPE_OTHER;
	return scope;
}

static void PX_VisualDiagCountSin(void)
{
	if (s_visual_diag.active) s_visual_diag.trig.sin_angle[PX_VisualDiagScope()]++;
}

static void PX_VisualDiagCountCos(void)
{
	if (s_visual_diag.active) s_visual_diag.trig.cos_angle[PX_VisualDiagScope()]++;
}

static void PX_VisualDiagCountSind(void)
{
	if (s_visual_diag.active) s_visual_diag.trig.sind[PX_VisualDiagScope()]++;
}

static void PX_VisualDiagNotePointRotate(px_float angle)
{
	union { px_float f; px_dword u; } bits;
	if (!s_visual_diag.active) return;
	s_visual_diag.trig.point_rotate[PX_VisualDiagScope()]++;
	bits.f=angle;
	if (s_visual_diag.angle_n<(int)(sizeof(s_visual_diag.angles)/sizeof(s_visual_diag.angles[0])))
	{
		s_visual_diag.angles[s_visual_diag.angle_n++]=bits.u;
	}
	else
	{
		s_visual_diag.angle_overflow=1;
	}
}

static int PX_VisualDiagUniqueAngles(void)
{
	int i,j,unique=0;
	for (i=0;i<s_visual_diag.angle_n;i++)
	{
		for (j=0;j<i;j++)
		{
			if (s_visual_diag.angles[j]==s_visual_diag.angles[i]) break;
		}
		if (j==i) unique++;
	}
	return unique;
}

void PX_VisualDiagBegin(void)
{
	memset(&s_visual_diag,0,sizeof(s_visual_diag));
	s_visual_diag.active=1;
}

void PX_VisualDiagEnd(void)
{
	s_visual_diag.active=0;
}

void PX_VisualDiagPush(int scope)
{
	if (s_visual_diag.sp<(int)(sizeof(s_visual_diag.stack)/sizeof(s_visual_diag.stack[0])))
	{
		s_visual_diag.stack[s_visual_diag.sp++]=s_visual_diag.scope;
	}
	s_visual_diag.scope=scope;
}

void PX_VisualDiagPop(void)
{
	if (s_visual_diag.sp>0) s_visual_diag.scope=s_visual_diag.stack[--s_visual_diag.sp];
	else s_visual_diag.scope=PX_VISUAL_SCOPE_OTHER;
}

void PX_VisualDiagRead(PX_VisualDiagTrig *out)
{
	if (!out) return;
	*out=s_visual_diag.trig;
	out->unique_point_rotate_angles=(px_dword)PX_VisualDiagUniqueAngles();
	out->point_rotate_angle_samples=(px_dword)s_visual_diag.angle_n;
	out->point_rotate_angle_overflow=s_visual_diag.angle_overflow?1u:0u;
}
#endif

px_double PX_sind(px_double x)
{
	px_double it;
	px_double term;
	px_double result;
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagCountSind();
#endif

	it=x/(2*PX_PI);
	x=x-(2*PX_PI)*PX_TRUNC(it);

	term=x;
	result=0;

	result+=term;
	term*=(-x*x)/(2*3);
	result+=term;
	term*=(-x*x)/(4*5);
	result+=term;
	term*=(-x*x)/(6*7);
	result+=term;
	term*=(-x*x)/(8*9);
	result+=term;
	term*=(-x*x)/(10*11);
	result+=term;
	term*=(-x*x)/(12*13);
	result+=term;
	term*=(-x*x)/(14*15);
	result+=term;
	term*=(-x*x)/(16*17);
	result+=term;
	term*=(-x*x)/(18*19);
	result+=term;
	term*=(-x*x)/(20*21);
	result+=term;
	term*=(-x*x)/(22*23);
	result+=term;
	term*=(-x*x)/(24*25);
	result+=term;
	term*=(-x*x)/(26*27);
	result+=term;
	term*=(-x*x)/(28*29);
	result+=term;
	term*=(-x*x)/(30*31);
	result+=term;
	term*=(-x*x)/(32*33);
	return result;
}

px_double PX_sinc(px_double i)
{
	return i?PX_sind(i * PX_PI) / (i * PX_PI):1;
}

px_double PX_sinc_interpolate(px_double x[], px_int size, px_double d)
{
	px_int i;
	px_double sum = 0;
	for (i = 0; i < size; i++)
		sum += x[i] * PX_sinc(d - i);
	return sum;
}

px_double PX_cosd(px_double radian)
{
	return PX_sind((PX_PI/2-radian));
}

px_float PX_sin_radian(px_float radian)
{
	return (px_float)PX_sind(radian);
}


px_float PX_cos_radian(px_float radian)
{
	return PX_sin_radian((px_float)(PX_PI/2-radian));
}

px_float PX_tan_radian(px_float radian)
{
	return PX_sin_radian(radian)/PX_cos_radian(radian);
}

px_float PX_sin_angle(px_float angle)
{
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagCountSin();
#endif
	angle-=((px_int)angle/360)*360;
	return (px_float)PX_sin_radian((angle*0.0174532925f));
}
px_float PX_cos_angle(px_float angle)
{
#if L2D_CFG_PROFILE_VISUAL
	PX_VisualDiagCountCos();
#endif
	angle-=((px_int)angle/360)*360;
	return PX_cos_radian((angle*0.0174532925f));
}

/* 从ARGB分量构造颜色值 */

px_color PX_COLOR(px_uchar a,px_uchar r,px_uchar g,px_uchar b)
{
	px_color color;
	color._argb.a=a;
	color._argb.r=r;
	color._argb.g=g;
	color._argb.b=b;
	return color;
}

px_point PX_PointAdd(px_point p1,px_point p2)
{
	p1.x+=p2.x;
	p1.y+=p2.y;
	p1.z+=p2.z;
	return p1;
}

px_point2D PX_Point2DAdd(px_point2D p1,px_point2D p2)
{
	p1.x+=p2.x;
	p1.y+=p2.y;
	return p1;
}

px_point2Di PX_Point2DiAdd(px_point2Di p1, px_point2Di p2)
{
	p1.x += p2.x;
	p1.y += p2.y;
	return p1;
}


px_point PX_PointSub(px_point p1,px_point p2)
{
	p1.x-=p2.x;
	p1.y-=p2.y;
	p1.z-=p2.z;
	return p1;
}

px_point2D PX_Point2DSub(px_point2D p1,px_point2D p2)
{
	p1.x-=p2.x;
	p1.y-=p2.y;
	return p1;
}

px_point2Di PX_Point2DiSub(px_point2Di p1, px_point2Di p2)
{
	p1.x -= p2.x;
	p1.y -= p2.y;
	return p1;
}


px_point4D PX_Point4DSub(px_point4D p1,px_point4D p2)
{
	px_point4D v;
	v.x=p1.x-p2.x;
	v.y=p1.y-p2.y;
	v.z=p1.z-p2.z;
	v.w=1;
	return v;
}

px_point PX_PointMul(px_point p1,px_float m)
{
	p1.x*=m;
	p1.y*=m;
	p1.z*=m;
	return p1;
}

px_point2D PX_Point2DMul(px_point2D p1,px_float m)
{
	p1.x*=m;
	p1.y*=m;
	return p1;
}

px_point2Di PX_Point2DiMul(px_point2Di p1, px_float m)
{
	p1.x =(px_int)(p1.x* m);
	p1.y =(px_int)(p1.y* m);

	return p1;
}

px_point PX_PointDiv(px_point p1,px_float m)
{
	p1.x/=m;
	p1.y/=m;
	p1.z/=m;
	return p1;
}

px_point2D PX_Point2DRrthonormal(px_point2D v)
{
	return PX_Point2DNormalization(PX_POINT2D(v.y,-v.x));
}

px_point2D PX_Point2DBase(px_point2D base1,px_point2D base2,px_point2D target)
{
	base1=PX_Point2DNormalization(base1);
	base2=PX_Point2DNormalization(base2);

	return PX_POINT2D(
		(target.x*base2.y-base2.x*target.y)/(base1.x*base2.y-base2.x*base1.y),
		(target.x*base1.y-base1.x*target.y)/(base2.x*base1.y-base1.x*base2.y)
		);
}

px_point2D PX_Point2DDiv(px_point2D p1,px_float m)
{
	p1.x/=m;
	p1.y/=m;
	return p1;
}


px_float PX_PointDot(px_point p1,px_point p2)
/* 三维向量点积运算 */
{
	return p1.x*p2.x+p1.y*p2.y+p1.z*p2.z;
}

px_float PX_Point2DDot(px_point2D p1,px_point2D p2)
{
	return p1.x*p2.x+p1.y*p2.y;
}

px_float PX_Point4DDot(px_point4D p1,px_point4D p2)
{
	return p1.x*p2.x+p1.y*p2.y+p1.z*p2.z;
}

px_point PX_PointCross(px_point p1,px_point p2)
{
	px_point pt;
	pt.x=p1.y*p2.z-p2.y*p1.z;
	pt.y=p1.z*p2.x-p2.z*p1.x;
	pt.z=p1.x*p2.y-p2.x*p1.y;
	return pt;
}

px_float PX_Point2DCross(px_point2D p1,px_point2D p2)
{
	return p1.x*p2.y-p2.x*p1.y;
}



px_point4D PX_Point4DCross(px_point4D p1,px_point4D p2)
{
	px_point4D pt;
	pt.x=p1.y*p2.z-p2.y*p1.z;
	pt.y=p1.z*p2.x-p2.z*p1.x;
	pt.z=p1.x*p2.y-p2.x*p1.y;
	pt.w=1;
	return pt;
}

px_point PX_PointInverse(px_point p1)
{
	return PX_POINT(-p1.x,-p1.y,-p1.z);
}

px_point2D PX_Point2DInverse(px_point2D p1)
{
	return PX_POINT2D(-p1.x, -p1.y);
}


px_float PX_PointMod(px_point p)
{
	return PX_sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
}

px_float PX_Point2DMod(px_point2D p)
{
	return PX_sqrt(p.x*p.x+p.y*p.y);
}


px_float  PX_PointSquare(px_point p)
{
	return (p.x*p.x+p.y*p.y+p.z*p.z);
}

px_float  PX_Point2DSquare(px_point2D p)
{
	return (p.x * p.x + p.y * p.y );
}


px_point PX_PointNormalization(px_point p)
{
	if (p.x||p.y||p.z)
	{
		return PX_PointDiv(p,PX_PointMod(p));
	}
	return p;
}

px_point2D PX_Point2DNormalization(px_point2D p)
{
	if (p.x||p.y)
	{
		return PX_Point2DDiv(p,PX_Point2DMod(p));
	}
	return p;
}

px_void PX_memset(px_void *dst,px_byte byte,px_int size)
{
	px_dword dw=byte?(byte<<24)|(byte<<16)|(byte<<8)|byte:0;
	px_dword *_4byteMovDst=(px_dword *)dst;
	px_uchar *pdst=(px_uchar *)dst+(size&~3);
	px_uint _movTs=size>>2;
	while (_movTs--)
	{
		*_4byteMovDst++=dw;
	}
	_movTs=size&3;
	while (_movTs--)
		*(pdst++)=byte;
}


px_void PX_memdwordset(px_void *dst,px_dword dw,px_int count)
{
	px_dword *p=(px_dword *)dst;
	if (p==0)
	{
		PX_ASSERT();
	}
	/* 性能优化：全 0 路径（每帧 SurfaceClearAll 黑色）走 memset，
	 * 其底层带 DC ZVA / cache 预取，远快于逐 dword 循环。
	 * 非 0 值走 4 路展开循环。 */
	if (dw == 0)
	{
		memset(dst, 0, (size_t)count * sizeof(px_dword));
		return;
	}
	while (count >= 4)
	{
		p[0] = dw;
		p[1] = dw;
		p[2] = dw;
		p[3] = dw;
		p += 4;
		count -= 4;
	}
	while (count-- > 0)
	{
		*p++ = dw;
	}
}


px_bool PX_memequ(const px_void *dst,const px_void *src,px_int size)
{
	px_dword *_4byteMovSrc=(px_dword *)src;
	px_dword *_4byteMovDst=(px_dword *)dst;
	px_uchar *psrc=(px_uchar *)src+(size&~3);
	px_uchar *pdst=(px_uchar *)dst+(size&~3);
	px_uint _movTs=size>>2;
	if (dst==PX_NULL||src==PX_NULL)
	{
		PX_ASSERT();
		return PX_FALSE;
	}
	while (_movTs--)
	{
		if(*_4byteMovDst++!=*_4byteMovSrc++)
			return PX_FALSE;
	}
	_movTs=size&3;
	while (_movTs--)
/* 内存拷贝（支持重叠区域和块拷贝优化） */
		if(*(pdst++)!=*(psrc++))
			return PX_FALSE;
	return PX_TRUE;
}


px_void PX_memcpy(px_void *dst,const px_void *src,px_int size)
{
	typedef struct
	{
		px_byte m[16];
	}PX_MEMCPY_16;

	typedef struct
	{
		px_byte m[32];
	}PX_MEMCPY_32;

	typedef struct
	{
		px_byte m[64];
	}PX_MEMCPY_64;

	typedef struct
	{
		px_byte m[128];
	}PX_MEMCPY_128;

	typedef struct
	{
		px_byte m[256];
	}PX_MEMCPY_256;

	typedef struct
	{
		px_byte m[512];
	}PX_MEMCPY_512;

	typedef struct
	{
		px_byte m[1024];
	}PX_MEMCPY_1024;

	typedef struct
	{
		px_byte m[2048];
	}PX_MEMCPY_2048;

	typedef struct
	{
		px_byte m[4096];
	}PX_MEMCPY_4096;

	px_dword *_4byteMovSrc;
	px_dword *_4byteMovDst;
	px_uchar *psrc;
	px_uchar *pdst;
	PX_MEMCPY_4096 *_4kbyteMovSrc,*_4kbyteMovDst;
	PX_MEMCPY_2048 *_2kbyteMovSrc,*_2kbyteMovDst;
	PX_MEMCPY_1024 *_1kbyteMovSrc,*_1kbyteMovDst;
	PX_MEMCPY_512 *_512byteMovSrc,*_512byteMovDst;
	PX_MEMCPY_256 *_256byteMovSrc,*_256byteMovDst;
	PX_MEMCPY_128 *_128byteMovSrc,*_128byteMovDst;
	PX_MEMCPY_64 *_64byteMovSrc,*_64byteMovDst;
	PX_MEMCPY_32 *_32byteMovSrc,*_32byteMovDst;
	PX_MEMCPY_16 *_16byteMovSrc,*_16byteMovDst;
	px_uint _movTs;

	if (size<=0)
	{
		return;
	}
	//is overlap?
	if (dst>src&&(px_char *)dst<(px_char *)src+size)
	{
		px_dword _4byteMov=0;//4bytes unit-copy
		//backward overlap
		psrc=(px_uchar *)src+size-1;
		pdst=(px_uchar *)dst+size-1;

		_movTs=size&3;

		while (_movTs--)
			*(pdst--)=*(psrc--);

		pdst-=3;
		psrc-=3;
		_4byteMovDst=(px_dword *)pdst;
		_4byteMovSrc=(px_dword *)psrc;
		_movTs=size>>2;
		while (_movTs--)
		{
			_4byteMov=*_4byteMovSrc;
			_4byteMovSrc--;
			*_4byteMovDst=_4byteMov;
			_4byteMovDst--;
		}
	}
	else if (src>dst&&(px_char *)src<(px_char *)dst+size)
	{
		px_dword _4byteMov=0;//4bytes unit-copy

		//forward overlap
		psrc=(px_uchar *)src;
		pdst=(px_uchar *)dst;

		_4byteMovDst=(px_dword *)pdst;
		_4byteMovSrc=(px_dword *)psrc;
		_movTs=size>>2;
		while (_movTs--)
		{
			_4byteMov=*_4byteMovSrc;
			_4byteMovSrc++;
			*_4byteMovDst=_4byteMov;
			_4byteMovDst++;
		}
		_movTs=size&3;

		psrc=(px_uchar *)_4byteMovSrc;
		pdst=(px_uchar *)_4byteMovDst;

		while (_movTs--)
			*(pdst++)=*(psrc++);

		pdst+=3;
		psrc+=3;
	}
	else
	{
		//no overlap,using block-copy
		//high->low
		_movTs=size>>12;
		if(_movTs)
		{
			_4kbyteMovSrc=(PX_MEMCPY_4096 *)src;
			_4kbyteMovDst=(PX_MEMCPY_4096 *)dst;
			while(_movTs--)*_4kbyteMovDst++=*_4kbyteMovSrc++;
			src=_4kbyteMovSrc;
			dst=_4kbyteMovDst;
		}

		_movTs=size&(1<<11);
		if(_movTs)
		{
			_2kbyteMovSrc=(PX_MEMCPY_2048 *)src;
			_2kbyteMovDst=(PX_MEMCPY_2048 *)dst;
			*_2kbyteMovDst++=*_2kbyteMovSrc++;
			src=_2kbyteMovSrc;
			dst=_2kbyteMovDst;
		}

		_movTs=size&(1<<10);
		if(_movTs)
		{
			_1kbyteMovSrc=(PX_MEMCPY_1024 *)src;
			_1kbyteMovDst=(PX_MEMCPY_1024 *)dst;
			*_1kbyteMovDst++=*_1kbyteMovSrc++;
			src=_1kbyteMovSrc;
			dst=_1kbyteMovDst;
		}

		_movTs=size&(1<<9);
		if(_movTs)
		{
			_512byteMovSrc=(PX_MEMCPY_512 *)src;
			_512byteMovDst=(PX_MEMCPY_512 *)dst;
			*_512byteMovDst++=*_512byteMovSrc++;
			src=_512byteMovSrc;
			dst=_512byteMovDst;
		}

		_movTs=size&(1<<8);
		if(_movTs)
		{
			_256byteMovSrc=(PX_MEMCPY_256 *)src;
			_256byteMovDst=(PX_MEMCPY_256 *)dst;
			*_256byteMovDst++=*_256byteMovSrc++;
			src=_256byteMovSrc;
			dst=_256byteMovDst;
		}

		_movTs=size&(1<<7);
		if(_movTs)
		{
			_128byteMovSrc=(PX_MEMCPY_128 *)src;
			_128byteMovDst=(PX_MEMCPY_128 *)dst;
			*_128byteMovDst++=*_128byteMovSrc++;
			src=_128byteMovSrc;
			dst=_128byteMovDst;
		}

		_movTs=size&(1<<6);
		if(_movTs)
		{
			_64byteMovSrc=(PX_MEMCPY_64 *)src;
			_64byteMovDst=(PX_MEMCPY_64 *)dst;
			*_64byteMovDst++=*_64byteMovSrc++;
			src=_64byteMovSrc;
			dst=_64byteMovDst;
		}

		_movTs=size&(1<<5);
		if(_movTs)
		{
			_32byteMovSrc=(PX_MEMCPY_32 *)src;
			_32byteMovDst=(PX_MEMCPY_32 *)dst;
			*_32byteMovDst++=*_32byteMovSrc++;
			src=_32byteMovSrc;
			dst=_32byteMovDst;
		}

		_movTs=size&(1<<4);
		if(_movTs)
		{
			_16byteMovSrc=(PX_MEMCPY_16 *)src;
			_16byteMovDst=(PX_MEMCPY_16 *)dst;
			*_16byteMovDst++=*_16byteMovSrc++;
			src=_16byteMovSrc;
			dst=_16byteMovDst;
		}

		_movTs=size&0x0F;
		if(_movTs>=12)
		{
			_4byteMovSrc=(px_dword *)src;
			_4byteMovDst=(px_dword *)dst;
			*_4byteMovDst++=*_4byteMovSrc++;
			*_4byteMovDst++=*_4byteMovSrc++;
			*_4byteMovDst++=*_4byteMovSrc++;
			_movTs-=12;
			psrc=(px_uchar *)_4byteMovSrc;
			pdst=(px_uchar *)_4byteMovDst;
			while(_movTs--)*pdst++=*psrc++;
		}
		else if(_movTs>=8)
		{
			_4byteMovSrc=(px_dword *)src;
			_4byteMovDst=(px_dword *)dst;
			*_4byteMovDst++=*_4byteMovSrc++;
			*_4byteMovDst++=*_4byteMovSrc++;
			_movTs-=8;
			psrc=(px_uchar *)_4byteMovSrc;
			pdst=(px_uchar *)_4byteMovDst;
			while(_movTs--)*pdst++=*psrc++;
		}
		else if(_movTs>=4)
		{
			_4byteMovSrc=(px_dword *)src;
			_4byteMovDst=(px_dword *)dst;
			*_4byteMovDst++=*_4byteMovSrc++;
			_movTs-=4;
			psrc=(px_uchar *)_4byteMovSrc;
			pdst=(px_uchar *)_4byteMovDst;
			while(_movTs--)*pdst++=*psrc++;
		}
		else
		{
			psrc=(px_uchar *)src;
			pdst=(px_uchar *)dst;
			while(_movTs--)*pdst++=*psrc++;
		}
		
	}
}

px_point PX_PointMulMatrix(px_point p,px_matrix mat)
{
	px_point point;
	point.x=p.x*mat._11+p.y*mat._21+p.z*mat._31+1*mat._41;
	point.y=p.x*mat._12+p.y*mat._22+p.z*mat._32+1*mat._42;
	point.z=p.x*mat._13+p.y*mat._23+p.z*mat._33+1*mat._43;
	return point;
}

px_point PX_POINT(px_float x,px_float y,px_float z)
{
	px_point p;
	p.x=x;
	p.y=y;
	p.z=z;
	return p;
}

px_point2D PX_POINT2D(px_float x,px_float y)
{
	px_point2D p;
	p.x=x;
	p.y=y;
	return p;
}
