#include "PX_LiveFramework.h"
#include "PX_LiveDeviceFormat.h"

typedef struct
{
	union
	{
		struct
		{
			px_uchar r;
			px_uchar g;
			px_uchar b;
			px_uchar a;
		};
		px_dword ucolor;
	}_argb;
}px_liveframework_rgba_color;

typedef struct
{
	px_point position;
	px_point normal;
	px_float u,v;
}PX_LiveRenderVertex;
static px_void PX_LiveFramework_RenderListPixelShaderFaster(px_surface *psurface,px_int x,px_int y,px_float z,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend)
{
	//texture mapping

	px_int resWidth;
	px_int resHeight;

	if (u<0||u>1||v<0||v>1)
	{
		return;
	}

	if (pTexture)
	{
		resWidth=pTexture->width;
		resHeight=pTexture->height;

		PX_SurfaceDrawPixel(psurface,x,y,PX_SURFACECOLOR(pTexture,(px_int)(u*resWidth),(px_int)(v*resHeight)));
	}
}
static px_void PX_LiveFramework_RenderListPixelShader(px_surface *psurface,px_int x,px_int y,px_float z,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend)
{
	//texture mapping
	px_double SampleX,SampleY,mapX,mapY;
	px_double mixa,mixr,mixg,mixb,Weight;
	px_color sampleColor;
	px_int resWidth;
	px_int resHeight;

	if (u<0||u>1||v<0||v>1)
	{
		return;
	}


	if (pTexture)
	{
		resWidth=pTexture->width;
		resHeight=pTexture->height;
		u=PX_ABS(u);
		v=PX_ABS(v);
		u-=(px_int)u;
		v-=(px_int)v;

		mapX=u*resWidth;
		mapY=v*resHeight;

		if (mapX<-0.5||mapX>resWidth+0.5)
		{
			return;
		}
		if (mapY<-0.5||mapY>resHeight+0.5)
		{
			return;
		}
		mixa=0;
		mixr=0;
		mixg=0;
		mixb=0;
		//Sample 4 points
		//lt

		SampleX=(mapX-0.5f);
		SampleY=(mapY-0.5f);

		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			//Sample color from resTexture
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
			Weight=(1-PX_FRAC(SampleX))*(1-PX_FRAC(SampleY));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		SampleX=(mapX+0.5f);
		SampleY=(mapY-0.5f);
		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			//Sample color from resTexture
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
			Weight=PX_FRAC(mapX+0.5f)*(1-PX_FRAC(mapY-0.5f));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		SampleX=(mapX-0.5f);
		SampleY=(mapY+0.5f);
		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			//Sample color from resTexture
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
			Weight=(1-PX_FRAC(SampleX))*(PX_FRAC(SampleY));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		SampleX=(mapX+0.5f);
		SampleY=(mapY+0.5f);
		if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
		{
			//Sample color from resTexture
			sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
			Weight=(PX_FRAC(SampleX))*(PX_FRAC(SampleY));
			mixa+=sampleColor._argb.a*Weight;
			mixr+=sampleColor._argb.r*Weight;
			mixg+=sampleColor._argb.g*Weight;
			mixb+=sampleColor._argb.b*Weight;
		}

		if (blend)
		{
			mixa*=blend->alpha;
			mixr*=blend->hdr_R;
			mixg*=blend->hdr_G;
			mixb*=blend->hdr_B;
		}

		mixa>255?mixa=255:0;
		mixr>255?mixr=255:0;
		mixg>255?mixg=255:0;
		mixb>255?mixb=255:0;

		PX_SurfaceDrawPixel(psurface,x,y,PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb));

	}
}

/* Literal float copy of the double bilinear taps. Straight RGB out. */
static px_void PX_LiveFramework_RenderListPixelShaderFloat(px_surface *psurface,px_int x,px_int y,px_float z,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend)
{
	px_float SampleX,SampleY,mapX,mapY;
	px_float mixa,mixr,mixg,mixb,Weight;
	px_color sampleColor;
	px_int resWidth;
	px_int resHeight;
	(void)z;
	(void)normal;
	if (u<0||u>1||v<0||v>1||!pTexture)
	{
		return;
	}
	resWidth=pTexture->width;
	resHeight=pTexture->height;
	u=PX_ABS(u);
	v=PX_ABS(v);
	u-=(px_int)u;
	v-=(px_int)v;
	mapX=u*resWidth;
	mapY=v*resHeight;
	if (mapX<-0.5f||mapX>resWidth+0.5f||mapY<-0.5f||mapY>resHeight+0.5f)
	{
		return;
	}
	mixa=0;
	mixr=0;
	mixg=0;
	mixb=0;
	SampleX=(mapX-0.5f);
	SampleY=(mapY-0.5f);
	if (SampleX>0&&SampleX<resWidth&&SampleY>0&&SampleY<resHeight)
	{
		sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
		Weight=(1-PX_FRAC(SampleX))*(1-PX_FRAC(SampleY));
		mixa+=sampleColor._argb.a*Weight;
		mixr+=sampleColor._argb.r*Weight;
		mixg+=sampleColor._argb.g*Weight;
		mixb+=sampleColor._argb.b*Weight;
	}
	SampleX=(mapX+0.5f);
	SampleY=(mapY-0.5f);
	if (SampleX>0&&SampleX<resWidth&&SampleY>0&&SampleY<resHeight)
	{
		sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
		Weight=PX_FRAC(mapX+0.5f)*(1-PX_FRAC(mapY-0.5f));
		mixa+=sampleColor._argb.a*Weight;
		mixr+=sampleColor._argb.r*Weight;
		mixg+=sampleColor._argb.g*Weight;
		mixb+=sampleColor._argb.b*Weight;
	}
	SampleX=(mapX-0.5f);
	SampleY=(mapY+0.5f);
	if (SampleX>0&&SampleX<resWidth&&SampleY>0&&SampleY<resHeight)
	{
		sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
		Weight=(1-PX_FRAC(SampleX))*(PX_FRAC(SampleY));
		mixa+=sampleColor._argb.a*Weight;
		mixr+=sampleColor._argb.r*Weight;
		mixg+=sampleColor._argb.g*Weight;
		mixb+=sampleColor._argb.b*Weight;
	}
	SampleX=(mapX+0.5f);
	SampleY=(mapY+0.5f);
	if (SampleX>0&&SampleX<resWidth&&SampleY>0&&SampleY<resHeight)
	{
		sampleColor=PX_SURFACECOLOR(pTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
		Weight=(PX_FRAC(SampleX))*(PX_FRAC(SampleY));
		mixa+=sampleColor._argb.a*Weight;
		mixr+=sampleColor._argb.r*Weight;
		mixg+=sampleColor._argb.g*Weight;
		mixb+=sampleColor._argb.b*Weight;
	}
	if (blend)
	{
		mixa*=blend->alpha;
		mixr*=blend->hdr_R;
		mixg*=blend->hdr_G;
		mixb*=blend->hdr_B;
	}
	if (mixa>255) mixa=255;
	if (mixr>255) mixr=255;
	if (mixg>255) mixg=255;
	if (mixb>255) mixb=255;
	PX_SurfaceDrawPixel(psurface,x,y,PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb));
}

/* Filter RGB in premultiplied space, then store straight RGB for the surface blend. */
static px_void PX_LiveFramework_RenderListPixelShaderPremul(px_surface *psurface,px_int x,px_int y,px_float z,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend)
{
	px_float mapX,mapY,cover,preR,preG,preB,mixa,mixr,mixg,mixb;
	px_int resWidth,resHeight,corner;
	(void)z;
	(void)normal;
	if (u<0||u>1||v<0||v>1||!pTexture)
	{
		return;
	}
	resWidth=pTexture->width;
	resHeight=pTexture->height;
	mapX=u*(px_float)resWidth;
	mapY=v*(px_float)resHeight;
	cover=0;
	preR=0;
	preG=0;
	preB=0;
	for (corner=0;corner<4;corner++)
	{
		px_float sampleX=(corner&1)?mapX+0.5f:mapX-0.5f;
		px_float sampleY=(corner&2)?mapY+0.5f:mapY-0.5f;
		px_int texelX=(px_int)sampleX;
		px_int texelY=(px_int)sampleY;
		px_float wx,wy,aw;
		px_color sampleColor;
		if (texelX<0||texelY<0||texelX>=resWidth||texelY>=resHeight)
		{
			continue;
		}
		wx=(corner&1)?(sampleX-texelX):(1.0f-(sampleX-texelX));
		wy=(corner&2)?(sampleY-texelY):(1.0f-(sampleY-texelY));
		sampleColor=PX_SURFACECOLOR(pTexture,texelX,texelY);
		aw=sampleColor._argb.a*wx*wy;
		cover+=aw;
		preR+=sampleColor._argb.r*aw;
		preG+=sampleColor._argb.g*aw;
		preB+=sampleColor._argb.b*aw;
	}
	if (cover<=0.01f)
	{
		return;
	}
	mixa=cover;
	mixr=preR/cover;
	mixg=preG/cover;
	mixb=preB/cover;
	if (blend)
	{
		mixa*=blend->alpha;
		mixr*=blend->hdr_R;
		mixg*=blend->hdr_G;
		mixb*=blend->hdr_B;
	}
	if (mixa>255) mixa=255;
	if (mixr>255) mixr=255;
	if (mixg>255) mixg=255;
	if (mixb>255) mixb=255;
	PX_SurfaceDrawPixel(psurface,x,y,PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb));
}

static px_void PX_LiveFramework_ShadePixel(PX_LiveFramework *pLiveFramework,px_surface *psurface,px_int x,px_int y,px_float z,px_float u,px_float v,px_point normal,px_texture *pTexture,PX_TEXTURERENDER_BLEND *blend)
{
	if (pLiveFramework->pixelShader)
	{
		px_point position;
		position.x=0;
		position.y=0;
		position.z=z;
		pLiveFramework->pixelShader(psurface,x,y,position,u,v,normal,pTexture,blend);
		return;
	}
	switch (pLiveFramework->samplerMode)
	{
	case 1:
		PX_LiveFramework_RenderListPixelShaderFloat(psurface,x,y,z,u,v,normal,pTexture,blend);
		break;
	case 2:
		PX_LiveFramework_RenderListPixelShaderFaster(psurface,x,y,z,u,v,normal,pTexture,blend);
		break;
	case 3:
		PX_LiveFramework_RenderListPixelShaderPremul(psurface,x,y,z,u,v,normal,pTexture,blend);
		break;
	default:
		PX_LiveFramework_RenderListPixelShader(psurface,x,y,z,u,v,normal,pTexture,blend);
		break;
	}
}

static px_void PX_LiveFramework_RenderListRasterization(px_surface *psurface,PX_LiveFramework *pLiveFramework,PX_LiveRenderVertex p0,PX_LiveRenderVertex p1,PX_LiveRenderVertex p2,px_texture *ptexture,PX_TEXTURERENDER_BLEND *blend)
{
	px_int   ix,iy;
	px_bool  k01infinite=PX_FALSE;
	px_bool  k02infinite=PX_FALSE;
	px_bool  k12infinite=PX_FALSE;
	px_float k01,b01,k02,b02,k12,b12;
	px_float x0;
	px_float y0;
	px_float z0;
	px_float s0;
	px_float t0;

	px_float x1;
	px_float y1;
	px_float z1;
	px_float s1;
	px_float t1;

	px_float x2;
	px_float y2;
	px_float z2;
	px_float s2;
	px_float t2;


	px_float y,xleft, xright; 
	px_float oneoverz_left, oneoverz_right; 
	px_float oneoverz_top, oneoverz_bottom; 
	px_float oneoverz, oneoverz_step;   
	px_float soverz_top, soverz_bottom; 
	px_float toverz_top, toverz_bottom; 
	px_float soverz_left, soverz_right; 
	px_float toverz_left, toverz_right;
	px_float soverz, soverz_step;
	px_float toverz, toverz_step; 
	px_float s, t;
	px_float btmy,midy;
	px_float originalZ;
	px_bool unitZ;

	px_float a,b,c;
	px_point position;
	unitZ=(p0.position.z==1.0f&&p1.position.z==1.0f&&p2.position.z==1.0f);
	a=(px_float)PX_sqrtd((p1.position.x-p2.position.x)*(p1.position.x-p2.position.x));
	b=(px_float)PX_sqrtd((p0.position.x-p2.position.x)*(p0.position.x-p2.position.x));
	c=(px_float)PX_sqrtd((p1.position.x-p0.position.x)*(p1.position.x-p0.position.x));

	position.x=(a*p0.position.x+b*p1.position.x+c*p2.position.x)/(a+b+c);
	position.y=(a*p0.position.y+b*p1.position.y+c*p2.position.y)/(a+b+c);

	//    p0
	// p1   p2

	if (p1.position.y<p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p1;
		p1=p0;
		p0=t;
	}

	if (p2.position.y<p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p2;
		p2=p0;
		p0=t;
	}

	btmy=p1.position.y;
	midy=p2.position.y;
	if (p2.position.y>btmy)
	{
		midy=p1.position.y;
		btmy=p2.position.y;
	}



	do 
	{
		px_float x01m;

		x0=p0.position.x;
		y0=p0.position.y;
		x1=p1.position.x;
		y1=p1.position.y;
		x2=p2.position.x;
		y2=p2.position.y;


		if (x0==x1)
		{
			x01m=x0;
		}
		else
		{
			k01=(y0-y1)/(x0-x1);
			b01=y0-k01*x0;
			x01m=(y2-b01)/k01;
		}

		if (x01m>x2)
		{
			PX_LiveRenderVertex t;
			t=p2;
			p2=p1;
			p1=t;
		}
	} while (0);



	x0=p0.position.x;
	y0=p0.position.y;
	z0=p0.position.z;
	s0=p0.u;
	t0=p0.v;

	x1=p1.position.x;
	y1=p1.position.y;
	z1=p1.position.z;
	s1=p1.u;
	t1=p1.v;

	x2=p2.position.x;
	y2=p2.position.y;
	z2=p2.position.z;
	s2=p2.u;
	t2=p2.v;

	k01infinite=PX_FALSE;
	k02infinite=PX_FALSE;
	k12infinite=PX_FALSE;
	if (x0==x1)
	{
		k01infinite=PX_TRUE;
		k01 = 1;
		b01=x0;
	}
	else
	{
		k01=(y0-y1)/(x0-x1);
		b01=y0-k01*x0;
	}

	if (x0==x2)
	{
		k02infinite=PX_TRUE;
		k02 = 1;
		b02=x0;
	}
	else
	{
		k02=(y0-y2)/(x0-x2);
		b02=y0-k02*x0;
	}

	if (x1==x2)
	{
		k12infinite=PX_TRUE;
		b12=x0;
	}
	else
	{
		k12=(y1-y2)/(x1-x2);
		b12=y1-k12*x1;
	}


	{
	px_float yBegin=(px_int)(y0+0.5f)+0.5f;
	px_float yEnd=midy;
	if (yBegin<0.5f) yBegin=0.5f;
	if (psurface->height>0&&yEnd>(px_float)psurface->height-0.5f) yEnd=(px_float)psurface->height-0.5f;
	for(y = yBegin; y <=yEnd; y++)
	{
		if (k01infinite)
		{
			xleft=b01;
		}
		else
		{
			xleft = (y-b01)/k01;
		}

		if (k02infinite)
		{
			xright=b02;
		}
		else
		{
			xright = (y-b02)/k02;
		}


		if (unitZ)
		{
			px_float sLeft=(y1==y0)?s0:((y-y0)*(s1-s0)/(y1-y0)+s0);
			px_float sRight=(y2==y0)?s0:((y-y0)*(s2-s0)/(y2-y0)+s0);
			px_float tLeft=(y1==y0)?t0:((y-y0)*(t1-t0)/(y1-y0)+t0);
			px_float tRight=(y2==y0)?t0:((y-y0)*(t2-t0)/(y2-y0)+t0);
			soverz=sLeft;
			toverz=tLeft;
			soverz_step=(xright==xleft)?0.0f:((sRight-sLeft)/(xright-xleft));
			toverz_step=(xright==xleft)?0.0f:((tRight-tLeft)/(xright-xleft));
			oneoverz=1.0f;
			oneoverz_step=0.0f;
		}
		else
		{
		oneoverz_top = 1.0f / z0;
		oneoverz_bottom = 1.0f/z1;
		oneoverz_left = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y1-y0) + oneoverz_top;
		oneoverz_bottom = 1.0f / z2;
		oneoverz_right = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y2-y0) + oneoverz_top;
		oneoverz_step = (oneoverz_right-oneoverz_left) / (xright-xleft);
		soverz_top = s0 / z0;
		soverz_bottom = s1 / z1;
		soverz_left = (y-y0) * (soverz_bottom-soverz_top) / (y1-y0) + soverz_top;
		soverz_bottom = s2 / z2;
		soverz_right = (y-y0) * (soverz_bottom-soverz_top) / (y2-y0) + soverz_top;
		soverz_step = (soverz_right-soverz_left) / (xright-xleft);
		toverz_top = t0 / z0;
		toverz_bottom = t1 / z1;
		toverz_left = (y-y0) * (toverz_bottom-toverz_top) / (y1-y0) + toverz_top;
		toverz_bottom = t2 / z2;
		toverz_right = (y-y0) * (toverz_bottom-toverz_top) / (y2-y0) + toverz_top;
		toverz_step = (toverz_right-toverz_left) / (xright-xleft);
		oneoverz = oneoverz_left,soverz = soverz_left, toverz = toverz_left;
		}

		{
		px_int ixBegin=(px_int)(xleft+0.5f);
		px_int ixEnd=(px_int)(xright+0.5f);
		if (ixBegin<0)
		{
			px_float skip=(px_float)(-ixBegin);
			soverz+=soverz_step*skip;
			toverz+=toverz_step*skip;
			oneoverz+=oneoverz_step*skip;
			ixBegin=0;
		}
		if (ixEnd>psurface->width) ixEnd=psurface->width;
		for(ix = ixBegin;ix < ixEnd; ++ix)
		{
			if (unitZ)
			{
				s=soverz;
				t=toverz;
				originalZ=1.0f;
			}
			else
			{
				s = soverz / oneoverz;
				t = toverz / oneoverz;
				originalZ=1.0f/oneoverz;
			}
			iy=(px_int)y;
			PX_LiveFramework_ShadePixel(pLiveFramework,psurface,ix,iy,originalZ,s,t,p0.normal,ptexture,blend);
			oneoverz += oneoverz_step;
			soverz += soverz_step;
			toverz += toverz_step;
		}
		}
	}
	}

	// p1   p2
	//    p0
	if (p1.position.y>p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p1;
		p1=p0;
		p0=t;
	}

	if (p2.position.y>p0.position.y)
	{
		PX_LiveRenderVertex t;
		t=p2;
		p2=p0;
		p0=t;
	}

	btmy=p1.position.y;
	midy=p2.position.y;
	if (p2.position.y<btmy)
	{
		midy=p1.position.y;
		btmy=p2.position.y;
	}



	do 
	{
		px_float x01m;

		x0=p0.position.x;
		y0=p0.position.y;
		x1=p1.position.x;
		y1=p1.position.y;
		x2=p2.position.x;
		y2=p2.position.y;


		if (x0==x1)
		{
			x01m=x0;
		}
		else
		{
			k01=(y0-y1)/(x0-x1);
			b01=y0-k01*x0;
			x01m=(y2-b01)/k01;
		}

		if (x01m>x2)
		{
			PX_LiveRenderVertex t;
			t=p2;
			p2=p1;
			p1=t;
		}
	} while (0);



	x0=p0.position.x;
	y0=p0.position.y;
	z0=p0.position.z;
	s0=p0.u;
	t0=p0.v;

	x1=p1.position.x;
	y1=p1.position.y;
	z1=p1.position.z;
	s1=p1.u;
	t1=p1.v;

	x2=p2.position.x;
	y2=p2.position.y;
	z2=p2.position.z;
	s2=p2.u;
	t2=p2.v;

	k01infinite=PX_FALSE;
	k02infinite=PX_FALSE;
	k12infinite=PX_FALSE;
	if (x0==x1)
	{
		k01infinite=PX_TRUE;
		b01=x0;
	}
	else
	{
		k01=(y0-y1)/(x0-x1);
		b01=y0-k01*x0;
	}

	if (x0==x2)
	{
		k02infinite=PX_TRUE;
		b02=x0;
	}
	else
	{
		k02=(y0-y2)/(x0-x2);
		b02=y0-k02*x0;
	}

	if (x1==x2)
	{
		k12infinite=PX_TRUE;
		b12=x0;
	}
	else
	{
		k12=(y1-y2)/(x1-x2);
		b12=y1-k12*x1;
	}


	{
	px_float yBegin2=(px_int)(midy+0.5f)+0.5f;
	px_float yEnd2=y0;
	if (yBegin2<0.5f) yBegin2=0.5f;
	if (psurface->height>0&&yEnd2>(px_float)psurface->height) yEnd2=(px_float)psurface->height;
	for(y = yBegin2; y < yEnd2; y++)
	{
		if (k01infinite)
		{
			xleft=b01;
		}
		else
		{
			xleft = (y-b01)/k01;
		}

		if (k02infinite)
		{
			xright=b02;
		}
		else
		{
			xright = (y-b02)/k02;
		}


		if (unitZ)
		{
			px_float sLeft=(y1==y0)?s0:((y-y0)*(s1-s0)/(y1-y0)+s0);
			px_float sRight=(y2==y0)?s0:((y-y0)*(s2-s0)/(y2-y0)+s0);
			px_float tLeft=(y1==y0)?t0:((y-y0)*(t1-t0)/(y1-y0)+t0);
			px_float tRight=(y2==y0)?t0:((y-y0)*(t2-t0)/(y2-y0)+t0);
			soverz=sLeft;
			toverz=tLeft;
			soverz_step=(xright==xleft)?0.0f:((sRight-sLeft)/(xright-xleft));
			toverz_step=(xright==xleft)?0.0f:((tRight-tLeft)/(xright-xleft));
			oneoverz=1.0f;
			oneoverz_step=0.0f;
		}
		else
		{
		oneoverz_top = 1.0f / z0;
		oneoverz_bottom = 1.0f/z1;
		oneoverz_left = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y1-y0) + oneoverz_top;
		oneoverz_bottom = 1.0f / z2;
		oneoverz_right = (y-y0) * (oneoverz_bottom-oneoverz_top) / (y2-y0) + oneoverz_top;
		oneoverz_step = (oneoverz_right-oneoverz_left) / (xright-xleft);
		soverz_top = s0 / z0;
		soverz_bottom = s1 / z1;
		soverz_left = (y-y0) * (soverz_bottom-soverz_top) / (y1-y0) + soverz_top;
		soverz_bottom = s2 / z2;
		soverz_right = (y-y0) * (soverz_bottom-soverz_top) / (y2-y0) + soverz_top;
		soverz_step = (soverz_right-soverz_left) / (xright-xleft);
		toverz_top = t0 / z0;
		toverz_bottom = t1 / z1;
		toverz_left = (y-y0) * (toverz_bottom-toverz_top) / (y1-y0) + toverz_top;
		toverz_bottom = t2 / z2;
		toverz_right = (y-y0) * (toverz_bottom-toverz_top) / (y2-y0) + toverz_top;
		toverz_step = (toverz_right-toverz_left) / (xright-xleft);
		oneoverz = oneoverz_left,soverz = soverz_left, toverz = toverz_left;
		}

		{
		px_int ixBegin=(px_int)(xleft+0.5f);
		px_int ixEnd=(px_int)(xright+0.5f);
		if (ixBegin<0)
		{
			px_float skip=(px_float)(-ixBegin);
			soverz+=soverz_step*skip;
			toverz+=toverz_step*skip;
			oneoverz+=oneoverz_step*skip;
			ixBegin=0;
		}
		if (ixEnd>psurface->width) ixEnd=psurface->width;
		for(ix = ixBegin;ix < ixEnd; ++ix)
		{
			if (unitZ)
			{
				s=soverz;
				t=toverz;
				originalZ=1.0f;
			}
			else
			{
				s = soverz / oneoverz;
				t = toverz / oneoverz;
				originalZ=1.0f/oneoverz;
			}
			iy=(px_int)y;
			PX_LiveFramework_ShadePixel(pLiveFramework,psurface,ix,iy,originalZ,s,t,p0.normal,ptexture,blend);
			oneoverz += oneoverz_step;
			soverz += soverz_step;
			toverz += toverz_step;
		}
		}
	}
	}

}

px_bool PX_LiveFrameworkInitialize(px_memorypool *mp,PX_LiveFramework *plive,px_int width,px_int height)
{
	PX_memset(plive,0,sizeof(PX_LiveFramework));
	plive->view_scale=1.0f;
	plive->view_revision=1;
	plive->mp=mp;
	plive->animationMode=PX_LIVE_MODE_NEUTRAL;
	PX_LiveRealtimeInitialize(&plive->realtime,mp);
	if(!PX_VectorInitialize(mp,&plive->layers,sizeof(PX_LiveLayer),1))return PX_FALSE;
	if(!PX_VectorInitialize(mp,&plive->livetextures,sizeof(PX_LiveTexture),1))return PX_FALSE;
	if(!PX_VectorInitialize(mp,&plive->liveAnimations,sizeof(PX_LiveAnimation),1))return PX_FALSE;
	plive->width=width;
	plive->height=height;
	plive->showFocusLayer=PX_TRUE;
	plive->currentEditAnimationIndex=-1;
	plive->currentEditFrameIndex=-1;
	plive->currentEditVertexIndex=-1;
	plive->currentEditLayerIndex=-1;
	return PX_TRUE;
}

px_void PX_LiveFrameworkPlay(PX_LiveFramework *plive)
{
	if (plive->animationMode==PX_LIVE_MODE_REALTIME30)
	{
		PX_LiveRealtimeLeave(plive);
	}
	plive->animationMode=PX_LIVE_MODE_TIMELINE;
	plive->status=PX_LIVEFRAMEWORK_STATUS_PLAYING;
}

px_void PX_LiveFrameworkPause(PX_LiveFramework *plive)
{
	plive->status=PX_LIVEFRAMEWORK_STATUS_STOP;
}

px_void PX_LiveFrameworkReset(PX_LiveFramework *plive)
{
	px_int i;
	plive->reg_duration=0;
	plive->reg_ip=0;
	plive->reg_elapsed=0;
	plive->reg_bp=-1;
	plive->status=PX_LIVEFRAMEWORK_STATUS_STOP;
	plive->animationMode=PX_LIVE_MODE_NEUTRAL;
	PX_LiveRealtimeResetAll(plive);

	for (i=0;i<plive->layers.size;i++)
	{
		px_int j;
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		pLayer->rel_beginRotationAngle=0;
		pLayer->rel_currentRotationAngle=0;
		pLayer->rel_endRotationAngle=0;

		pLayer->rel_beginTranslation=PX_POINT(0,0,0);
		pLayer->rel_currentTranslation=PX_POINT(0,0,0);
		pLayer->rel_endTranslation=PX_POINT(0,0,0);

		pLayer->rel_beginLocalTranslation=PX_POINT(0,0,0);
		pLayer->rel_currentLocalTranslation=PX_POINT(0,0,0);
		pLayer->rel_endLocalTranslation=PX_POINT(0,0,0);
		pLayer->rel_beginLocalRotationAngle=0;
		pLayer->rel_currentLocalRotationAngle=0;
		pLayer->rel_endLocalRotationAngle=0;
		pLayer->rel_beginLocalScale=1;
		pLayer->rel_currentLocalScale=1;
		pLayer->rel_endLocalScale=1;

		pLayer->rel_beginStretch=1;
		pLayer->rel_currentStretch=1;
		pLayer->rel_endStretch=1;

		pLayer->RenderTextureIndex=pLayer->LinkTextureIndex;
		pLayer->rel_impulse=PX_POINT(0,0,0);
		pLayer->panc_beginx=pLayer->panc_sx;
		pLayer->panc_currentx=pLayer->panc_sx;
		pLayer->panc_endx=pLayer->panc_sx;
		pLayer->panc_beginy=pLayer->panc_sy;
		pLayer->panc_currenty=pLayer->panc_sy;
		pLayer->panc_endy=pLayer->panc_sy;

		for (j=0;j<pLayer->vertices.size;j++)
		{
			PX_LiveVertex *pVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j);
			pVertex->beginTranslation=PX_POINT(0,0,0);
			pVertex->currentTranslation=PX_POINT(0,0,0);
			pVertex->endTranslation=PX_POINT(0,0,0);

			pVertex->currentPosition=pVertex->sourcePosition;
			pVertex->velocity=PX_POINT(0,0,0);
		}
	}
}

px_void PX_LiveFrameworkStop(PX_LiveFramework *plive)
{
	PX_LiveFrameworkReset(plive);
}

static px_void PX_LiveFrameworkUpdateLayerInterpolation(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_float schedule;
	px_int i;

	if (plive->status==PX_LIVEFRAMEWORK_STATUS_STOP)
	{
		schedule = 1;
	}
	else if (plive->reg_duration==0)
	{
		schedule=1;
	}
	else
	{
		schedule=plive->reg_elapsed*1.0f/plive->reg_duration;
	}

	if (schedule>1)
	{
		schedule=1;
	}

	//update parameters
	pLayer->panc_currentx= pLayer->panc_beginx + (pLayer->panc_endx - pLayer->panc_beginx) * schedule;
	pLayer->panc_currenty = pLayer->panc_beginy + (pLayer->panc_endy - pLayer->panc_beginy) * schedule;

	//point translation
	for (i=0;i<pLayer->vertices.size;i++)
	{
		px_point *pbegin,*pcurrent,*pend;
		pbegin=&PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i)->beginTranslation;
		pcurrent=&PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i)->currentTranslation;
		pend=&PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i)->endTranslation;

		pcurrent->x=pbegin->x+(pend->x-pbegin->x)*schedule;
		pcurrent->y=pbegin->y+(pend->y-pbegin->y)*schedule;
	}

	if (pLayer->parent_index!=-1)
	{
		//stretch
		pLayer->rel_currentStretch=pLayer->rel_beginStretch+(pLayer->rel_endStretch-pLayer->rel_beginStretch)*schedule;
	}
	else
	{
		//keypoint translation
		pLayer->rel_currentTranslation.x=pLayer->rel_beginTranslation.x+(pLayer->rel_endTranslation.x-pLayer->rel_beginTranslation.x)*schedule;
		pLayer->rel_currentTranslation.y=pLayer->rel_beginTranslation.y+(pLayer->rel_endTranslation.y-pLayer->rel_beginTranslation.y)*schedule;
		pLayer->rel_currentTranslation.z=0;
	}
	
	//rotation
	pLayer->rel_currentRotationAngle=pLayer->rel_beginRotationAngle+(pLayer->rel_endRotationAngle-pLayer->rel_beginRotationAngle)*schedule;

	//selected-layer-only visual transform
	pLayer->rel_currentLocalTranslation.x=pLayer->rel_beginLocalTranslation.x+(pLayer->rel_endLocalTranslation.x-pLayer->rel_beginLocalTranslation.x)*schedule;
	pLayer->rel_currentLocalTranslation.y=pLayer->rel_beginLocalTranslation.y+(pLayer->rel_endLocalTranslation.y-pLayer->rel_beginLocalTranslation.y)*schedule;
	pLayer->rel_currentLocalTranslation.z=0;
	pLayer->rel_currentLocalRotationAngle=pLayer->rel_beginLocalRotationAngle+(pLayer->rel_endLocalRotationAngle-pLayer->rel_beginLocalRotationAngle)*schedule;
	pLayer->rel_currentLocalScale=pLayer->rel_beginLocalScale+(pLayer->rel_endLocalScale-pLayer->rel_beginLocalScale)*schedule;

	

}

static px_void PX_LiveFramework_UpdateLayerKeyPoint(PX_LiveFramework *pLive,PX_LiveLayer *pLayer)
{
	px_int i;
	if (pLayer->parent_index!=-1)
	{
		//stretch
		px_point v=PX_PointSub(pLayer->keyPoint,PX_LiveFrameworkGetLayerParent(pLive,pLayer)->keyPoint);
		v=PX_PointMul(v,pLayer->rel_currentStretch);
		/* RT30 translation is local to the child and follows its parent transform. */
		if (pLive->animationMode==PX_LIVE_MODE_REALTIME30)
		{
			v=PX_PointAdd(v,pLayer->rel_currentTranslation);
		}
		v=PX_PointRotate(v,PX_LiveFrameworkGetLayerParent(pLive,pLayer)->rel_currentRotationAngle);
		pLayer->currentKeyPoint=PX_PointAdd(v,PX_LiveFrameworkGetLayerParent(pLive,pLayer)->currentKeyPoint);
	}
	else
	{
		pLayer->currentKeyPoint=PX_PointAdd(pLayer->keyPoint,pLayer->rel_currentTranslation);
	}
	pLayer->currentKeyPoint.z=pLayer->keyPoint.z;

	for (i=0;i<PX_COUNTOF(pLayer->child_index);i++)
	{
		if (pLayer->child_index[i]!=-1)
		{
			PX_LiveFramework_UpdateLayerKeyPoint(pLive,PX_LiveFrameworkGetLayerChild(pLive,pLayer,pLayer->child_index[i]));
		}
		else
		{
			break;
		}
	}
}

/*
 * Compose the selected-layer visual transforms from this layer up through its
 * ancestors.  The result maps an already evaluated hierarchy/world point to
 * its visual subtree position.  It is calculated once per layer update, not
 * once per vertex per ancestor.
 */
static px_void PX_LiveFramework_GetLayerVisualTransform(PX_LiveFramework *pLive,PX_LiveLayer *pLayer,px_float *pScale,px_float *pRotation,px_point *pTranslation)
{
	PX_LiveLayer *pCurrent=pLayer;
	*pScale=1;
	*pRotation=0;
	*pTranslation=PX_POINT(0,0,0);
	while(pCurrent)
	{
		px_point pivot=pCurrent->currentKeyPoint;
		px_point localTranslation=PX_PointRotate(pCurrent->rel_currentLocalTranslation,pCurrent->rel_currentRotationAngle);
		px_point relative=PX_PointSub(*pTranslation,pivot);
		relative=PX_PointRotate(relative,pCurrent->rel_currentLocalRotationAngle);
		relative=PX_PointMul(relative,pCurrent->rel_currentLocalScale);
		*pTranslation=PX_PointAdd(PX_PointAdd(relative,pivot),localTranslation);
		pTranslation->z=0;
		*pScale*=pCurrent->rel_currentLocalScale;
		*pRotation+=pCurrent->rel_currentLocalRotationAngle;
		pCurrent=PX_LiveFrameworkGetLayerParent(pLive,pCurrent);
	}
}

static px_point PX_LiveFramework_GetLayerVisualKeyPoint(PX_LiveFramework *pLive,PX_LiveLayer *pLayer)
{
	px_float scale,rotation;
	px_point translation,point;
	PX_LiveFramework_GetLayerVisualTransform(pLive,pLayer,&scale,&rotation,&translation);
	point=PX_PointRotate(pLayer->currentKeyPoint,rotation);
	point=PX_PointAdd(PX_PointMul(point,scale),translation);
	point.z=pLayer->currentKeyPoint.z;
	return point;
}

static px_void PX_LiveFramework_UpdateLayerVertices(PX_LiveFramework *pLive,PX_LiveLayer *pLayer,px_dword elapsed)
{
	px_int i;
	px_point2D keyDirection;
	px_int k;
	px_float visualScale,visualRotation;
	px_point visualTranslation;
	px_point stretchU[PX_LIVE_LAYER_MAX_LINK_NODE];
	px_float stretchExtra[PX_LIVE_LAYER_MAX_LINK_NODE];
	px_int stretchCount=0;


	PX_LiveFrameworkUpdateLayerRenderVerticesUV(pLive,pLayer);

	if (pLayer->child_index[0]==-1)
	{
		keyDirection=PX_POINT2D(0,1);
	}
	else
	{
		keyDirection=PX_POINT2D(0,0);
		for (i=0;i<PX_COUNTOF(pLayer->child_index);i++)
		{
			px_point v;
			if (pLayer->child_index[i]==-1)
			{
				break;
			}
			
			v=PX_PointNormalization(PX_PointSub(pLayer->currentKeyPoint,PX_LiveFrameworkGetLayerChild(pLive,pLayer,pLayer->child_index[i])->currentKeyPoint));
			keyDirection=PX_Point2DAdd(keyDirection,PX_POINT2D(v.x,v.y));
		}
		keyDirection=PX_Point2DNormalization(keyDirection);
	}
	PX_LiveFramework_GetLayerVisualTransform(pLive,pLayer,&visualScale,&visualRotation,&visualTranslation);
	for (i=0;i<PX_COUNTOF(pLayer->child_index);i++)
	{
		PX_LiveLayer *pChild=PX_LiveFrameworkGetLayerChild(pLive,pLayer,pLayer->child_index[i]);
		px_point v1;
		px_float v1len2;
		if (!pChild)
		{
			break;
		}
		if (pChild->rel_currentStretch==1)
		{
			continue;
		}
		v1=PX_PointSub(pChild->keyPoint,pLayer->keyPoint);
		v1len2=PX_PointDot(v1,v1);
		if (v1len2<=1.0e-8f)
		{
			continue;
		}
		stretchU[stretchCount]=PX_PointMul(v1,(px_float)(1.0/PX_sqrtd((px_double)v1len2)));
		stretchExtra[stretchCount]=pChild->rel_currentStretch-1.0f;
		stretchCount++;
	}

	//for each vertex
	for (i=0;i<pLayer->vertices.size;i++)
	{
		PX_LiveVertex *plv=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i);
		px_point resultPosition;
		
		//get relative position
		resultPosition.x=plv->sourcePosition.x-pLayer->keyPoint.x;
		resultPosition.y=plv->sourcePosition.y-pLayer->keyPoint.y;
		resultPosition.z=plv->sourcePosition.z-pLayer->keyPoint.z;
		//panc translation
		if (pLayer->panc_currentx!=pLayer->panc_sx|| pLayer->panc_currenty != pLayer->panc_sy)
		{
			if (plv->sourcePosition.x>pLayer->panc_x&& plv->sourcePosition.x < pLayer->panc_x+pLayer->panc_width)
			{
				if (plv->sourcePosition.x < pLayer->panc_sx)
				{
					px_float disx = plv->sourcePosition.x - pLayer->panc_x;
					resultPosition.x += disx * (pLayer->panc_currentx-pLayer->panc_x) /(pLayer->panc_sx- pLayer->panc_x) - disx;
				}
				else
				{
					px_float disx =  (pLayer->panc_x+pLayer->panc_width)- plv->sourcePosition.x;
					resultPosition.x -= disx * (pLayer->panc_x + pLayer->panc_width - pLayer->panc_currentx) / (pLayer->panc_x + pLayer->panc_width - pLayer->panc_sx) - disx;
				}
			}

			if (plv->sourcePosition.y > pLayer->panc_y && plv->sourcePosition.y < pLayer->panc_y + pLayer->panc_height)
			{
				if (plv->sourcePosition.y < pLayer->panc_sy)
				{
					px_float disy = plv->sourcePosition.y - pLayer->panc_y;
					resultPosition.y += disy * (pLayer->panc_currenty - pLayer->panc_y) / (pLayer->panc_sy - pLayer->panc_y) - disy;
				}
				else
				{
					px_float disy = (pLayer->panc_y + pLayer->panc_height) - plv->sourcePosition.y;
					resultPosition.y -= disy * (pLayer->panc_y + pLayer->panc_height - pLayer->panc_currenty) / (pLayer->panc_y + pLayer->panc_height - pLayer->panc_sy) - disy;
				}
			}

			
		}
		
		
		//relative translation
		resultPosition.x+=plv->currentTranslation.x;
		resultPosition.y+=plv->currentTranslation.y;
		resultPosition.z+=plv->currentTranslation.z;

		/* p' = p + u * max(dot(u,p), 0) * (s-1). u is the rest parent-to-child direction. */
		{
			px_int stretchIndex;
			px_float v2len2=PX_PointDot(resultPosition,resultPosition);
			if (v2len2>1.0e-8f)
			{
				for (stretchIndex=0;stretchIndex<stretchCount;stretchIndex++)
				{
					px_float projected=PX_PointDot(stretchU[stretchIndex],resultPosition);
					if (projected>0)
					{
						resultPosition=PX_PointAdd(resultPosition,PX_PointMul(stretchU[stretchIndex],projected*stretchExtra[stretchIndex]));
					}
				}
			}
		}


		//Hierarchy rotation
		resultPosition=PX_PointRotate(resultPosition,pLayer->rel_currentRotationAngle);

		//absolute translation
		resultPosition.x+=pLayer->currentKeyPoint.x;
		resultPosition.y+=pLayer->currentKeyPoint.y;

		/* Apply this layer's visual transform and every inherited ancestor transform. */
		{
			px_float sourceZ=resultPosition.z;
			resultPosition=PX_PointRotate(resultPosition,visualRotation);
			resultPosition=PX_PointAdd(PX_PointMul(resultPosition,visualScale),visualTranslation);
			resultPosition.z=sourceZ;
		}


		//elastic
		k=plv->k;

		if (k==0)
		{
			plv->currentPosition=resultPosition;
		}
		else
		{
			px_point2D direction,direction_normal;
			px_float distance;
			px_dword updateelapsed=elapsed+elapsed/2;
			px_dword atomelapsed;

			while (updateelapsed)
			{
				if (updateelapsed>50)
				{
					atomelapsed=50;
					updateelapsed-=50;
				}
				else
				{
					atomelapsed=updateelapsed;
					updateelapsed=0;
				}
				direction.x=resultPosition.x-plv->currentPosition.x;
				direction.y=resultPosition.y-plv->currentPosition.y;

				direction_normal=PX_Point2DNormalization(direction);
				distance=PX_Point2DMod(direction);


				if (distance>k)
				{
					plv->currentPosition.x=resultPosition.x-direction_normal.x*(k);
					plv->currentPosition.y=resultPosition.y-direction_normal.y*(k);
					distance=k*1.0f;
				}


				do
				{
					px_point2D incv=PX_Point2DMul(direction_normal,distance*distance);
					px_point2D  velocity;
					px_point velocity_vx,velocity_vy;
					px_float _cos,length;
					incv.x+=pLayer->rel_impulse.x;
					incv.y+=pLayer->rel_impulse.y;
					incv=PX_Point2DMul(incv,atomelapsed/1000.f);
					velocity=PX_Point2DAdd(PX_POINT2D(plv->velocity.x,plv->velocity.y),incv);
					if (velocity.x||velocity.y)
					{
						//resistance
						_cos=PX_Point2DDot(velocity,keyDirection)/PX_Point2DMod(velocity)/PX_Point2DMod(keyDirection);
						length=_cos*PX_Point2DMod(velocity);
						velocity_vx.x=length*keyDirection.x;
						velocity_vx.y=length*keyDirection.y;
						velocity_vx.z=0;

						velocity_vy.x=velocity.x-velocity_vx.x;
						velocity_vy.y=velocity.y-velocity_vx.y;
						velocity_vy.z=0;


						velocity_vx=PX_PointMul(velocity_vx,1.0f-atomelapsed/(k*10.f+50));
						velocity_vy=PX_PointMul(velocity_vy,1.0f-atomelapsed/(k*30.f+50));

						if (velocity_vx.x>10000||velocity_vx.y>10000||velocity_vy.x>10000||velocity_vy.y>10000)
						{
							PX_ASSERT();
						}

						plv->velocity=PX_PointAdd(velocity_vx,velocity_vy);
					}
					plv->currentPosition=PX_PointAdd(plv->currentPosition,PX_PointMul(plv->velocity,atomelapsed/1000.f));
										
				}while(0);
			}
			
		}
		
	}
}

static px_void PX_LiveFrameworkUpdatePhysical(PX_LiveFramework *plive,px_dword elapsed,px_bool interpolateTimeline)
{
	px_int i;

	if (interpolateTimeline)
	{
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			PX_LiveFrameworkUpdateLayerInterpolation(plive,pLayer);
		}
	}

	//LayerUpdate
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		if (pLayer->parent_index==-1)
		{
			PX_LiveFramework_UpdateLayerKeyPoint(plive,pLayer);
		}
	}

	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		PX_LiveFramework_UpdateLayerVertices(plive,pLayer,elapsed);
	}
}

static px_bool PX_LiveFrameworkExecuteInstr(PX_LiveFramework *plive,px_int animation_index,px_int frameindex)
{
	//execute instr
	px_int frame_offset=0,frame_size=0;
	px_int layerindex=0;
	PX_LiveAnimation *pAnimation;
	px_byte *pFrameInstrData;
	PX_LiveAnimationFrameHeader *pFrameHeader;
	if (animation_index<0||animation_index>=plive->liveAnimations.size)
	{
		goto _ERROR;
	}
	plive->animationMode=PX_LIVE_MODE_TIMELINE;
	pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,animation_index);
	if (frameindex<0)
	{
		goto _ERROR;
	}
	if (frameindex >= pAnimation->framesMemPtr.size)
	{
		goto _ERROR;
	}
	pFrameInstrData=*PX_VECTORAT(px_byte *,&pAnimation->framesMemPtr,frameindex);
	pFrameHeader=(PX_LiveAnimationFrameHeader *)pFrameInstrData;
	frame_size=pFrameHeader->size+sizeof(PX_LiveAnimationFrameHeader);
	//////////////////////////////////////////////////////////////////////////
	//time stamp update
	plive->reg_duration=pFrameHeader->duration_ms;
	frame_offset+=sizeof(PX_LiveAnimationFrameHeader);
	//////////////////////////////////////////////////////////////////////////
	//payload
	//////////////////////////////////////////////////////////////////////////
	while (frame_offset<frame_size)
	{
		PX_LiveLayer *pLayer;
		PX_LiveAnimationFramePayload *pPayload=(PX_LiveAnimationFramePayload *)(pFrameInstrData+frame_offset);

		pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerindex);
		
		//////////////////////////////////////////////////////////////////////////
		//maptexture
		pLayer->RenderTextureIndex=pPayload->mapTexture;
		
		//////////////////////////////////////////////////////////////////////////
		//rotation register
		pLayer->rel_beginRotationAngle=pLayer->rel_endRotationAngle;
		pLayer->rel_currentRotationAngle=pLayer->rel_beginRotationAngle;
		pLayer->rel_endRotationAngle=pPayload->rotation;

		//////////////////////////////////////////////////////////////////////////
		//stretch register
		pLayer->rel_beginStretch=pLayer->rel_endStretch;
		pLayer->rel_currentStretch=pLayer->rel_beginStretch;
		pLayer->rel_endStretch=pPayload->stretch;

		//////////////////////////////////////////////////////////////////////////
		//translation register
		pLayer->rel_beginTranslation=pLayer->rel_endTranslation;
		pLayer->rel_currentTranslation=pLayer->rel_beginTranslation;
		pLayer->rel_endTranslation=pPayload->translation;

		//////////////////////////////////////////////////////////////////////////
		//selected-layer-only visual transform (stored in the legacy reserve area)
		pLayer->rel_beginLocalTranslation=pLayer->rel_endLocalTranslation;
		pLayer->rel_currentLocalTranslation=pLayer->rel_beginLocalTranslation;
		pLayer->rel_beginLocalRotationAngle=pLayer->rel_endLocalRotationAngle;
		pLayer->rel_currentLocalRotationAngle=pLayer->rel_beginLocalRotationAngle;
		pLayer->rel_beginLocalScale=pLayer->rel_endLocalScale;
		pLayer->rel_currentLocalScale=pLayer->rel_beginLocalScale;
		if (pPayload->localTransformMagic==PX_LIVE_ANIMATION_LOCAL_TRANSFORM_MAGIC)
		{
			pLayer->rel_endLocalTranslation=pPayload->localTranslation;
			pLayer->rel_endLocalRotationAngle=pPayload->localRotation;
			pLayer->rel_endLocalScale=1+pPayload->localScaleOffset;
		}
		else
		{
			pLayer->rel_endLocalTranslation=PX_POINT(0,0,0);
			pLayer->rel_endLocalRotationAngle=0;
			pLayer->rel_endLocalScale=1;
		}

		//////////////////////////////////////////////////////////////////////////
		//impulse
		pLayer->rel_impulse=pPayload->impulse;


		//////////////////////////////////////////////////////////////////////////
		//panc
		pLayer->panc_x = pPayload->panc_x;
		pLayer->panc_y = pPayload->panc_y;

		pLayer->panc_width = pPayload->panc_width;
		pLayer->panc_height = pPayload->panc_height;

		pLayer->panc_sx = pPayload->panc_sx;
		pLayer->panc_sy = pPayload->panc_sy;

		pLayer->panc_beginx = pLayer->panc_endx;
		pLayer->panc_beginy = pLayer->panc_endy;

		pLayer->panc_currentx = pLayer->panc_beginx;
		pLayer->panc_currenty = pLayer->panc_beginy;

		pLayer->panc_endx = pPayload->panc_endx;
		pLayer->panc_endy = pPayload->panc_endy;

		//////////////////////////////////////////////////////////////////////////
		//vertices register
		if (pPayload->translationVerticesCount!=pLayer->vertices.size)
		{
			goto _ERROR;
		}
		else
		{
			px_int j;
			px_point *pVertexTranslation=(px_point *)(pFrameInstrData+frame_offset+sizeof(PX_LiveAnimationFramePayload));
		
			for (j=0;j<(px_int)pPayload->translationVerticesCount;j++)
			{
				PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->beginTranslation=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->endTranslation;
				PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->currentTranslation=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->beginTranslation;
				PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,j)->endTranslation=pVertexTranslation[j];
			}
		}
		frame_offset+=sizeof(PX_LiveAnimationFramePayload);
		frame_offset+=sizeof(px_point)*pPayload->translationVerticesCount;
		layerindex++;
	}
	if (layerindex!=plive->layers.size)
	{
		goto _ERROR;
	}
	return PX_TRUE;
_ERROR:
	return PX_FALSE;
}

static px_void PX_LiveFrameworkUpdateVM(PX_LiveFramework *plive,px_dword elapsed)
{
	if (plive->status==PX_LIVEFRAMEWORK_STATUS_STOP)
	{
		return;
	}

	plive->reg_elapsed+=elapsed;


	while (PX_TRUE)
	{

		if (plive->reg_elapsed>=plive->reg_duration)
		{
			px_dword duration = plive->reg_duration;
			if (!PX_LiveFrameworkExecuteInstr(plive, plive->reg_animation, plive->reg_ip))
			{
				plive->status = PX_LIVEFRAMEWORK_STATUS_STOP;
				goto _ERROR;
			}

			//update time
			plive->reg_elapsed -= duration;

			//update ip
			plive->reg_ip++;
		}
		else
			break;
	}
	return;
_ERROR:
	return;
}

static px_float PX_LiveFrameworkTriangleCross(px_point a,px_point b,px_point c)
{
	return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}

px_void PX_LiveFrameworkCountMeshDefects(const PX_LiveFramework *plive,PX_LiveMeshDefects *defects)
{
	px_int layerIndex;
	if (!defects)
	{
		return;
	}
	PX_memset(defects,0,sizeof(*defects));
	if (!plive)
	{
		return;
	}
	for (layerIndex=0;layerIndex<plive->layers.size;layerIndex++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerIndex);
		px_int triangleIndex;
		px_int vertexIndex;
		for (triangleIndex=0;triangleIndex<layer->triangles.size;triangleIndex++)
		{
			PX_LiveTriangle *triangle=PX_VECTORAT(PX_LiveTriangle,&layer->triangles,triangleIndex);
			PX_LiveVertex *v0,*v1,*v2;
			px_float sourceCross,currentCross;
			if (triangle->index1<0||triangle->index2<0||triangle->index3<0||
				triangle->index1>=layer->vertices.size||triangle->index2>=layer->vertices.size||triangle->index3>=layer->vertices.size)
			{
				defects->degenerate++;
				continue;
			}
			v0=PX_VECTORAT(PX_LiveVertex,&layer->vertices,triangle->index1);
			v1=PX_VECTORAT(PX_LiveVertex,&layer->vertices,triangle->index2);
			v2=PX_VECTORAT(PX_LiveVertex,&layer->vertices,triangle->index3);
			sourceCross=PX_LiveFrameworkTriangleCross(v0->sourcePosition,v1->sourcePosition,v2->sourcePosition);
			currentCross=PX_LiveFrameworkTriangleCross(v0->currentPosition,v1->currentPosition,v2->currentPosition);
			if (PX_ABS(sourceCross)>0.05f&&PX_ABS(currentCross)<0.05f)
			{
				defects->degenerate++;
			}
			else if (sourceCross*currentCross<0&&PX_ABS(sourceCross)>0.05f&&PX_ABS(currentCross)>0.05f)
			{
				defects->flipped++;
			}
		}
		for (vertexIndex=0;vertexIndex<layer->vertices.size;vertexIndex++)
		{
			PX_LiveVertex *vertex=PX_VECTORAT(PX_LiveVertex,&layer->vertices,vertexIndex);
			if (vertex->u<-0.02f||vertex->u>1.02f||vertex->v<-0.02f||vertex->v>1.02f)
			{
				defects->uvOutside++;
			}
		}
	}
}

static px_bool PX_LiveFrameworkHasElasticVertex(PX_LiveFramework *plive)
{
	px_int layerIndex,vertexIndex;
	for (layerIndex=0;layerIndex<plive->layers.size;layerIndex++)
	{
		PX_LiveLayer *layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerIndex);
		for (vertexIndex=0;vertexIndex<layer->vertices.size;vertexIndex++)
		{
			if (PX_VECTORAT(PX_LiveVertex,&layer->vertices,vertexIndex)->k)
			{
				return PX_TRUE;
			}
		}
	}
	return PX_FALSE;
}

px_void PX_LiveFrameworkUpdate(PX_LiveFramework *plive,px_dword elapsed)
{
	if (!plive)
	{
		return;
	}
	switch (plive->animationMode)
	{
	case PX_LIVE_MODE_TIMELINE:
		PX_LiveFrameworkUpdateVM(plive,elapsed);
		PX_LiveFrameworkUpdatePhysical(plive,elapsed,PX_TRUE);
		break;
	case PX_LIVE_MODE_REALTIME30:
		/* Keep the last committed pose when evaluation fails. */
		if (PX_LiveRealtimeUpdate(plive))
		{
			/* Unchanged parameters do not rebuild vertices unless a vertex still has elasticity. */
			if (plive->meshPoseRevision!=plive->realtime.evaluatedRevision||PX_LiveFrameworkHasElasticVertex(plive))
			{
				PX_LiveFrameworkUpdatePhysical(plive,elapsed,PX_FALSE);
				plive->meshPoseRevision=plive->realtime.evaluatedRevision;
			}
		}
		break;
	case PX_LIVE_MODE_NEUTRAL:
	default:
		PX_LiveFrameworkUpdatePhysical(plive,elapsed,PX_FALSE);
		break;
	}
}

static px_void PX_LiveFrameworkRenderLayer(px_surface *psurface,PX_LiveFramework *plive,PX_LiveLayer *pLayer,px_float x,px_float y,px_dword elapsed)
{
	PX_TEXTURERENDER_BLEND blend;
	PX_LiveTexture *pLiveTexture;
	px_float view_scale=plive->view_scale>0?plive->view_scale:1.0f;
	if (!pLayer->visible)
	{
		return;
	}
	pLiveTexture=PX_LiveFrameworkGetLiveTexture(plive,pLayer->RenderTextureIndex);
	if (pLiveTexture)
	{
		px_texture *pTexture;		
		pTexture=&pLiveTexture->Texture;

		if (pLayer->triangles.size&&pLayer->vertices.size)
		{
			px_int t;
			PX_Delaunay_Triangle *pTriangleIndex;
			for (t=0;t<pLayer->triangles.size;t++)
			{
				PX_LiveRenderVertex v0,v1,v2;
				PX_LiveVertex *pv0,*pv1,*pv2;
				pTriangleIndex=PX_VECTORAT(PX_Delaunay_Triangle,&pLayer->triangles,t);
				pv0=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index1);
				pv1=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index2);
				pv2=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index3);
				v0.position.x=pv0->currentPosition.x*view_scale+x;
				v0.position.y=pv0->currentPosition.y*view_scale+y;
				v0.position.z=1;
				v0.normal=pv0->normal;
				v0.u=pv0->u;
				v0.v=pv0->v;

				v1.position.x=pv1->currentPosition.x*view_scale+x;
				v1.position.y=pv1->currentPosition.y*view_scale+y;
				v1.position.z=1;
				v1.normal=pv1->normal;
				v1.u=pv1->u;
				v1.v=pv1->v;

				v2.position.x=pv2->currentPosition.x*view_scale+x;
				v2.position.y=pv2->currentPosition.y*view_scale+y;
				v2.position.z=1;
				v2.normal=pv2->normal;
				v2.u=pv2->u;
				v2.v=pv2->v;

				if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
				{
					if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex)||!plive->showFocusLayer)
					{
						PX_LiveFramework_RenderListRasterization(psurface,plive,v0,v1,v2,pTexture,PX_NULL);
					}
					else
					{
						blend.alpha=0.2f;
						blend.hdr_B=1;
						blend.hdr_G=1;
						blend.hdr_R=1;
						PX_LiveFramework_RenderListRasterization(psurface,plive,v0,v1,v2,pTexture,&blend);
					}
				}
				else
				{
					PX_LiveFramework_RenderListRasterization(psurface,plive,v0,v1,v2,pTexture,PX_NULL);
				}
				
			}

			if (pLayer->showMesh)
			{
				for (t=0;t<pLayer->triangles.size;t++)
				{
					PX_LiveRenderVertex v0,v1,v2;
					PX_LiveVertex *pv0,*pv1,*pv2;
					pTriangleIndex=PX_VECTORAT(PX_Delaunay_Triangle,&pLayer->triangles,t);
					pv0=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index1);
					pv1=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index2);
					pv2=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,pTriangleIndex->index3);
					v0.position.x=pv0->currentPosition.x*view_scale+x;
					v0.position.y=pv0->currentPosition.y*view_scale+y;
					v0.normal=pv0->normal;
					v0.u=pv0->u;
					v0.v=pv0->v;

					v1.position.x=pv1->currentPosition.x*view_scale+x;
					v1.position.y=pv1->currentPosition.y*view_scale+y;
					v1.normal=pv1->normal;
					v1.u=pv1->u;
					v1.v=pv1->v;

					v2.position.x=pv2->currentPosition.x*view_scale+x;
					v2.position.y=pv2->currentPosition.y*view_scale+y;
					v2.normal=pv2->normal;
					v2.u=pv2->u;
					v2.v=pv2->v;


					PX_GeoDrawLine(psurface,(px_int)v0.position.x,(px_int)v0.position.y,(px_int)v1.position.x,(px_int)v1.position.y,1,PX_COLOR(128,255,0,0));
					PX_GeoDrawLine(psurface,(px_int)v0.position.x,(px_int)v0.position.y,(px_int)v2.position.x,(px_int)v2.position.y,1,PX_COLOR(128,255,0,0));
					PX_GeoDrawLine(psurface,(px_int)v1.position.x,(px_int)v1.position.y,(px_int)v2.position.x,(px_int)v2.position.y,1,PX_COLOR(128,255,0,0));

					PX_GeoDrawSolidCircle(psurface,(px_int)v0.position.x,(px_int)v0.position.y,3,PX_COLOR(128,255,0,0));
					PX_GeoDrawSolidCircle(psurface,(px_int)v1.position.x,(px_int)v1.position.y,3,PX_COLOR(128,255,0,0));
					PX_GeoDrawSolidCircle(psurface,(px_int)v2.position.x,(px_int)v2.position.y,3,PX_COLOR(128,255,0,0));
				}

				if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
				{
					if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex))
					{
						if (plive->currentEditVertexIndex>=0&&plive->currentEditVertexIndex<pLayer->vertices.size)
						{
							PX_LiveVertex *pLiveVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,plive->currentEditVertexIndex);
							PX_GeoDrawCircle(psurface,(px_int)(pLiveVertex->currentPosition.x*view_scale+x),(px_int)(pLiveVertex->currentPosition.y*view_scale+y),5,1,PX_COLOR(255,255,128,0));
						}
					}
				}

			}
		}
		else
		{
			if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
			{
				if (plive->showFocusLayer)
				{
					if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex))
					{
						PX_TextureRenderEx(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX*view_scale),
							(px_int)(y+pLiveTexture->textureOffsetY*view_scale),PX_ALIGN_LEFTTOP,PX_NULL,view_scale,0);
					}
					else
					{
						blend.alpha=0.2f;
						blend.hdr_B=1;
						blend.hdr_G=1;
						blend.hdr_R=1;
						PX_TextureRenderEx(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX*view_scale),
							(px_int)(y+pLiveTexture->textureOffsetY*view_scale),PX_ALIGN_LEFTTOP,&blend,view_scale,0);
					}
				}
				else
				{
					PX_TextureRenderEx(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX*view_scale),
						(px_int)(y+pLiveTexture->textureOffsetY*view_scale),PX_ALIGN_LEFTTOP,PX_NULL,view_scale,0);
				}
				
			}
			else
			{
				PX_TextureRenderEx(psurface,pTexture,(px_int)(x+pLiveTexture->textureOffsetX*view_scale),
					(px_int)(y+pLiveTexture->textureOffsetY*view_scale),PX_ALIGN_LEFTTOP,PX_NULL,view_scale,0);

			}
		}
	
	}
}

px_void PX_LiveFrameworkRenderCurrent(px_surface *psurface,PX_LiveFramework *plive,px_float x,px_float y,PX_ALIGN refPoint)
{
	PX_QuickSortAtom sAtom[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER];
	px_int i,count;
	px_float view_scale=plive->view_scale>0?plive->view_scale:1.0f;

	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=plive->width*view_scale/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=plive->width*view_scale;
		break;
	case PX_ALIGN_LEFTMID:
		y-=plive->height*view_scale/2;
		break;
	case PX_ALIGN_CENTER:
		y-=plive->height*view_scale/2;
		x-=plive->width*view_scale/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=plive->height*view_scale/2;
		x-=plive->width*view_scale;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=plive->height*view_scale;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=plive->height*view_scale;
		x-=plive->width*view_scale/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=plive->height*view_scale;
		x-=plive->width*view_scale;
		break;
	}


	if (plive->layers.size)
	{
		count=plive->layers.size;
		if (count>PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
		{
			count=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER;
		}
		for (i=0;i<count;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			sAtom[i].weight=pLayer->currentKeyPoint.z;
			sAtom[i].pData=pLayer;
		}
		PX_Quicksort_ArrayMaxToMin(sAtom,0,count-1);

		for (i=0;i<count;i++)
		{
			PX_LiveLayer *pLayer=(PX_LiveLayer *)sAtom[i].pData;
			PX_LiveFrameworkRenderLayer(psurface,plive,pLayer,x,y,0);
		}
	}
	
	if (plive->showKeypoint&&!plive->showlinker)
	{
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			px_point visualKeyPoint=PX_LiveFramework_GetLayerVisualKeyPoint(plive,pLayer);
			if (pLayer->parent_index!=-1)
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),3,PX_COLOR(255,0,64,192));
				PX_GeoDrawCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),5,1,PX_COLOR(255,0,64,192));
			}
			else
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),3,PX_COLOR(255,255,0,0));
				PX_GeoDrawCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),5,1,PX_COLOR(255,255,0,0));
			}
			
		}
	}

	if (plive->showlinker)
	{
		for (i=0;i<plive->layers.size;i++)
		{
			px_int j;
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			px_point visualKeyPoint=PX_LiveFramework_GetLayerVisualKeyPoint(plive,pLayer);

			if (pLayer->parent_index!=-1)
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),3,PX_COLOR(255,0,64,192));
				PX_GeoDrawCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),5,1,PX_COLOR(255,0,64,192));
			}
			else
			{
				PX_GeoDrawSolidCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),3,PX_COLOR(255,255,0,0));
				PX_GeoDrawCircle(psurface,(px_int)(visualKeyPoint.x*view_scale+x),(px_int)(visualKeyPoint.y*view_scale+y),5,1,PX_COLOR(255,255,0,0));
			}

			for (j=0;j<PX_LIVE_LAYER_MAX_LINK_NODE;j++)
			{
				
				PX_LiveLayer *pLinkLayer=PX_LiveFrameworkGetLayerChild(plive,pLayer,pLayer->child_index[j]);

				if (!pLinkLayer)
				{
					break;
				}

				{
					px_point linkVisualKeyPoint=PX_LiveFramework_GetLayerVisualKeyPoint(plive,pLinkLayer);
					PX_GeoDrawArrow(psurface,
						PX_POINT2D(visualKeyPoint.x*view_scale+x,visualKeyPoint.y*view_scale+y),
						PX_POINT2D(linkVisualKeyPoint.x*view_scale+x,linkVisualKeyPoint.y*view_scale+y),
						1,
						PX_COLOR(255,255,0,0)
						);
				}

			}
		}
	}


	if (plive->showRange)
	{
		PX_GeoDrawBorder(psurface,(px_int)x,(px_int)y,(px_int)(x+plive->width*view_scale),(px_int)(y+plive->height*view_scale),1,PX_COLOR(255,0,0,255));
	}

	if (plive->showRootHelperLine)
	{
		PX_LiveLayer *pLayer=PX_LiveFrameworkGetCurrentEditLiveLayer(plive);
		if (pLayer&&pLayer->parent_index==-1)
		{
			px_point visualKeyPoint=PX_LiveFramework_GetLayerVisualKeyPoint(plive,pLayer);
			PX_GeoDrawLine(psurface,(px_int)(x+pLayer->keyPoint.x*view_scale),(px_int)(y+pLayer->keyPoint.y*view_scale),(px_int)(x+visualKeyPoint.x*view_scale),(px_int)(y+visualKeyPoint.y*view_scale),1,PX_COLOR(255,255,0,255));
		}
	}

}

px_void PX_LiveFrameworkRender(px_surface *psurface,PX_LiveFramework *plive,px_float x,px_float y,PX_ALIGN refPoint,px_dword elapsed)
{
	PX_LiveFrameworkUpdate(plive,elapsed);
	PX_LiveFrameworkRenderCurrent(psurface,plive,x,y,refPoint);
}

px_void PX_LiveFrameworkRenderRefer(px_surface *psurface,PX_LiveFramework *plive,PX_ALIGN refPoint,px_dword elapsed)
{
	PX_LiveFrameworkRender(psurface,plive,plive->refer_x,plive->refer_y,refPoint,elapsed);
}



px_bool PX_LiveFrameworkPlayAnimation(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->liveAnimations.size)
	{
		if (plive->animationMode==PX_LIVE_MODE_REALTIME30)
		{
			PX_LiveRealtimeLeave(plive);
		}
		plive->animationMode=PX_LIVE_MODE_TIMELINE;
		plive->currentEditFrameIndex=-1;
		plive->currentEditLayerIndex=-1;
		plive->currentEditVertexIndex=-1;
		plive->reg_animation=index;
		plive->reg_ip = 0;
		plive->reg_elapsed=0;
		plive->reg_duration=0;
		plive->status=PX_LIVEFRAMEWORK_STATUS_PLAYING;
		return PX_TRUE;
	}
	return PX_FALSE;
}

px_bool PX_LiveFrameworkPlayAnimationByName(PX_LiveFramework *plive,const px_char name[])
{
	px_int i;
	for (i=0;i<plive->liveAnimations.size;i++)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
		if (PX_strequ2(name,pAnimation->id))
		{
			PX_LiveFrameworkPlayAnimation(plive,i);
			return PX_TRUE;
		}
	}
	return PX_FALSE;
}

PX_LiveLayer * PX_LiveFrameworkGetLayerById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_LiveFrameworkGetLayer(plive,i);
		if (pLayer&&PX_memequ(pLayer->id,id,PX_LIVE_ID_MAX_LEN))
		{
			return pLayer;
		}
	}
	return PX_NULL;
}

PX_LiveLayer * PX_LiveFrameworkCreateLayer(PX_LiveFramework *plive,const px_char id[PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER])
{
	PX_LiveLayer layer;
	px_int i;

	if (plive->layers.size>=PX_LIVEFRAMEWORK_MAX_SUPPORT_LAYER)
	{
		return PX_NULL;
	}

	if (PX_LiveFrameworkGetLayerById(plive,id))
	{
		return PX_NULL;
	}


	PX_memset(&layer,0,sizeof(layer));

	if(!PX_VectorInitialize(plive->mp,&layer.triangles,sizeof(PX_Delaunay_Triangle),0))return PX_FALSE;
	if(!PX_VectorInitialize(plive->mp,&layer.vertices,sizeof(PX_LiveVertex),0))return PX_FALSE;


	layer.rotationAngle=0;
	layer.visible=PX_TRUE;
	layer.keyPoint.z=1;

	layer.rel_beginStretch=1;
	layer.rel_currentStretch=1;
	layer.rel_endStretch=1;
	layer.rel_beginLocalScale=1;
	layer.rel_currentLocalScale=1;
	layer.rel_endLocalScale=1;

	layer.RenderTextureIndex = -1;
	layer.LinkTextureIndex = -1;

	layer.parent_index=-1;
	for (i=0;i<PX_COUNTOF(layer.child_index);i++)
	{
		layer.child_index[i]=-1;
	}

	PX_strcpy(layer.id,id,sizeof(layer.id));

	if(!PX_VectorPushback(&plive->layers,&layer))
		return PX_FALSE;

	return PX_VECTORLAST(PX_LiveLayer,&plive->layers);

}

PX_LiveLayer * PX_LiveFrameworkGetLayerParent(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	if (pLayer->parent_index>=0&&pLayer->parent_index<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,pLayer->parent_index);
	}
	
#ifdef PX_DEBUG_MODE
	if (pLayer->parent_index!=-1)
	{
		//Data Crash
		PX_ASSERT();
	}
#endif
	
	return PX_NULL;
}

PX_LiveLayer * PX_LiveFrameworkGetLayerChild(PX_LiveFramework *plive,PX_LiveLayer *pLayer,px_int childIndex)
{
	if (childIndex>=0&&childIndex<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,childIndex);
	}

	if (childIndex!=-1)
	{
		PX_ASSERT();
	}
	
	return PX_NULL;
}

px_bool PX_LiveFrameworkLinkLayerTexture(PX_LiveFramework *plive,const px_char layer_id[],const px_char texture_id[])
{
	px_int index=PX_LiveFrameworkGetLiveTextureIndexById(plive,texture_id);
	PX_LiveTexture *pLiveTexture=PX_LiveFrameworkGetLiveTextureById(plive,texture_id);
	PX_LiveLayer *pLayer=PX_LiveFrameworkGetLayerById(plive,layer_id);
	if (pLiveTexture&&pLayer)
	{
		pLayer->LinkTextureIndex=index;
		pLayer->RenderTextureIndex=index;
		pLayer->keyPoint.x=pLiveTexture->textureOffsetX+pLiveTexture->Texture.width/2.0f;
		pLayer->keyPoint.y=pLiveTexture->textureOffsetY+pLiveTexture->Texture.height/2.0f;
		pLayer->rel_currentTranslation=PX_POINT(0,0,0);
		return PX_TRUE;
	}
	return PX_FALSE;
}

PX_LiveLayer * PX_LiveFrameworkGetLayer(PX_LiveFramework *plive,px_int i)
{
	if (i>=0&&i<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
	}
	return PX_NULL;
}

px_int PX_LiveFrameworkGetLayerIndex(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		if (pLayer==PX_VECTORAT(PX_LiveLayer,&plive->layers,i))
		{
			return i;
		}
	}
	return -1;
}

PX_LiveLayer *PX_LiveFrameworkGetLastCreateLayer(PX_LiveFramework *plive)
{
	if (plive->layers.size)
	{
		return PX_VECTORLAST(PX_LiveLayer,&plive->layers);
	}
	return PX_NULL;
}

px_void PX_LiveFrameworkUpdateLayerSourceVerticesUV(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_int i;
	for (i=0;i<pLayer->vertices.size;i++)
	{
		PX_LiveVertex *pVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i);
		
		if (pLayer->LinkTextureIndex>=0&&pLayer->LinkTextureIndex<plive->livetextures.size)
		{
			PX_LiveTexture *pLiveTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,pLayer->LinkTextureIndex);
			pVertex->u=(pVertex->sourcePosition.x-pLiveTexture->textureOffsetX)*1.0f/pLiveTexture->Texture.width;
			pVertex->v=(pVertex->sourcePosition.y-pLiveTexture->textureOffsetY)*1.0f/pLiveTexture->Texture.height;
		}
	}
}

px_void PX_LiveFrameworkUpdateSourceVerticesUV(PX_LiveFramework *plive)
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		PX_LiveFrameworkUpdateLayerSourceVerticesUV(plive,pLayer);
	}
}

px_void PX_LiveFrameworkUpdateLayerRenderVerticesUV(PX_LiveFramework *plive,PX_LiveLayer *pLayer)
{
	px_int i;
	for (i=0;i<pLayer->vertices.size;i++)
	{
		PX_LiveVertex *pVertex=PX_VECTORAT(PX_LiveVertex,&pLayer->vertices,i);

		if (pLayer->RenderTextureIndex>=0&&pLayer->RenderTextureIndex<plive->livetextures.size)
		{
			PX_LiveTexture *pLiveTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,pLayer->RenderTextureIndex);
			pVertex->u=(pVertex->sourcePosition.x-pLiveTexture->textureOffsetX)*1.0f/pLiveTexture->Texture.width;
			pVertex->v=(pVertex->sourcePosition.y-pLiveTexture->textureOffsetY)*1.0f/pLiveTexture->Texture.height;
		}
	}
}

PX_LiveAnimation * PX_LiveFrameworkGetAnimationById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->liveAnimations.size;i++)
	{
		PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetAnimation(plive,i);
		if (pAnimation&&PX_strequ(pAnimation->id,id))
		{
			return pAnimation;
		}
	}
	return PX_NULL;

}

PX_LiveAnimation * PX_LiveFrameworkCreateAnimation(PX_LiveFramework *plive,const px_char id[])
{
	PX_LiveAnimation animation;
	PX_memset(&animation,0,sizeof(animation));
	PX_VectorInitialize(plive->mp,&animation.framesMemPtr,sizeof(px_void *),1);
	PX_strcpy(animation.id,id,sizeof(animation.id));
	if(!PX_VectorPushback(&plive->liveAnimations,&animation))return PX_NULL;
	return PX_VECTORLAST(PX_LiveAnimation,&plive->liveAnimations);
}

PX_LiveAnimation * PX_LiveFrameworkGetAnimation(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->liveAnimations.size)
	{
		return PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,index);
	}
	return PX_NULL;
}

PX_LiveAnimation * PX_LiveFrameworkGetLastCreateAnimation(PX_LiveFramework *plive)
{
	if (plive->liveAnimations.size)
	{
		return PX_VECTORLAST(PX_LiveAnimation,&plive->liveAnimations);
	}
	return PX_NULL;
}

px_bool PX_LiveFrameworkAddLiveTexture(PX_LiveFramework *plive,PX_LiveTexture livetexture)
{
	return PX_VectorPushback(&plive->livetextures,&livetexture);
}

PX_LiveTexture * PX_LiveFrameworkGetLiveTextureById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (PX_strequ(id,pTexture->id))
		{
			return pTexture;
		}
	}
	return PX_NULL;
}

px_int PX_LiveFrameworkGetLiveTextureIndexById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (PX_strequ(id,pTexture->id))
		{
			return i;
		}
	}
	return -1;
}

px_int PX_LiveFrameworkGetLiveTextureIndex(PX_LiveFramework *plive,PX_LiveTexture *pCompareTexture)
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (pTexture==pCompareTexture)
		{
			return i;
		}
	}
	return -1;
}

PX_LiveTexture * PX_LiveFrameworkGetLiveTexture(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->livetextures.size)
	{
		return PX_VECTORAT(PX_LiveTexture,&plive->livetextures,index);
	}
	return PX_NULL;
}

px_void PX_LiveFrameworkDeleteLiveTextureById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->livetextures.size;i++)
	{
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
		if (PX_strequ(id,pTexture->id))
		{
			PX_LiveFrameworkDeleteLiveTexture(plive,i);
			return;
		}
	}
}

px_void PX_LiveFrameworkDeleteLiveAnimationById(PX_LiveFramework *plive,const px_char id[])
{
	px_int i;
	for (i=0;i<plive->liveAnimations.size;i++)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
		if (PX_strequ(id,pAnimation->id))
		{
			PX_LiveFrameworkDeleteLiveAnimation(plive,i);
			return;
		}
	}
}

px_bool PX_LiveFrameworkLinkLayerSearchSubLayer(PX_LiveFramework *plive,PX_LiveLayer *pLayer,PX_LiveLayer *pSearchLayer)
{
	px_int i;
	for (i=0;i<PX_LIVE_LAYER_MAX_LINK_NODE;i++)
	{
		PX_LiveLayer *pSubLinkLayer=PX_LiveFrameworkGetLayerChild(plive,pLayer,pLayer->child_index[i]);

		if (pSubLinkLayer==PX_NULL)
		{
			return PX_FALSE;
		}

		if (pSubLinkLayer==pSearchLayer)
		{
			return PX_TRUE;
		}
		else
		{
			if(pSubLinkLayer!=PX_NULL)
			return PX_LiveFrameworkLinkLayerSearchSubLayer(plive,pSubLinkLayer,pSearchLayer);
		}
	}
	return PX_FALSE;
}

px_void PX_LiveFrameworkLinkLayer(PX_LiveFramework *plive,PX_LiveLayer *pLayer,PX_LiveLayer *linkLayer)
{
	px_int i;
	if (pLayer==linkLayer)
	{
		return;
	}

	if (PX_LiveFrameworkLinkLayerSearchSubLayer(plive,pLayer,pLayer))
	{
		return;
	}

	for (i=0;i<PX_LIVE_LAYER_MAX_LINK_NODE;i++)
	{
		PX_LiveLayer *pSubLinkLayer=PX_LiveFrameworkGetLayerChild(plive,pLayer,pLayer->child_index[i]);

		if (!pSubLinkLayer)
		{
			break;
		}

		if (pSubLinkLayer==linkLayer)
		{
			return;
		}

	}
	if(i<PX_LIVE_LAYER_MAX_LINK_NODE)
	{
		if (linkLayer->parent_index==-1)
		{
			pLayer->child_index[i]=PX_LiveFrameworkGetLayerIndex(plive,linkLayer);
			linkLayer->parent_index=PX_LiveFrameworkGetLayerIndex(plive,pLayer);
		}
	}
}

px_void PX_LiveFrameworkClearLinker(PX_LiveFramework *plive)
{
	px_int i;
	for (i=0;i<plive->layers.size;i++)
	{
		px_int j;
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
		for (j=0;j<PX_COUNTOF(pLayer->child_index);j++)
		{
			pLayer->child_index[j]=-1;
		}
		pLayer->parent_index=-1;
	}
}

px_void PX_LiveFrameworkDeleteLayer(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->layers.size)
	{
		px_int i,j;
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,index);
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pSearchLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);

			if (pSearchLayer->parent_index == index)
			{
				pSearchLayer->parent_index = -1;
			}
			else if (pSearchLayer->parent_index > index)
			{
				pSearchLayer->parent_index--;
			}

			for (j=0;j<PX_LIVE_LAYER_MAX_LINK_NODE;j++)
			{
				if (pSearchLayer->child_index[j]==index)
				{
					px_int k;
					for (k=j;k<PX_COUNTOF(pLayer->child_index);k++)
					{
						pSearchLayer->child_index[k]=pSearchLayer->child_index[k+1];
						if (pSearchLayer->child_index[k]==-1)
						{
							break;
						}
					}
				}
				
				if (pSearchLayer->child_index[j] > index)
				{
					pSearchLayer->child_index[j]--;
				}
			}
		}

		PX_VectorFree(&pLayer->vertices);
		PX_VectorFree(&pLayer->triangles);
		PX_VectorErase(&plive->layers,index);
	}
}

px_void PX_LiveFrameworkDeleteLiveTexture(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->livetextures.size)
	{
		px_int i;
		PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,index);
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *player=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			if (player->LinkTextureIndex>=index)
			{
				if (player->LinkTextureIndex>index)
				{
					player->LinkTextureIndex--;
					player->RenderTextureIndex=player->LinkTextureIndex;
				}
				else
				{
					player->LinkTextureIndex=-1;
					player->RenderTextureIndex=player->LinkTextureIndex;
				}
				
			}
		}
		PX_TextureFree(&pTexture->Texture);
		PX_VectorErase(&plive->livetextures,index);
	}
}

px_void PX_LiveFrameworkDeleteLiveAnimationFrame(PX_LiveFramework *plive,PX_LiveAnimation *pliveAnimation,px_int index)
{
	if (index>=0&&index<pliveAnimation->framesMemPtr.size)
	{
		px_void *pFrameMemories=*PX_VECTORAT(px_void*,&pliveAnimation->framesMemPtr,index);
		if(pFrameMemories)
			MP_Free(plive->mp,pFrameMemories);
		PX_VectorErase(&pliveAnimation->framesMemPtr,index);
	}
}

px_void PX_LiveFrameworkDeleteLiveAnimation(PX_LiveFramework *plive,px_int index)
{
	if (index>=0&&index<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,index);
		while (pAnimation->framesMemPtr.size)
		{
			PX_LiveFrameworkDeleteLiveAnimationFrame(plive,pAnimation,0);
		}
		PX_VectorFree(&pAnimation->framesMemPtr);
		PX_VectorErase(&plive->liveAnimations,index);

		plive->currentEditAnimationIndex=-1;
		plive->currentEditFrameIndex=-1;
		plive->currentEditLayerIndex=-1;
		plive->currentEditVertexIndex=1;
	}
}

px_void PX_LiveFrameworkDeleteLiveAnimationFrameByIndex(PX_LiveFramework *plive,px_int AnimationIndex,px_int frameIndex)
{
	if (AnimationIndex>=0&&AnimationIndex<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,AnimationIndex);
		if (frameIndex>=0&&frameIndex<pAnimation->framesMemPtr.size)
		{
			PX_LiveFrameworkDeleteLiveAnimationFrame(plive,pAnimation,frameIndex);
		}
		PX_VectorErase(&pAnimation->framesMemPtr,frameIndex);
	}
}

px_void PX_LiveFrameworkFree(PX_LiveFramework *plive)
{
	PX_LiveRealtimeFree(&plive->realtime);
	while (plive->liveAnimations.size)
	{
		PX_LiveFrameworkDeleteLiveAnimation(plive,0);
	}
	while (plive->layers.size)
	{
		PX_LiveFrameworkDeleteLayer(plive,0);
	}
	while(plive->livetextures.size)
	{
		PX_LiveFrameworkDeleteLiveTexture(plive,0);
	}
	PX_VectorFree(&plive->liveAnimations);
	PX_VectorFree(&plive->layers);
	PX_VectorFree(&plive->livetextures);
}

px_void * PX_LiveFrameworkGetCurrentEditFrame(PX_LiveFramework *plive)
{
	if (plive->currentEditAnimationIndex>=0&&plive->currentEditAnimationIndex<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,plive->currentEditAnimationIndex);
		if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
		{
			return *PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex);
		}
	}
	return PX_NULL;
}

PX_LiveLayer * PX_LiveFrameworkGetCurrentEditLiveLayer(PX_LiveFramework *plive)
{
	if (plive->currentEditLayerIndex>=0&&plive->currentEditLayerIndex<plive->layers.size)
	{
		return PX_VECTORAT(PX_LiveLayer,&plive->layers,plive->currentEditLayerIndex);
	}
	return PX_NULL;
}

PX_LiveAnimationFramePayload *PX_LiveFrameworkGetCurrentEditAnimationFramePayloadIndex(PX_LiveFramework *plive,px_int payloadIndex)
{
	px_int boffset=0;
	px_int FramePayloadSize;
	px_byte *pOffset;
	PX_LiveAnimationFrameHeader *pHeader;
	pOffset=(px_byte *)PX_LiveFrameworkGetCurrentEditFrame(plive);
	if (!pOffset)
	{
		return PX_NULL;
	}

	pHeader=(PX_LiveAnimationFrameHeader *)pOffset;
	FramePayloadSize=pHeader->size+sizeof(PX_LiveAnimationFrameHeader);
	boffset+=sizeof(PX_LiveAnimationFrameHeader);
	while (PX_TRUE)
	{
		PX_LiveAnimationFramePayload *pPayloadHeder;
		if (boffset>FramePayloadSize)
		{
			break;
		}
		pPayloadHeder=(PX_LiveAnimationFramePayload *)(pOffset+boffset);
		if (payloadIndex<=0)
		{
			return pPayloadHeder;
		}
		boffset+=sizeof(PX_LiveAnimationFramePayload);
		boffset+=pPayloadHeder->translationVerticesCount*sizeof(px_point);
		payloadIndex--;
	}
	return PX_NULL;
}

PX_LiveAnimationFramePayload * PX_LiveFrameworkGetCurrentEditAnimationFramePayload(PX_LiveFramework *plive)
{
	return PX_LiveFrameworkGetCurrentEditAnimationFramePayloadIndex(plive,plive->currentEditLayerIndex);
}

px_void PX_LiveFrameworkCurrentEditMoveFrameDown(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (!pAnimation)
	{
		return;
	}
	if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
	{
		if (plive->currentEditFrameIndex==pAnimation->framesMemPtr.size-1)
		{
			return;
		}
		else
		{
			px_void **ptr1,**ptr2,*temp;
			ptr1=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex);
			ptr2=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex+1);
			temp=*ptr1;
			*ptr1=*ptr2;
			*ptr2=temp;
		}
	}
}

px_void PX_LiveFrameworkCurrentEditMoveFrameUp(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (!pAnimation)
	{
		return;
	}
	if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
	{
		if (plive->currentEditFrameIndex==0)
		{
			return;
		}
		else
		{
			px_void **ptr1,**ptr2,*temp;
			ptr1=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex);
			ptr2=PX_VECTORAT(px_void *,&pAnimation->framesMemPtr,plive->currentEditFrameIndex-1);
			temp=*ptr1;
			*ptr1=*ptr2;
			*ptr2=temp;
		}
	}
}

PX_LiveAnimation * PX_LiveFrameworkGetCurrentEditAnimation(PX_LiveFramework *plive)
{
	if (plive->currentEditAnimationIndex>=0&&plive->currentEditAnimationIndex<plive->liveAnimations.size)
	{
		PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,plive->currentEditAnimationIndex);
		return pAnimation;
	}
	return PX_NULL;
}

PX_LiveAnimationFrameHeader * PX_LiveFrameworkGetCurrentEditAnimationFrame(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (pAnimation)
	{
		if (plive->currentEditFrameIndex>=0&&plive->currentEditFrameIndex<pAnimation->framesMemPtr.size)
		{
			return (PX_LiveAnimationFrameHeader *)(*PX_VECTORAT(px_void*,&pAnimation->framesMemPtr,plive->currentEditFrameIndex));
		}
	}
	return PX_NULL;
}

PX_LiveVertex * PX_LiveFrameworkGetCurrentEditLiveVertex(PX_LiveFramework *plive)
{
	PX_LiveLayer *currentEditLayer=PX_LiveFrameworkGetCurrentEditLiveLayer(plive);
	if (currentEditLayer)
	{
		if (plive->currentEditVertexIndex>=0&&plive->currentEditVertexIndex<currentEditLayer->vertices.size)
		{
			return PX_VECTORAT(PX_LiveVertex,&currentEditLayer->vertices,plive->currentEditVertexIndex);
		}
	}
	return PX_NULL;
}

px_point * PX_LiveFrameworkGetCurrentEditAnimationFramePayloadVertex(PX_LiveFramework *plive)
{
	px_byte *pPayloadByte=(px_byte *)PX_LiveFrameworkGetCurrentEditAnimationFramePayload(plive);
	PX_LiveAnimationFramePayload *ppayload=(PX_LiveAnimationFramePayload *)pPayloadByte;
	if (pPayloadByte)
	{
		if (plive->currentEditVertexIndex>=0&&plive->currentEditVertexIndex<(px_int)ppayload->translationVerticesCount)
		{
			return (px_point *)(pPayloadByte+sizeof(PX_LiveAnimationFramePayload)+plive->currentEditVertexIndex*sizeof(px_point));
		}
	}
	return PX_NULL;
}

px_void PX_LiveFrameworkDeleteCurrentEditAnimation(PX_LiveFramework *plive)
{
	PX_LiveFrameworkDeleteLiveAnimation(plive,plive->currentEditAnimationIndex);
}

px_void PX_LiveFrameworkDeleteCurrentEditAnimationFrame(PX_LiveFramework *plive)
{
	PX_LiveAnimation *pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (pAnimation)
	{
		PX_LiveFrameworkDeleteLiveAnimationFrame(plive,pAnimation,plive->currentEditFrameIndex);
	}
	
}

px_bool PX_LiveFrameworkNewEditFrame(PX_LiveFramework *plive,px_char id[],px_bool bCopyFrame)
{
	PX_LiveAnimation *pAnimation;
	px_void *pFramebuffer;
	pAnimation=PX_LiveFrameworkGetCurrentEditAnimation(plive);
	if (!pAnimation||!id||!id[0])
	{
		return PX_FALSE;
	}
	pFramebuffer=PX_LiveFrameworkGetCurrentEditAnimationFrame(plive);

	if (pFramebuffer)
	{
		px_uint size;
		PX_LiveAnimationFrameHeader *pHeader;
		px_void *newFramePtr;
		px_void *buffer=pFramebuffer;
		pHeader=(PX_LiveAnimationFrameHeader *)buffer;
		size=pHeader->size+sizeof(PX_LiveAnimationFrameHeader);
		newFramePtr=MP_Malloc(plive->mp,size);
		if (newFramePtr)
		{
			PX_memcpy(newFramePtr,buffer,size);
			pHeader=(PX_LiveAnimationFrameHeader *)newFramePtr;
			if (!id||!id[0])
			{
				MP_Free(plive->mp,newFramePtr);
				return PX_FALSE;
			}
			PX_memcpy(pHeader->frameid,id,sizeof(pHeader->frameid));
			if (bCopyFrame)
			{
				if (!PX_VectorPushTo(&pAnimation->framesMemPtr,&newFramePtr,plive->currentEditFrameIndex+1))
				{
					MP_Free(plive->mp,newFramePtr);
					return PX_FALSE;
				}
			}
			else if (!PX_VectorPushback(&pAnimation->framesMemPtr,&newFramePtr))
			{
				MP_Free(plive->mp,newFramePtr);
				return PX_FALSE;
			}
			return PX_TRUE;
		}
		else
		{
			return PX_FALSE;
		}
	}
	else
	{
		px_void *newFramePtr;
		//create new frame
		px_int i;
		px_uint size=0;
		//header
		size+=sizeof(PX_LiveAnimationFrameHeader);
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *player=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			//payload
			size+=sizeof(PX_LiveAnimationFramePayload);
			//translation
			size+=sizeof(px_point)*player->vertices.size;
		}
		newFramePtr=MP_Malloc(plive->mp,size);

		if (newFramePtr)
		{
			PX_LiveAnimationFrameHeader *pHeader=(PX_LiveAnimationFrameHeader *)newFramePtr;
			PX_LiveAnimationFramePayload *pPayload;
			px_byte *pdata;
			px_int offset=0;
			PX_memset(newFramePtr,0,size);
			pdata=(px_byte *)newFramePtr;
			pHeader->size=size-sizeof(PX_LiveAnimationFrameHeader);
			offset+=sizeof(PX_LiveAnimationFrameHeader);
			for (i=0;i<plive->layers.size;i++)
			{
				PX_LiveLayer *player=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
				pPayload=(PX_LiveAnimationFramePayload *)(pdata+offset);
				pPayload->translationVerticesCount=player->vertices.size;
				pPayload->mapTexture=player->LinkTextureIndex;
				pPayload->stretch=1;
				offset+=sizeof(PX_LiveAnimationFramePayload)+sizeof(px_point)*player->vertices.size;
			}
			PX_memcpy(pHeader->frameid,id,sizeof(pHeader->frameid));
			return PX_VectorPushback(&pAnimation->framesMemPtr,&newFramePtr);
		}
		else
		{
			return PX_FALSE;
		}
	}


	
	
	return PX_FALSE;
}

px_void PX_LiveFrameworkRunCurrentEditFrame(PX_LiveFramework *plive)
{
	PX_LiveFrameworkExecuteInstr(plive,plive->currentEditAnimationIndex,plive->currentEditFrameIndex);
	PX_LiveFrameworkExecuteInstr(plive,plive->currentEditAnimationIndex,plive->currentEditFrameIndex);
}

typedef struct
{
	px_char id[PX_LIVE_REALTIME_AXIS_ID_MAX_LEN];
	px_uint32 idHash;
	px_byte middleKeyIndex;
	px_byte defaultSampleIndex;
	px_byte coordFractionBits;
	px_byte rotationFractionBits;
	px_byte stretchFractionBits;
	px_uint16 bindingCount;
	px_uint32 vertexIndexCount;
	px_uint32 sampleStride;
	px_uint32 sampleBytes;
	px_uint32 bindingsOffset;
	px_uint32 vertexIndicesOffset;
	px_uint32 samplesOffset;
}PX_LiveFrameworkRT30TrailerAxis;

static px_bool PX_LiveFrameworkRT30CatU8(px_memory *memory,px_byte value)
{
	return PX_MemoryCat(memory,&value,1);
}

static px_bool PX_LiveFrameworkRT30CatU16(px_memory *memory,px_uint16 value)
{
	px_byte bytes[2];
	bytes[0]=(px_byte)value;
	bytes[1]=(px_byte)(value>>8);
	return PX_MemoryCat(memory,bytes,2);
}

static px_bool PX_LiveFrameworkRT30CatU32(px_memory *memory,px_uint32 value)
{
	px_byte bytes[4];
	bytes[0]=(px_byte)value;
	bytes[1]=(px_byte)(value>>8);
	bytes[2]=(px_byte)(value>>16);
	bytes[3]=(px_byte)(value>>24);
	return PX_MemoryCat(memory,bytes,4);
}

static px_bool PX_LiveFrameworkRT30AxisDataLayout(const PX_LiveRealtimeAxis *axis,
	px_uint32 cursor,px_uint32 *bindingsOffset,px_uint32 *vertexIndicesOffset,
	px_uint32 *samplesOffset,px_uint32 *nextOffset)
{
	px_uint32 bytes;
	*bindingsOffset=cursor;
	if (!PX_LiveDeviceSafeMulU32(axis->bindingCount,
		PX_LIVE_RT30_TRAILER_BINDING_ENTRY_SIZE,&bytes)||
		!PX_LiveDeviceSafeAddU32(cursor,bytes,&cursor)||
		!PX_LiveDeviceSafeAlignU32(cursor,4,vertexIndicesOffset)||
		!PX_LiveDeviceSafeMulU32(axis->vertexIndexCount,2,&bytes)||
		!PX_LiveDeviceSafeAddU32(*vertexIndicesOffset,bytes,&cursor)||
		!PX_LiveDeviceSafeAlignU32(cursor,4,samplesOffset)||
		!PX_LiveDeviceSafeAddU32(*samplesOffset,axis->sampleBytes,&cursor)||
		!PX_LiveDeviceSafeAlignU32(cursor,4,nextOffset))
	{
		return PX_FALSE;
	}
	return PX_TRUE;
}

static px_bool PX_LiveFrameworkRT30CalculateTrailerSize(const PX_LiveFramework *plive,
	px_uint32 *trailerSize)
{
	px_uint32 tableBytes,cursor;
	px_int i;
	if (!PX_LiveDeviceSafeMulU32(plive->realtime.axisCount,
		PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE,&tableBytes)||
		!PX_LiveDeviceSafeAddU32(PX_LIVE_RT30_TRAILER_HEADER_SIZE,tableBytes,&cursor)||
		!PX_LiveDeviceSafeAlignU32(cursor,4,&cursor))
	{
		return PX_FALSE;
	}
	for (i=0;i<plive->realtime.axisCount;i++)
	{
		const PX_LiveRealtimeAxis *axis=&plive->realtime.axes[i];
		px_uint32 bindingsOffset,indicesOffset,samplesOffset,sampleBytes;
		px_int idLength=0;
		while (idLength<PX_LIVE_REALTIME_AXIS_ID_MAX_LEN&&axis->id[idLength]) idLength++;
		if (!axis->valid||!axis->idHash||idLength==0||
			idLength>=PX_LIVE_REALTIME_AXIS_ID_MAX_LEN||
			PX_LiveRealtimeHashId(axis->id)!=axis->idHash||
			!axis->bindings||!axis->samples||!axis->bindingCount||
			axis->middleKeyIndex<1||axis->middleKeyIndex>28||
			(axis->defaultSampleIndex!=0&&axis->defaultSampleIndex!=axis->middleKeyIndex&&axis->defaultSampleIndex!=29)||
			axis->coordFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			axis->rotationFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			axis->stretchFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			!axis->sampleStride||
			!PX_LiveDeviceSafeMulU32(axis->sampleStride,PX_LIVE_REALTIME_SAMPLE_COUNT,&sampleBytes)||
			sampleBytes!=axis->sampleBytes||
			(axis->vertexIndexCount&&!axis->vertexIndices)||
			!PX_LiveFrameworkRT30AxisDataLayout(axis,cursor,&bindingsOffset,
				&indicesOffset,&samplesOffset,&cursor))
		{
			return PX_FALSE;
		}
	}
	*trailerSize=cursor;
	return PX_TRUE;
}

static px_bool PX_LiveFrameworkExportRealtimeTrailer(PX_LiveFramework *plive,
	px_memory *exportbuffer)
{
	px_uint32 trailerSize,tableBytes,dataOffset;
	px_int startOffset=exportbuffer->usedsize;
	px_int i;
	if (!plive->realtime.axisCount)
	{
		return PX_TRUE;
	}
	if (!PX_LiveFrameworkRT30CalculateTrailerSize(plive,&trailerSize)||
		trailerSize>0x7fffffffu||
		!PX_LiveDeviceSafeMulU32(plive->realtime.axisCount,
			PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE,&tableBytes)||
		!PX_LiveDeviceSafeAddU32(PX_LIVE_RT30_TRAILER_HEADER_SIZE,tableBytes,&dataOffset)||
		!PX_LiveDeviceSafeAlignU32(dataOffset,4,&dataOffset))
	{
		return PX_FALSE;
	}
	if (!PX_LiveFrameworkRT30CatU32(exportbuffer,PX_LIVE_RT30_TRAILER_MAGIC)||
		!PX_LiveFrameworkRT30CatU16(exportbuffer,PX_LIVE_RT30_TRAILER_VERSION)||
		!PX_LiveFrameworkRT30CatU16(exportbuffer,PX_LIVE_RT30_TRAILER_HEADER_SIZE)||
		!PX_LiveFrameworkRT30CatU32(exportbuffer,trailerSize)||
		!PX_LiveFrameworkRT30CatU16(exportbuffer,plive->realtime.axisCount)||
		!PX_LiveFrameworkRT30CatU16(exportbuffer,PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE)||
		!PX_LiveFrameworkRT30CatU32(exportbuffer,PX_LIVE_RT30_TRAILER_HEADER_SIZE)||
		!PX_LiveFrameworkRT30CatU32(exportbuffer,0))
	{
		return PX_FALSE;
	}
	for (i=0;i<plive->realtime.axisCount;i++)
	{
		const PX_LiveRealtimeAxis *axis=&plive->realtime.axes[i];
		px_uint32 bindingsOffset,indicesOffset,samplesOffset,nextOffset;
		if (!PX_LiveFrameworkRT30AxisDataLayout(axis,dataOffset,&bindingsOffset,
			&indicesOffset,&samplesOffset,&nextOffset)||
			!PX_MemoryCat(exportbuffer,axis->id,PX_LIVE_REALTIME_AXIS_ID_MAX_LEN)||
			!PX_LiveFrameworkRT30CatU32(exportbuffer,axis->idHash)||
			!PX_LiveFrameworkRT30CatU8(exportbuffer,axis->middleKeyIndex)||
			!PX_LiveFrameworkRT30CatU8(exportbuffer,axis->defaultSampleIndex)||
			!PX_LiveFrameworkRT30CatU8(exportbuffer,axis->coordFractionBits)||
			!PX_LiveFrameworkRT30CatU8(exportbuffer,axis->rotationFractionBits)||
			!PX_LiveFrameworkRT30CatU8(exportbuffer,axis->stretchFractionBits)||
			!PX_LiveFrameworkRT30CatU8(exportbuffer,0)||
			!PX_LiveFrameworkRT30CatU16(exportbuffer,axis->bindingCount)||
			!PX_LiveFrameworkRT30CatU32(exportbuffer,axis->vertexIndexCount)||
			!PX_LiveFrameworkRT30CatU32(exportbuffer,axis->sampleStride)||
			!PX_LiveFrameworkRT30CatU32(exportbuffer,axis->sampleBytes)||
			!PX_LiveFrameworkRT30CatU32(exportbuffer,bindingsOffset)||
			!PX_LiveFrameworkRT30CatU32(exportbuffer,indicesOffset)||
			!PX_LiveFrameworkRT30CatU32(exportbuffer,samplesOffset))
		{
			return PX_FALSE;
		}
		dataOffset=nextOffset;
	}
	for (i=0;i<plive->realtime.axisCount;i++)
	{
		const PX_LiveRealtimeAxis *axis=&plive->realtime.axes[i];
		px_int j;
		for (j=0;j<axis->bindingCount;j++)
		{
			const PX_LiveRealtimeBinding *binding=&axis->bindings[j];
			if (!PX_LiveFrameworkRT30CatU16(exportbuffer,binding->layerIndex)||
				!PX_LiveFrameworkRT30CatU16(exportbuffer,binding->propertyMask)||
				!PX_LiveFrameworkRT30CatU16(exportbuffer,binding->vertexCount)||
				!PX_LiveFrameworkRT30CatU16(exportbuffer,0)||
				!PX_LiveFrameworkRT30CatU32(exportbuffer,binding->vertexIndexOffset)||
				!PX_LiveFrameworkRT30CatU32(exportbuffer,binding->sampleOffset))
			{
				return PX_FALSE;
			}
		}
		for (j=0;j<(px_int)axis->vertexIndexCount;j++)
		{
			if (!PX_LiveFrameworkRT30CatU16(exportbuffer,axis->vertexIndices[j])) return PX_FALSE;
		}
		while (((px_uint32)(exportbuffer->usedsize-startOffset)&3u)!=0)
		{
			if (!PX_LiveFrameworkRT30CatU8(exportbuffer,0)) return PX_FALSE;
		}
		for (j=0;j<(px_int)(axis->sampleBytes/2);j++)
		{
			if (!PX_LiveFrameworkRT30CatU16(exportbuffer,(px_uint16)axis->samples[j])) return PX_FALSE;
		}
		while (((px_uint32)(exportbuffer->usedsize-startOffset)&3u)!=0)
		{
			if (!PX_LiveFrameworkRT30CatU8(exportbuffer,0)) return PX_FALSE;
		}
	}
	return (px_uint32)(exportbuffer->usedsize-startOffset)==trailerSize;
}

static px_bool PX_LiveFrameworkRT30ReadAxis(const px_byte *data,px_uint32 size,
	px_uint32 axesOffset,px_uint32 index,PX_LiveFrameworkRT30TrailerAxis *axis)
{
	PX_LiveDeviceReader reader;
	px_uint32 relative,offset;
	px_byte reserved;
	if (!PX_LiveDeviceSafeMulU32(index,PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE,&relative)||
		!PX_LiveDeviceSafeAddU32(axesOffset,relative,&offset)||
		!PX_LiveDeviceRangeIsValid(size,offset,PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE)||
		!PX_LiveDeviceReaderInitialize(&reader,data,size)||
		!PX_LiveDeviceReaderSkip(&reader,offset)||
		!PX_LiveDeviceReaderReadBytes(&reader,axis->id,sizeof(axis->id))||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->idHash)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->middleKeyIndex)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->defaultSampleIndex)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->coordFractionBits)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->rotationFractionBits)||
		!PX_LiveDeviceReaderReadU8(&reader,&axis->stretchFractionBits)||
		!PX_LiveDeviceReaderReadU8(&reader,&reserved)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&axis->bindingCount)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->vertexIndexCount)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->sampleStride)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->sampleBytes)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->bindingsOffset)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->vertexIndicesOffset)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axis->samplesOffset))
	{
		return PX_FALSE;
	}
	return PX_TRUE;
}

static px_bool PX_LiveFrameworkValidateRealtimeTrailer(PX_LiveFramework *plive,
	const px_byte *data,px_uint32 size,px_uint16 axisCount,px_uint32 axesOffset)
{
	px_uint32 tableBytes,dataOffset;
	px_int i;
	px_byte commonCoord=0,commonRotation=0,commonStretch=0;
	if (!PX_LiveDeviceSafeMulU32(axisCount,PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE,&tableBytes)||
		!PX_LiveDeviceSafeAddU32(axesOffset,tableBytes,&dataOffset)||
		!PX_LiveDeviceSafeAlignU32(dataOffset,4,&dataOffset))
	{
		return PX_FALSE;
	}
	for (i=0;i<axisCount;i++)
	{
		PX_LiveFrameworkRT30TrailerAxis axis,previous;
		PX_LiveDeviceReader reader;
		px_uint32 bindingBytes,indexBytes,sampleBytes,nextOffset,staticBytes;
		px_uint32 expectedSampleOffset=0,expectedVertexOffset=0;
		px_int j,idLength=0;
		if (!PX_LiveFrameworkRT30ReadAxis(data,size,axesOffset,i,&axis)) return PX_FALSE;
		while (idLength<PX_LIVE_REALTIME_AXIS_ID_MAX_LEN&&axis.id[idLength]) idLength++;
		if (idLength==0||idLength>=PX_LIVE_REALTIME_AXIS_ID_MAX_LEN||
			PX_LiveRealtimeHashId(axis.id)!=axis.idHash||
			axis.middleKeyIndex<1||axis.middleKeyIndex>28||
			(axis.defaultSampleIndex!=0&&axis.defaultSampleIndex!=axis.middleKeyIndex&&axis.defaultSampleIndex!=29)||
			axis.coordFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			axis.rotationFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			axis.stretchFractionBits>PX_LIVE_DEVICE_MAX_FRACTION_BITS||
			!axis.bindingCount||axis.bindingCount>PX_LIVE_DEVICE_MAX_BINDINGS_PER_AXIS||
			!axis.sampleStride||
			!PX_LiveDeviceSafeMulU32(axis.sampleStride,PX_LIVE_REALTIME_SAMPLE_COUNT,&sampleBytes)||
			sampleBytes!=axis.sampleBytes||
			!PX_LiveDeviceSafeMulU32(axis.bindingCount,PX_LIVE_RT30_TRAILER_BINDING_ENTRY_SIZE,&bindingBytes)||
			!PX_LiveDeviceSafeMulU32(axis.vertexIndexCount,2,&indexBytes)||
			!PX_LiveDeviceSafeAddU32((px_uint32)axis.bindingCount*(px_uint32)sizeof(PX_LiveRealtimeBinding),
				indexBytes,&staticBytes)||
			!PX_LiveDeviceSafeAddU32(staticBytes,axis.sampleBytes,&staticBytes)||
			staticBytes>PX_LIVE_REALTIME_STATIC_BUDGET_BYTES||
			axis.bindingsOffset!=dataOffset||
			!PX_LiveDeviceSafeAddU32(axis.bindingsOffset,bindingBytes,&dataOffset)||
			!PX_LiveDeviceSafeAlignU32(dataOffset,4,&dataOffset)||
			axis.vertexIndicesOffset!=dataOffset||
			!PX_LiveDeviceSafeAddU32(axis.vertexIndicesOffset,indexBytes,&dataOffset)||
			!PX_LiveDeviceSafeAlignU32(dataOffset,4,&dataOffset)||
			axis.samplesOffset!=dataOffset||
			!PX_LiveDeviceSafeAddU32(axis.samplesOffset,axis.sampleBytes,&dataOffset)||
			!PX_LiveDeviceSafeAlignU32(dataOffset,4,&nextOffset)||dataOffset>size)
		{
			return PX_FALSE;
		}
		dataOffset=nextOffset;
		if (i==0)
		{
			commonCoord=axis.coordFractionBits;
			commonRotation=axis.rotationFractionBits;
			commonStretch=axis.stretchFractionBits;
		}
		else if (axis.coordFractionBits!=commonCoord||axis.rotationFractionBits!=commonRotation||
			axis.stretchFractionBits!=commonStretch)
		{
			return PX_FALSE;
		}
		for (j=0;j<i;j++)
		{
			if (!PX_LiveFrameworkRT30ReadAxis(data,size,axesOffset,j,&previous)||
				previous.idHash==axis.idHash) return PX_FALSE;
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		if (!PX_LiveDeviceReaderSkip(&reader,axis.bindingsOffset)) return PX_FALSE;
		for (j=0;j<axis.bindingCount;j++)
		{
			px_uint16 layerIndex,propertyMask,vertexCount,reserved16,vertexIndex;
			px_uint32 vertexOffset,sampleOffset,propertyBytes=0,k,indexByteOffset;
			PX_LiveLayer *layer;
			if (!PX_LiveDeviceReaderReadU16LE(&reader,&layerIndex)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&propertyMask)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&vertexCount)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&reserved16)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&vertexOffset)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&sampleOffset)||
				layerIndex>=(px_uint16)plive->layers.size||!propertyMask||
				(propertyMask&~PX_LIVE_REALTIME_PROPERTY_ALL)||
				vertexOffset!=expectedVertexOffset||sampleOffset!=expectedSampleOffset)
			{
				return PX_FALSE;
			}
			layer=PX_VECTORAT(PX_LiveLayer,&plive->layers,layerIndex);
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_TRANSLATION) propertyBytes+=4;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_ROTATION) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_STRETCH) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_TRANSLATION) propertyBytes+=4;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_ROTATION) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_LOCAL_SCALE) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_TEXTURE) propertyBytes+=2;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_IMPULSE) propertyBytes+=4;
			if (propertyMask&PX_LIVE_REALTIME_PROPERTY_VERTICES)
			{
				if (expectedVertexOffset>axis.vertexIndexCount||
					vertexCount>axis.vertexIndexCount-expectedVertexOffset) return PX_FALSE;
				for (k=0;k<vertexCount;k++)
				{
					PX_LiveDeviceReader indexReader;
					if (!PX_LiveDeviceSafeMulU32(expectedVertexOffset+k,2,&indexByteOffset)||
						!PX_LiveDeviceSafeAddU32(axis.vertexIndicesOffset,indexByteOffset,&indexByteOffset)||
						!PX_LiveDeviceReaderInitialize(&indexReader,data,size)||
						!PX_LiveDeviceReaderSkip(&indexReader,indexByteOffset)||
						!PX_LiveDeviceReaderReadU16LE(&indexReader,&vertexIndex)||
						vertexIndex>=(px_uint16)layer->vertices.size) return PX_FALSE;
				}
				propertyBytes+=(px_uint32)vertexCount*4u;
				expectedVertexOffset+=vertexCount;
			}
			else if (vertexCount) return PX_FALSE;
			if (!PX_LiveDeviceSafeAddU32(expectedSampleOffset,propertyBytes,&expectedSampleOffset)||
				expectedSampleOffset>axis.sampleStride) return PX_FALSE;
		}
		if (expectedVertexOffset!=axis.vertexIndexCount||
			!PX_LiveDeviceSafeAlignU32(expectedSampleOffset,4,&expectedSampleOffset)||
			expectedSampleOffset!=axis.sampleStride) return PX_FALSE;
	}
	return dataOffset==size;
}

static px_void PX_LiveFrameworkFreeTemporaryRealtimeAxis(px_memorypool *mp,
	PX_LiveRealtimeAxis *axis)
{
	if (axis->bindings) MP_Free(mp,axis->bindings);
	if (axis->vertexIndices) MP_Free(mp,axis->vertexIndices);
	if (axis->samples) MP_Free(mp,axis->samples);
	PX_memset(axis,0,sizeof(*axis));
}

static px_bool PX_LiveFrameworkImportRealtimeTrailer(px_memorypool *mp,
	PX_LiveFramework *plive,const px_byte *data,px_uint32 size)
{
	PX_LiveDeviceReader reader;
	px_uint32 magic,chunkSize,axesOffset,reserved32;
	px_uint16 version,headerSize,axisCount,axisEntrySize;
	px_int i;
	if (size<4) return PX_TRUE;
	PX_LiveDeviceReaderInitialize(&reader,data,size);
	if (!PX_LiveDeviceReaderReadU32LE(&reader,&magic)) return PX_FALSE;
	if (magic!=PX_LIVE_RT30_TRAILER_MAGIC) return PX_TRUE;
	if (!PX_LiveDeviceReaderReadU16LE(&reader,&version)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&headerSize)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&chunkSize)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&axisCount)||
		!PX_LiveDeviceReaderReadU16LE(&reader,&axisEntrySize)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&axesOffset)||
		!PX_LiveDeviceReaderReadU32LE(&reader,&reserved32)||
		version!=PX_LIVE_RT30_TRAILER_VERSION||
		headerSize!=PX_LIVE_RT30_TRAILER_HEADER_SIZE||chunkSize!=size||
		!axisCount||axisCount>PX_LIVE_REALTIME_MAX_AXES||
		axisEntrySize!=PX_LIVE_RT30_TRAILER_AXIS_ENTRY_SIZE||
		axesOffset!=PX_LIVE_RT30_TRAILER_HEADER_SIZE||
		!PX_LiveFrameworkValidateRealtimeTrailer(plive,data,size,axisCount,axesOffset))
	{
		return PX_FALSE;
	}
	for (i=0;i<axisCount;i++)
	{
		PX_LiveFrameworkRT30TrailerAxis wire;
		PX_LiveRealtimeAxis source;
		px_int j;
		PX_memset(&source,0,sizeof(source));
		if (!PX_LiveFrameworkRT30ReadAxis(data,size,axesOffset,i,&wire)) return PX_FALSE;
		PX_memcpy(source.id,wire.id,sizeof(source.id));
		source.id[PX_LIVE_REALTIME_AXIS_ID_MAX_LEN-1]=0;
		source.idHash=wire.idHash;
		source.middleKeyIndex=wire.middleKeyIndex;
		source.defaultSampleIndex=wire.defaultSampleIndex;
		source.sampleIndex=wire.defaultSampleIndex;
		source.coordFractionBits=wire.coordFractionBits;
		source.rotationFractionBits=wire.rotationFractionBits;
		source.stretchFractionBits=wire.stretchFractionBits;
		source.valid=PX_TRUE;
		source.weightQ15=0;
		source.bindingCount=wire.bindingCount;
		source.vertexIndexCount=wire.vertexIndexCount;
		source.sampleStride=wire.sampleStride;
		source.sampleBytes=wire.sampleBytes;
		source.bindings=(PX_LiveRealtimeBinding *)MP_Malloc(mp,
			(px_uint)source.bindingCount*sizeof(PX_LiveRealtimeBinding));
		if (source.vertexIndexCount)
		{
			source.vertexIndices=(px_uint16 *)MP_Malloc(mp,
				source.vertexIndexCount*(px_uint)sizeof(px_uint16));
		}
		source.samples=(px_int16 *)MP_Malloc(mp,source.sampleBytes);
		if (!source.bindings||(source.vertexIndexCount&&!source.vertexIndices)||!source.samples)
		{
			PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
			return PX_FALSE;
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		PX_LiveDeviceReaderSkip(&reader,wire.bindingsOffset);
		for (j=0;j<source.bindingCount;j++)
		{
			px_uint16 reserved16;
			if (!PX_LiveDeviceReaderReadU16LE(&reader,&source.bindings[j].layerIndex)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&source.bindings[j].propertyMask)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&source.bindings[j].vertexCount)||
				!PX_LiveDeviceReaderReadU16LE(&reader,&reserved16)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&source.bindings[j].vertexIndexOffset)||
				!PX_LiveDeviceReaderReadU32LE(&reader,&source.bindings[j].sampleOffset))
			{
				PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
				return PX_FALSE;
			}
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		PX_LiveDeviceReaderSkip(&reader,wire.vertexIndicesOffset);
		for (j=0;j<(px_int)source.vertexIndexCount;j++)
		{
			if (!PX_LiveDeviceReaderReadU16LE(&reader,&source.vertexIndices[j]))
			{
				PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
				return PX_FALSE;
			}
		}
		PX_LiveDeviceReaderInitialize(&reader,data,size);
		PX_LiveDeviceReaderSkip(&reader,wire.samplesOffset);
		for (j=0;j<(px_int)(source.sampleBytes/2);j++)
		{
			if (!PX_LiveDeviceReaderReadI16LE(&reader,&source.samples[j]))
			{
				PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
				return PX_FALSE;
			}
		}
		if (!PX_LiveRealtimeInstallBakedAxis(plive,PX_LIVE_REALTIME_INVALID_HANDLE,
			&source,PX_NULL))
		{
			PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
			return PX_FALSE;
		}
		PX_LiveFrameworkFreeTemporaryRealtimeAxis(mp,&source);
	}
	return PX_TRUE;
}

px_bool PX_LiveFrameworkExport(PX_LiveFramework *plive,px_memory *exportbuffer)
{
	//header
	if(!PX_MemoryCat(exportbuffer,"PainterEngineLiveDBinary",24))return PX_FALSE;

	//export LiveFramework structure
	do 
	{

		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_dword version;
			px_int32 width;
			px_int32 height;
			px_int32 layerCount;
			px_int32 animationCount;
			px_int32 textureCount;
		}PX_LiveFrameworkBaseAttributes;

		PX_LiveFrameworkBaseAttributes desc;

		PX_memset(&desc,0,sizeof(PX_LiveFrameworkBaseAttributes));
		
		PX_memcpy(desc.id,plive->id,PX_LIVE_ID_MAX_LEN);
		desc.version = PX_LIVE_VERSION;

		desc.width=plive->width;
		
		desc.height=plive->height;

		desc.layerCount=plive->layers.size;

		desc.animationCount=plive->liveAnimations.size;

		desc.textureCount=plive->livetextures.size;
		
		if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
	} while (0);

	//export live texture
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32 width;
			px_int32 height;
			px_int32 textureOffsetX;
			px_int32 textureOffsetY;
		}PX_LiveTextureExportInfo;

		px_int i;
		for (i=0;i<plive->livetextures.size;i++)
		{
			PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
			
			//export live texture structure
			do 
			{
				PX_LiveTextureExportInfo desc;
				desc.height=pTexture->Texture.height;
				desc.width=pTexture->Texture.width;
				PX_memcpy(desc.id,pTexture->id,sizeof(desc.id));
				desc.textureOffsetX=pTexture->textureOffsetX;
				desc.textureOffsetY=pTexture->textureOffsetY;
				if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
			} while (0);
			
			//export pixels data
			do 
			{
				px_int k;
				px_liveframework_rgba_color renderColor;
				px_color *pColor =(px_color*)(pTexture->Texture.surfaceBuffer);
				for (k = 0; k < pTexture->Texture.width * pTexture->Texture.height; k++)
				{
					renderColor._argb.r = pColor[k]._argb.r;
					renderColor._argb.g = pColor[k]._argb.g;
					renderColor._argb.b = pColor[k]._argb.b;
					renderColor._argb.a = pColor[k]._argb.a;
					if (!PX_MemoryCat(exportbuffer, &renderColor, sizeof(px_liveframework_rgba_color)))return PX_FALSE;
				}
				//if(!PX_MemoryCat(exportbuffer,pTexture->Texture.surfaceBuffer,sizeof(px_color)*pTexture->Texture.height*pTexture->Texture.width))return PX_FALSE;
			} while (0);
		}
	} while (0);

	//export layers
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32 parent_index;
			px_int32 child_index[PX_LIVE_LAYER_MAX_LINK_NODE];
			px_int32 triangleCount;
			px_int32 verticesCount;
			px_point32 KeyPoint;
			px_int32  LinkTextureIndex;
		}PX_LiveFramework_LayerExportInfo;
		px_int i;
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			PX_LiveFramework_LayerExportInfo desc;
			PX_memcpy(desc.id,pLayer->id,sizeof(desc.id));

			desc.triangleCount=pLayer->triangles.size;
			desc.verticesCount=pLayer->vertices.size;

			desc.parent_index=pLayer->parent_index;

			do 
			{
				px_int j;
				for (j=0;j<PX_COUNTOF(pLayer->child_index);j++)
				{
					desc.child_index[j]=pLayer->child_index[j];
				}
			} while (0);

			desc.KeyPoint=pLayer->keyPoint;
			desc.LinkTextureIndex=pLayer->LinkTextureIndex;

			if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
			
			
			//export layer triangles
			do 
			{
				if(!PX_MemoryCat(exportbuffer,pLayer->triangles.data,pLayer->triangles.nodesize*pLayer->triangles.size))return PX_FALSE;
			} while (0);

			//export layer vertex
			do 
			{
				if(!PX_MemoryCat(exportbuffer,pLayer->vertices.data,pLayer->vertices.nodesize*pLayer->vertices.size))return PX_FALSE;
			} while (0);
		}
	} while (0);

	//export live animation
	do 
	{
		px_int i;
		for (i=0;i<plive->liveAnimations.size;i++)
		{
			PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
			//export Live Animation structure
			do 
			{
				typedef struct  
				{
					px_char id[PX_LIVE_ID_MAX_LEN];
					px_int32  size;
				}PX_LiveAnimationExportInfo;

				PX_LiveAnimationExportInfo desc;
				PX_memcpy(desc.id,pAnimation->id,sizeof(desc.id));
				desc.size=pAnimation->framesMemPtr.size;
				if(!PX_MemoryCat(exportbuffer,&desc,sizeof(desc)))return PX_FALSE;
			} while (0);
			
			//export live animation frame
			do 
			{
				px_int j;
				for (j=0;j<pAnimation->framesMemPtr.size;j++)
				{
					//export size
					px_void *pdata;
					px_int32 payloadsize;
					PX_LiveAnimationFrameHeader *pheader;
					pdata=*PX_VECTORAT(px_void*,&pAnimation->framesMemPtr,j);
					pheader=(PX_LiveAnimationFrameHeader *)pdata;
					payloadsize=sizeof(PX_LiveAnimationFrameHeader)+pheader->size;

					if(!PX_MemoryCat(exportbuffer,pdata,payloadsize))return PX_FALSE;
				}
			} while (0);


		}
	} while (0);
	if (!PX_LiveFrameworkExportRealtimeTrailer(plive,exportbuffer)) return PX_FALSE;
	return PX_TRUE;
}

px_bool PX_LiveFrameworkImport(px_memorypool *mp,PX_LiveFramework *plive,px_void *buffer,px_int size)
{
	px_byte*bBuffer = (px_byte *)buffer;
	px_int rOffset=0;
	//////////////////////////////////////////////////////////////////////////
	//check header
	
	do 
	{
		if (!PX_memequ(buffer,"PainterEngineLiveDBinary",24))
		{
			return PX_FALSE;
		}
		rOffset+=24;if(rOffset>size) return PX_FALSE;
		
	} while (0);
	
	//////////////////////////////////////////////////////////////////////////
	//import live framework structure
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_dword version;
			px_int32 width;
			px_int32 height;
			px_int32 layerCount;
			px_int32 animationCount;
			px_int32 textureCount;
		}PX_LiveFrameworkBaseAttributes;

		PX_LiveFrameworkBaseAttributes *pReadLiveFrameworkAttributes=(PX_LiveFrameworkBaseAttributes *)(bBuffer+rOffset);
		rOffset+=sizeof(PX_LiveFrameworkBaseAttributes);if(rOffset>size) return PX_FALSE;

		//////////////////////////////////////////////////////////////////////////
		if (pReadLiveFrameworkAttributes->version != PX_LIVE_VERSION)
		{
			return PX_FALSE;
		}

		PX_memset(plive,0,sizeof(PX_LiveFramework));
		plive->view_scale=1.0f;
		plive->view_revision=1;
		plive->animationMode=PX_LIVE_MODE_NEUTRAL;

		PX_memcpy(plive->id,pReadLiveFrameworkAttributes->id,sizeof(plive->id));
		plive->width=pReadLiveFrameworkAttributes->width;
		plive->height=pReadLiveFrameworkAttributes->height;

		//////////////////////////////////////////////////////////////////////////
		plive->mp=mp;
		PX_LiveRealtimeInitialize(&plive->realtime,mp);
		if(!PX_VectorInitialize(mp,&plive->layers,sizeof(PX_LiveLayer),pReadLiveFrameworkAttributes->layerCount))return PX_FALSE;
		plive->layers.size=pReadLiveFrameworkAttributes->layerCount;

		if(!PX_VectorInitialize(mp,&plive->livetextures,sizeof(PX_LiveTexture),pReadLiveFrameworkAttributes->textureCount))return PX_FALSE;
		plive->livetextures.size=pReadLiveFrameworkAttributes->textureCount;

		if(!PX_VectorInitialize(mp,&plive->liveAnimations,sizeof(PX_LiveAnimation),pReadLiveFrameworkAttributes->animationCount))return PX_FALSE;
		plive->liveAnimations.size=pReadLiveFrameworkAttributes->animationCount;

		plive->reg_animation=0;
		plive->reg_bp=0;
		plive->reg_duration=0;
		plive->reg_elapsed=0;
		plive->reg_ip=0;

		plive->currentEditAnimationIndex=-1;
		plive->currentEditFrameIndex=-1;
		plive->currentEditLayerIndex=-1;
		plive->currentEditVertexIndex=-1;
	} while (0);

	
	//////////////////////////////////////////////////////////////////////////
	//
	//import live texture
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32 width;
			px_int32 height;
			px_int32 textureOffsetX;
			px_int32 textureOffsetY;
		}PX_LiveTextureImportInfo;

		px_int i;
		for (i=0;i<plive->livetextures.size;i++)
		{
			PX_LiveTexture *pTexture=PX_VECTORAT(PX_LiveTexture,&plive->livetextures,i);
			do 
			{
				px_liveframework_rgba_color *pColor;
				px_color *prenderColor;
				px_int k;
				//import live texture structure
				PX_LiveTextureImportInfo *pLiveTextureImportInfo;
				pLiveTextureImportInfo=((PX_LiveTextureImportInfo *)(bBuffer+rOffset));
				PX_memset(pTexture,0,sizeof(PX_LiveTexture));

				if(!PX_TextureCreate(mp,&pTexture->Texture,pLiveTextureImportInfo->width,pLiveTextureImportInfo->height))
					goto _ERROR;
				PX_memcpy(pTexture->id,pLiveTextureImportInfo->id,sizeof(pTexture->id));
				pTexture->textureOffsetX=pLiveTextureImportInfo->textureOffsetX;
				pTexture->textureOffsetY=pLiveTextureImportInfo->textureOffsetY;

				rOffset+=sizeof(PX_LiveTextureImportInfo);if(rOffset>size) 
					goto _ERROR;
				//import texture data
				for (k = 0; k < pTexture->Texture.width * pTexture->Texture.height; k++)
				{
					pColor=(px_liveframework_rgba_color *)(bBuffer+rOffset);
					prenderColor= pTexture->Texture.surfaceBuffer;
					prenderColor[k]._argb.r=pColor[k]._argb.r;
					prenderColor[k]._argb.g=pColor[k]._argb.g;
					prenderColor[k]._argb.b=pColor[k]._argb.b;
					prenderColor[k]._argb.a=pColor[k]._argb.a;
				}
				//PX_memcpy(pTexture->Texture.surfaceBuffer,(bBuffer+rOffset),pTexture->Texture.width*pTexture->Texture.height*sizeof(px_color));
				rOffset+=pTexture->Texture.width*pTexture->Texture.height*sizeof(px_color);if(rOffset>size) 
					goto _ERROR;
			} while (0);
		}
	} while (0);

	//////////////////////////////////////////////////////////////////////////
	//import layer
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int parent_index;
			px_int child_index[PX_LIVE_LAYER_MAX_LINK_NODE];
			px_int triangleCount;
			px_int verticesCount;
			px_point KeyPoint;
			px_int  LinkTextureIndex;
		}PX_LiveFramework_LayerExportInfo;

		px_int i;
		for (i=0;i<plive->layers.size;i++)
		{
			PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&plive->layers,i);
			PX_LiveFramework_LayerExportInfo *pReadLayer=(PX_LiveFramework_LayerExportInfo *)(bBuffer+rOffset);
			rOffset+=sizeof(PX_LiveFramework_LayerExportInfo);if(rOffset>size) goto _ERROR;

			PX_memset(pLayer,0,sizeof(PX_LiveLayer));
			PX_memcpy(pLayer->id,pReadLayer->id,sizeof(pLayer->id));
			pLayer->keyPoint=pReadLayer->KeyPoint;
			pLayer->LinkTextureIndex=pReadLayer->LinkTextureIndex;
			pLayer->RenderTextureIndex=pLayer->LinkTextureIndex;

			pLayer->rel_beginStretch=1;
			pLayer->rel_currentStretch=1;
			pLayer->rel_endStretch=1;
			pLayer->rel_beginLocalScale=1;
			pLayer->rel_currentLocalScale=1;
			pLayer->rel_endLocalScale=1;
			pLayer->visible=PX_TRUE;

			if(!PX_VectorInitialize(mp,&pLayer->triangles,sizeof(PX_Delaunay_Triangle),pReadLayer->triangleCount)) 
				goto _ERROR;
			pLayer->triangles.size=pReadLayer->triangleCount;

			if(!PX_VectorInitialize(mp,&pLayer->vertices,sizeof(PX_LiveVertex),pReadLayer->verticesCount)) 
				goto _ERROR;
			pLayer->vertices.size=pReadLayer->verticesCount;


			pLayer->parent_index=pReadLayer->parent_index;

			do 
			{
				px_int j;
				for (j=0;j<PX_COUNTOF(pReadLayer->child_index);j++)
				{
					pLayer->child_index[j]=pReadLayer->child_index[j];
				}
			} while (0);

			
			//import layer triangles
			do 
			{
				PX_memcpy(pLayer->triangles.data,bBuffer+rOffset,pLayer->triangles.nodesize*pLayer->triangles.size);
				rOffset+=pLayer->triangles.nodesize*pLayer->triangles.size;if(rOffset>size) goto _ERROR;
			} while (0);

			//import layer vertex
			do 
			{
				PX_memcpy(pLayer->vertices.data,bBuffer+rOffset,pLayer->vertices.nodesize*pLayer->vertices.size);
				rOffset+=pLayer->vertices.nodesize*pLayer->vertices.size;if(rOffset>size) goto _ERROR;
			} while (0);
		}
	} while (0);

	//import live animation
	do 
	{
		typedef struct  
		{
			px_char id[PX_LIVE_ID_MAX_LEN];
			px_int32  size;
		}PX_LiveAnimationImportInfo;

		px_int i;
		for (i=0;i<plive->liveAnimations.size;i++)
		{
			PX_LiveAnimation *pAnimation=PX_VECTORAT(PX_LiveAnimation,&plive->liveAnimations,i);
			
			//import Live Animation structure
			do 
			{
				PX_LiveAnimationImportInfo *pLiveAnimationImport=(PX_LiveAnimationImportInfo *)(bBuffer+rOffset);
				rOffset+=sizeof(PX_LiveAnimationImportInfo);if(rOffset>size) goto _ERROR;

				PX_memcpy(pAnimation->id,pLiveAnimationImport->id,sizeof(pAnimation->id));

				if(!PX_VectorInitialize(mp,&pAnimation->framesMemPtr,sizeof(px_void *),pLiveAnimationImport->size))goto _ERROR;
				pAnimation->framesMemPtr.size=pLiveAnimationImport->size;
			} while (0);

			//import live animation frame
			do 
			{
				px_int j;
				for (j=0;j<pAnimation->framesMemPtr.size;j++)
				{
					//import size
					px_void **ppdata;
					px_int32 payloadsize;
					PX_LiveAnimationFrameHeader *pheader;
					ppdata=PX_VECTORAT(px_void*,&pAnimation->framesMemPtr,j);
					pheader=(PX_LiveAnimationFrameHeader *)(bBuffer+rOffset);
					payloadsize=sizeof(PX_LiveAnimationFrameHeader)+pheader->size;

					*ppdata=MP_Malloc(mp,payloadsize);
					if(*ppdata==PX_NULL) goto _ERROR;
					PX_memcpy(*ppdata,bBuffer+rOffset,payloadsize);
					rOffset+=payloadsize;if(rOffset>size) goto _ERROR;
				}
			} while (0);
		}
	} while (0);
	if (rOffset<size&&!PX_LiveFrameworkImportRealtimeTrailer(mp,plive,bBuffer+rOffset,
		(px_uint32)(size-rOffset))) goto _ERROR;
	PX_LiveFrameworkReset(plive);
	return PX_TRUE;
_ERROR:
	PX_LiveFrameworkFree(plive);
	return PX_FALSE;
}

//////////////////////////////////////////////////////////////////////////
//liveframework mirror

px_bool PX_LiveCreate(px_memorypool *mp,PX_LiveFramework *pLiveFramework,PX_Live *pLive)
{
	px_int i,initializedVertices=0;
	*pLive=*pLiveFramework;
	pLive->mp=mp;
	PX_LiveRealtimeInitialize(&pLive->realtime,mp);
	if(!PX_VectorInitialize(mp,&pLive->layers,sizeof(PX_LiveLayer),0))goto _ERROR;
	if(!PX_VectorCopy(&pLive->layers,&pLiveFramework->layers)) goto _ERROR;
	for (i=0;i<pLive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&pLive->layers,i);
		PX_LiveLayer *pFrameworkLayer=PX_VECTORAT(PX_LiveLayer,&pLiveFramework->layers,i);
		PX_memset(&pLayer->vertices,0,sizeof(pLayer->vertices));
		if(!PX_VectorInitialize(mp,&pLayer->vertices,sizeof(PX_LiveVertex),0))
		{
			goto _ERROR;
		}
		initializedVertices++;
		if(!PX_VectorCopy(&pLayer->vertices,&pFrameworkLayer->vertices))
		{
			goto _ERROR;
		}
	}
	if (!PX_LiveRealtimeClone(&pLive->realtime,mp,&pLiveFramework->realtime))
	{
		goto _ERROR;
	}
	if (!PX_LiveRealtimePrepareRuntime(pLive))
	{
		goto _ERROR;
	}
	return PX_TRUE;
_ERROR:
	PX_LiveRealtimeFree(&pLive->realtime);
	for (i=0;i<initializedVertices;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&pLive->layers,i);
		PX_VectorFree(&pLayer->vertices);
	}
	PX_VectorFree(&pLive->layers);
	PX_memset(pLive,0,sizeof(*pLive));
	pLive->mp=mp;
	return PX_FALSE;
}

px_void PX_LiveFree(PX_Live *pLive)
{
	px_int i;
	PX_LiveRealtimeFree(&pLive->realtime);
	for (i=0;i<pLive->layers.size;i++)
	{
		PX_LiveLayer *pLayer=PX_VECTORAT(PX_LiveLayer,&pLive->layers,i);
		PX_VectorFree(&pLayer->vertices);
	}
	PX_VectorFree(&pLive->layers);
}

px_void PX_LivePlay(PX_Live*plive)
{
	PX_LiveFrameworkPlay(plive);
}

px_int PX_LiveGetAnimationCount(PX_Live* plive)
{
	return plive->liveAnimations.size;
}

px_bool PX_LivePlayAnimation(PX_Live *plive,px_int index)
{
	return PX_LiveFrameworkPlayAnimation(plive,index);
}

px_bool PX_LivePlayAnimationByName(PX_Live *plive,const px_char name[])
{
	return PX_LiveFrameworkPlayAnimationByName(plive,name);
}

px_void PX_LivePause(PX_Live *plive)
{
	PX_LiveFrameworkPause(plive);
}

px_void PX_LiveReset(PX_Live *plive)
{
	PX_LiveFrameworkReset(plive);
}

px_void PX_LiveStop(PX_Live *plive)
{
	PX_LiveFrameworkStop(plive);
}

px_bool PX_LiveEnterRealtime30(PX_Live *plive)
{
	return PX_LiveRealtimeEnter(plive);
}

px_void PX_LiveLeaveRealtime30(PX_Live *plive)
{
	PX_LiveRealtimeLeave(plive);
}

px_bool PX_LiveSetRealtimeAxisSample(PX_Live *plive,px_int axisHandle,px_uchar sampleIndex,px_uint16 weightQ15)
{
	return PX_LiveRealtimeSetAxisSample(plive,axisHandle,sampleIndex,weightQ15);
}

px_bool PX_LiveSetRealtimeAxisNormalizedQ15(PX_Live *plive,px_int axisHandle,px_uint16 normalizedQ15,px_uint16 weightQ15)
{
	return PX_LiveRealtimeSetAxisNormalizedQ15(plive,axisHandle,normalizedQ15,weightQ15);
}

px_int PX_LiveFindRealtimeAxisById(const PX_Live *plive,const px_char id[])
{
	return PX_LiveRealtimeFindAxisById(plive,id);
}

px_void PX_LiveRender(px_surface *psurface,PX_Live *plive,px_int x,px_int y,PX_ALIGN refPoint,px_dword elapsed)
{
	PX_LiveFrameworkRender(psurface,plive,(px_float)x,(px_float)y,refPoint,elapsed);
}
