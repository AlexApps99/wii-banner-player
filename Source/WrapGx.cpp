
#include <gl/glew.h>
#include <gl/glu.h>
#include <gl/gl.h>

#include <iostream>

#include "TextureDecoder.h"

#include "WrapGx.h"

static u8 g_texture_decode_buffer[512 * 512 * 4];

#define ARRAY_LENGTH(a)	(sizeof(a)/sizeof(*a))

// silly
GXFifoObj * 	GX_Init (void *base, u32 size)
{
	glewInit();

	return NULL;
}

struct GLTexObj
{
	GLuint tex;
	//u16 width, height;
};

// TODO: doesn't handle mipmap or maxlod
u32 	GX_GetTexBufferSize (u16 wd, u16 ht, u32 fmt, u8 mipmap, u8 maxlod)
{
	const u32 bsw = TexDecoder_GetBlockWidthInTexels(fmt) - 1;
	const u32 bsh = TexDecoder_GetBlockHeightInTexels(fmt) - 1;

	const u32 expanded_width  = (wd  + bsw) & (~bsw);
	const u32 expanded_height = (ht + bsh) & (~bsh);

	return TexDecoder_GetTextureSizeInBytes(expanded_width, expanded_height, fmt);
}

void 	GX_InitTexObj (GXTexObj *obj, void *img_ptr, u16 wd, u16 ht, u8 fmt, u8 wrap_s, u8 wrap_t, u8 mipmap)
{	
	GLTexObj& txobj = *(GLTexObj*)obj;

	// generate texture
	glGenTextures(1, &txobj.tex);
	glBindTexture(GL_TEXTURE_2D, txobj.tex);

	// texture lods
	// TODO: not sure if correct
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, min_lod);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, max_lod);

	// TODO: ?
	//edge_lod
	//lod_bias
	//wrap_s		// these 2 are handled by the materials values
	//wrap_t

	const u32 bsw = TexDecoder_GetBlockWidthInTexels(fmt) - 1;
	const u32 bsh = TexDecoder_GetBlockHeightInTexels(fmt) - 1;

	const u32 expanded_width  = (wd  + bsw) & (~bsw);
	const u32 expanded_height = (ht + bsh) & (~bsh);

	GLenum gl_format, gl_iformat, gl_type = 0;

	// decode texture
	auto const pcfmt = TexDecoder_Decode(g_texture_decode_buffer,
		(u8*)img_ptr, expanded_width, expanded_height, fmt, 0, 0, true);

	// load texture
	switch (pcfmt)
	{
	default:
	case PC_TEX_FMT_NONE:
		std::cout << "Error decoding texture!!!\n";

	case PC_TEX_FMT_BGRA32:
		gl_format = GL_BGRA;
		gl_iformat = 4;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_RGBA32:
		gl_format = GL_RGBA;
		gl_iformat = 4;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_I4_AS_I8:
		gl_format = GL_LUMINANCE;
		gl_iformat = GL_INTENSITY4;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_IA4_AS_IA8:
		gl_format = GL_LUMINANCE_ALPHA;
		gl_iformat = GL_LUMINANCE4_ALPHA4;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_I8:
		gl_format = GL_LUMINANCE;
		gl_iformat = GL_INTENSITY8;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_IA8:
		gl_format = GL_LUMINANCE_ALPHA;
		gl_iformat = GL_LUMINANCE8_ALPHA8;
		gl_type = GL_UNSIGNED_BYTE;
		break;

	case PC_TEX_FMT_RGB565:
		gl_format = GL_RGB;
		gl_iformat = GL_RGB;
		gl_type = GL_UNSIGNED_SHORT_5_6_5;
		break;
	}

	if (expanded_width != wd)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, expanded_width);

	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexImage2D(GL_TEXTURE_2D, 0, gl_iformat, wd, ht, 0, gl_format, gl_type, g_texture_decode_buffer);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);

	if (expanded_width != wd)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void 	GX_InitTexObjWrapMode (GXTexObj *obj, u8 wrap_s, u8 wrap_t)
{
	GLTexObj& txobj = *(GLTexObj*)obj;

	glBindTexture(GL_TEXTURE_2D, txobj.tex);

	static const GLenum wraps[] =
	{
		GL_CLAMP_TO_EDGE,
		GL_REPEAT,
		GL_MIRRORED_REPEAT,
		GL_REPEAT,
	};

	if (wrap_s < ARRAY_LENGTH(wraps))
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wraps[wrap_s]);

	if (wrap_t < ARRAY_LENGTH(wraps))
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wraps[wrap_t]);
}

void 	GX_InitTexObjFilterMode (GXTexObj *obj, u8 minfilt, u8 magfilt)
{
	GLTexObj& txobj = *(GLTexObj*)obj;

	glBindTexture(GL_TEXTURE_2D, txobj.tex);

	const GLint filters[] =
	{
		GL_NEAREST,
		GL_LINEAR,
		GL_NEAREST_MIPMAP_NEAREST,
		GL_LINEAR_MIPMAP_NEAREST,
		GL_NEAREST_MIPMAP_LINEAR,
		GL_LINEAR_MIPMAP_LINEAR,
	};

	// texture filters
	if (minfilt < ARRAY_LENGTH(filters))
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filters[minfilt]);

	if (magfilt < ARRAY_LENGTH(filters))
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filters[magfilt]);
}

static const GLenum logic_ops[] =
{
	GL_CLEAR,
	GL_AND,
	GL_AND_REVERSE,
	GL_COPY,
	GL_AND_INVERTED,
	GL_NOOP,
	GL_XOR,
	GL_OR,
	GL_NOR,
	GL_EQUIV,
	GL_INVERT,
	GL_OR_REVERSE,
	GL_COPY_INVERTED,
	GL_OR_INVERTED,
	GL_NAND,
	GL_SET,
};

void 	GX_SetBlendMode (u8 type, u8 src_fact, u8 dst_fact, u8 op)
{
	static const GLenum blend_types[] =
	{
		0,	// none
		GL_FUNC_ADD,
		GL_FUNC_REVERSE_SUBTRACT,	// LOGIC??
		GL_FUNC_SUBTRACT,
	};

	if (type < ARRAY_LENGTH(blend_types))
	{
		if (type)
		{
			glEnable(GL_BLEND);
			glBlendEquation(blend_types[type]);
		}
		else
		{
			glDisable(GL_BLEND);
		}
	}

	static const GLenum blend_factors[] =
	{
		GL_ZERO,
		GL_ONE,
		GL_SRC_COLOR,
		GL_ONE_MINUS_SRC_COLOR,
		GL_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA,
		GL_ONE_MINUS_DST_ALPHA,
	};

	if (src_fact < ARRAY_LENGTH(blend_factors) && dst_fact < ARRAY_LENGTH(blend_factors))
	{
		glBlendFunc(blend_factors[src_fact], blend_factors[dst_fact]);
	}

	if (op < ARRAY_LENGTH(logic_ops))
	{
		glLogicOp(logic_ops[op]);
	}
}

// TODO: incomplete
void 	GX_SetAlphaCompare (u8 comp0, u8 ref0, u8 aop, u8 comp1, u8 ref1)
{
	static const GLenum alpha_funcs[] =
	{
		GL_NEVER,
		GL_EQUAL,
		GL_LEQUAL,
		GL_GREATER,
		GL_NOTEQUAL,
		GL_GEQUAL,
		GL_ALWAYS,
	};

	if (comp0 < ARRAY_LENGTH(alpha_funcs))
	{
		glAlphaFunc(alpha_funcs[comp0], (float)ref0 / 255.f);
		//glAlphaFunc(alpha_funcs[comp1], (float)ref1 / 255.f);
	}

	if (aop < ARRAY_LENGTH(logic_ops))
	{
		//glLogicOp();	// TODO: need to do this guy, but for alpha
	}
}

void 	GX_Begin (u8 primitve, u8 vtxfmt, u16 vtxcnt)
{
	// hax
	glBegin(GL_TRIANGLE_FAN);
}

void 	GX_End ()
{
	glEnd();
}

void	GX_Position3f32 (f32 x, f32 y, f32 z)
{
	glVertex3f(x, y, z);
}

void	GX_Color4u8 (u8 r, u8 g, u8 b, u8 a)
{
	glColor4ub(r, g, b, a);
}

void	GX_Coloru32 (u32 c)
{
	glColor4ubv((u8*)&c);
}

void	GX_TexCoord2f32 (f32 s, f32 t)
{
	glTexCoord2f(s, t);
}

// TODO: not using mt at all, fix that

void 	guMtxIdentity (Mtx mt)
{
	glLoadIdentity();
}

void 	guOrtho (Mtx44 mt, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
	glOrtho(l, r, b, t, n, f);
}

void 	guLightOrtho (Mtx mt, f32 t, f32 b, f32 l, f32 r, f32 scaleS, f32 scaleT, f32 transS, f32 transT)
{
	// hax
	guOrtho(mt, t, b, l, r, -1000, 1000);
}

void 	guMtxTrans (Mtx mt, f32 xT, f32 yT, f32 zT)
{
	glTranslatef(xT, yT, zT);
}

void 	guMtxScale (Mtx mt, f32 xS, f32 yS, f32 zS)
{
	glScalef(xS, yS, zS);
}

GLuint g_texture_slots[8] = {};

void 	GX_LoadTexObj (GXTexObj *obj, u8 mapid)
{
	const GLTexObj& txobj = *(GLTexObj*)obj;

	g_texture_slots[mapid & 0x7] = txobj.tex;
}

void 	guMtxRotAxisRad (Mtx mt, guVector *axis, f32 rad)
{
	glRotatef(rad, axis->x, axis->y, axis->z);
}

inline void ActiveState(u8 stage)
{
	glActiveTexture(GL_TEXTURE0 + stage);
}

void 	GX_SetTevOrder (u8 tevstage, u8 texcoord, u32 texmap, u8 color)
{
	ActiveState(tevstage);

	// TODO: support texture disable n crap?

	glBindTexture(GL_TEXTURE_2D, g_texture_slots[texmap & 7]);

	//glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGBA);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_INTERPOLATE);

	// messin around
	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}

void 	GX_SetTevSwapMode (u8 tevstage, u8 ras_sel, u8 tex_sel)
{
	ActiveState(tevstage);

	// TODO:
}

void 	GX_SetTevIndirect (u8 tevstage, u8 indtexid, u8 format, u8 bias, u8 mtxid,
	u8 wrap_s, u8 wrap_t, u8 addprev, u8 utclod, u8 a)
{
	ActiveState(tevstage);
}

static const GLuint combine_ops[] =
{
	GL_ADD,
	GL_DECAL,
	GL_MODULATE,
	GL_BLEND, // TODO: passclear
	GL_REPLACE,
};

void 	GX_SetTevKAlphaSel (u8 tevstage, u8 sel)
{
	ActiveState(tevstage);
}

void 	GX_SetTevKColorSel (u8 tevstage, u8 sel)
{
	ActiveState(tevstage);
}

static const GLint alpha_combiner_inputs[] =
{
	GL_PREVIOUS,
	GL_PREVIOUS,	// GX_CA_A0
	GL_PREVIOUS,	// GX_CA_A0 
	GL_PREVIOUS,	// GX_CA_A1
	GL_TEXTURE,
	GL_PREVIOUS,	// TODO: rasterizer
	GL_CONSTANT,
	GL_ZERO,
};

static const GLint alpha_combiner_first_inputs[] =
{
	GL_PREVIOUS,
	GL_ZERO,	// GX_CA_A0
	GL_ONE,	// GX_CA_A0
	GL_PREVIOUS,	// GX_CA_A1
	GL_TEXTURE,
	GL_PRIMARY_COLOR,	// TODO: rasterizer
	GL_CONSTANT,
	GL_ZERO,
};

void 	GX_SetTevAlphaIn (u8 tevstage, u8 a, u8 b, u8 c, u8 d)
{
	ActiveState(tevstage);

	//return;

	//glColor4ub(0xff, 0xff, 0xff, 0xff);

	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, alpha_combiner_first_inputs[b & 0x7]);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, alpha_combiner_first_inputs[a & 0x7]);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA, alpha_combiner_first_inputs[c & 0x7]);

	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA, GL_SRC_ALPHA);
}

void 	GX_SetTevAlphaOp (u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid)
{
	ActiveState(tevstage);

	glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, tevscale);

	//glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);

	//const float
	//	bias = (float)tevbias / 255,
	//	scale = (float)tevscale / 255;

	//glPixelTransferf(GL_ALPHA_BIAS, bias);
	//glPixelTransferf(GL_ALPHA_SCALE, scale);
}

static const GLint color_combiner_inputs[] =
{
	GL_PREVIOUS,
	GL_PREVIOUS,	// TODO: alpha
	GL_PRIMARY_COLOR,//GL_SRC0_RGB,
	GL_PRIMARY_COLOR,//GL_SRC0_ALPHA,
	GL_PRIMARY_COLOR,//GL_SRC1_RGB,
	GL_PRIMARY_COLOR,//GL_SRC1_ALPHA,
	GL_PRIMARY_COLOR,//GL_SRC2_RGB,
	GL_PRIMARY_COLOR,//GL_SRC2_ALPHA,
	GL_TEXTURE,
	GL_TEXTURE,	// TODO: alpha
	GL_PRIMARY_COLOR,	// ras
	GL_PRIMARY_COLOR,	// ras
	GL_PRIMARY_COLOR,
	GL_PREVIOUS,	// half
	GL_CONSTANT,
	GL_ZERO,
};

void 	GX_SetTevColorIn (u8 tevstage, u8 a, u8 b, u8 c, u8 d)
{
	ActiveState(tevstage);

	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, color_combiner_inputs[b & 0xf]);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, color_combiner_inputs[a & 0xf]);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, color_combiner_inputs[c & 0xf]);

	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
}

void 	GX_SetTevColorOp (u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid)
{
	ActiveState(tevstage);

	glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, tevscale);

	//glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	
	//const float
	//	bias = (float)tevbias / 255,
	//	scale = (float)tevscale / 255;

	//glPixelTransferf(GL_RED_BIAS, bias);
	//glPixelTransferf(GL_BLUE_BIAS, bias);
	//glPixelTransferf(GL_GREEN_BIAS, bias);

	//glPixelTransferf(GL_RED_SCALE, scale);
	//glPixelTransferf(GL_BLUE_SCALE, scale);
	//glPixelTransferf(GL_GREEN_SCALE, scale);
}

void 	GX_SetNumTevStages (u8 num)
{
	static u8 current_num = 0;

	// enable stages
	while (current_num < num)
	{
		ActiveState(current_num);
		glEnable(GL_TEXTURE_2D);
		//glEnable(GL_TEXTURE_RECTANGLE_ARB);

		++current_num;
	}

	// disable stages
	while (current_num > num)
	{
		--current_num;

		ActiveState(current_num);
		glDisable(GL_TEXTURE_2D);
		//glDisable(GL_TEXTURE_RECTANGLE_ARB);
	}
}
