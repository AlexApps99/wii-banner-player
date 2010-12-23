
#include <gl/glew.h>
#include <gl/glu.h>
#include <gl/gl.h>

#include <iostream>
#include <map>
#include <vector>
#include <sstream>

#include "TextureDecoder.h"

#include "WrapGx.h"

#include "Types.h"

static u8 g_texture_decode_buffer[512 * 512 * 4];

#define USE_GLSL	1
#define USE_TEXENV	2

#define TEV_EMULATION		USE_GLSL
//#define TEV_EMULATION		USE_TEXENV

static float g_color_registers[3][4];

static const GLuint g_texmap_start_index = 1;
static const GLuint g_framebuffer_index = 0;
static GLuint g_framebuffer_texture;

// silly
GXFifoObj * 	GX_Init (void *base, u32 size)
{
	glewInit();

	glGenTextures(1, &g_framebuffer_texture);
	glBindTexture(GL_TEXTURE_2D, g_framebuffer_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 608, 456, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

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
	glActiveTexture(GL_TEXTURE0);
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
		(u8*)img_ptr, expanded_width, expanded_height, fmt, 0, 0, true);	// TODO: doesn't need to be true, just testing stuff

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

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wraps[wrap_s & 0x3]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wraps[wrap_t & 0x3]);
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
		GL_NEAREST,	// blah
		GL_NEAREST,
	};

	// texture filters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filters[minfilt & 0x7]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filters[magfilt & 0x7]);
}

void 	GX_SetBlendMode (u8 type, u8 src_fact, u8 dst_fact, u8 op)
{
	static const GLenum blend_types[] =
	{
		0,	// none
		GL_FUNC_ADD,
		GL_FUNC_REVERSE_SUBTRACT,	// LOGIC??
		GL_FUNC_SUBTRACT,
	};

	if (type)
	{
		glEnable(GL_BLEND);
		glBlendEquation(blend_types[type & 0x3]);
	}
	else
	{
		glDisable(GL_BLEND);
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

	glBlendFunc(blend_factors[src_fact & 0x7], blend_factors[dst_fact & 0x7]);

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

	glLogicOp(logic_ops[op & 0xf]);
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
		GL_ALWAYS,	 // blah
	};

	glAlphaFunc(alpha_funcs[comp0 & 0x7], (float)ref0 / 255.f);
	//glAlphaFunc(alpha_funcs[comp1 & 0x7], (float)ref1 / 255.f);


	//glLogicOp();	// TODO: need to do this guy, but for alpha
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

#if TEV_EMULATION == USE_GLSL

struct TevStageProps
{
	TevStageProps()
	{
		memset(this, 0, sizeof(*this));
	}

	// color inputs
	u8 color_a : 4;
	u8 color_b : 4;

	u8 color_c : 4;
	u8 color_d : 4;

	// alpha inputs
	u8 alpha_a : 4;
	u8 alpha_b : 4;

	u8 alpha_c : 4;
	u8 alpha_d : 4;

	// outputs
	u8 color_regid : 1;
	u8 alpha_regid : 1;
	u8 pad : 6;

	u8 texcoord;

	u8 texmap;

	bool operator<(const TevStageProps& rhs) const
	{
		return memcmp(this, &rhs, sizeof(*this)) < 0;
	}
};

typedef std::vector<TevStageProps> TevStages;

struct CompiledTevStages
{
	CompiledTevStages()
		: program(0)
		, fragment_shader(0)
		, vertex_shader(0)
	{}

	void Enable();
	void Compile(const TevStages& stages);

	GLuint program, fragment_shader, vertex_shader;
};

std::map<TevStages, CompiledTevStages> g_compiled_tev_stages;

TevStages g_active_stages;

void CompiledTevStages::Enable()
{
	glUseProgram(program);

	// TODO: cache these values
	glUniform4fv(glGetUniformLocation(program, "registers"), 3, g_color_registers[0]);
}

void CompiledTevStages::Compile(const TevStages& stages)
{
	// w.e good for now
	static const unsigned int sampler_count = 8;

	// generate vertex/fragment shader code
	{
	std::ostringstream vert_ss;
	
	//for (unsigned int i = 0; i != sampler_count; ++i)
		//vert_ss << "varying vec2 texcoords" << i << ';';

	vert_ss << "varying vec2 position_fb" ";";

	vert_ss << "void main(){";

	vert_ss << "gl_FrontColor = gl_Color;";
	vert_ss << "gl_BackColor = gl_Color;";
		
	for (unsigned int i = 0; i != sampler_count; ++i)
		vert_ss << "gl_TexCoord[" << i << "] = gl_TextureMatrix[" << i << "] * gl_MultiTexCoord" << 0 << ";";

	vert_ss << "gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;";
	//vert_ss << "position_fb = vec2(gl_Position.x * 0.5 + 0.5, gl_Position.y * 0.5 + 0.5);";
	vert_ss << "position_fb = gl_Position.xy;";
	vert_ss << '}';

	// create/compile vertex shader
	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	
	{
	const auto& vert_src_str = vert_ss.str();
	const GLchar* vert_src = vert_src_str.c_str();
	glShaderSource(vertex_shader, 1, &vert_src, NULL);
	}

	}	// done generating verex shader

	glCompileShader(vertex_shader);

	// generate fragment shader code
	{
	std::ostringstream frag_ss;

	// uniforms
	// textures
	for (unsigned int i = 0; i != sampler_count; ++i)
		frag_ss << "uniform sampler2D textures" << i << ';';
	frag_ss << "uniform sampler2D texture_fb" ";";
	// color/output registers
	frag_ss << "uniform vec4 registers[3]" ";";
	// const color
	frag_ss << "uniform vec4 color_constant" ";";

	// these come from the vertex shader
	frag_ss << "varying vec2 position_fb" ";";
	//for (unsigned int i = 0; i != sampler_count; ++i)
		//frag_ss << "varying vec2 texcoords" << i << ';';

	frag_ss << "void main(){";

	// previous stage color
	//frag_ss << "vec4 color_previous = vec4(1.0)" ";";
	frag_ss << "vec4 color_previous = texture2D(texture_fb, position_fb);";
	// current stage texture color
	frag_ss << "vec4 color_texture" ";";
	// color/output registers
	for (unsigned int i = 0; i != 3; ++i)
		frag_ss << "vec4 color_registers" << i << " = registers[" << i << "]" ";";

	static const char* const color_inputs[] =
	{
		"color_previous" ".rgb",
		"color_previous" ".aaa",
		"color_registers" "0" ".rgb",
		"color_registers" "0" ".aaa",
		"color_registers" "1" ".rgb",
		"color_registers" "1" ".aaa",
		"color_registers" "2" ".rgb",
		"color_registers" "2" ".aaa",
		"color_texture" ".rgb",
		"color_texture" ".aaa",
		"gl_Color" ".rgb",
		"gl_Color" ".aaa",
		"vec3(1.0)",
		"vec3(0.5)",
		"color_constant" ".rgb",
		"vec3(0.0)",
	};

	static const char* const alpha_inputs[] =
	{
		"color_previous" ".a",
		"color_registers" "0" ".a",
		"color_registers" "1" ".a",
		"color_registers" "2" ".a",
		"color_texture" ".a",
		"gl_Color" ".a",
		"color_constant" ".a",
		"0.0",
	};

	static const char* const output_registers[] =
	{
		"color_previous",
		"color_registers" "0",
		"color_registers" "1",
		"color_registers" "2",
	};

	ForEach(stages, [&](const TevStageProps& stage)
	{
		// current texture color
		// 0xff is a common value for a disabled texture
		if (stage.texmap < sampler_count)
			frag_ss << "color_texture = texture2D(textures" << (int)stage.texmap
				<< ", gl_TexCoord[" << (int)stage.texcoord << "].xy);";

		frag_ss << '{';

		// all 4 inputs
		// TODO: stick these directly in the mix() function
		frag_ss << "vec4 a = vec4("
			<< color_inputs[stage.color_a] << ','
			<< alpha_inputs[stage.alpha_a] << ");";

		frag_ss << "vec4 b = vec4("
			<< color_inputs[stage.color_b] << ','
			<< alpha_inputs[stage.alpha_b] << ");";

		frag_ss << "vec4 c = vec4("
			<< color_inputs[stage.color_c] << ','
			<< alpha_inputs[stage.alpha_c] << ");";

		frag_ss << "vec4 d = vec4("
			<< color_inputs[stage.color_d] << ','
			<< alpha_inputs[stage.alpha_d] << ");";

		// TODO: is color_previous always set, no matter the regid?

		// mix the inputs using interpolation/addition
		// TODO: the "d" input is not always added, but it usually is
		frag_ss << "vec4 color = mix(a, b, c) + d;";

		// output register
		frag_ss << output_registers[stage.color_regid] << ".rgb = color" ".rgb;";
		frag_ss << output_registers[stage.alpha_regid] << ".a = color" ".a;";

		frag_ss << '}';
	});

	frag_ss << "gl_FragColor = color_previous;";

	frag_ss << '}';

	std::cout << frag_ss.str() << '\n';

	// create/compile fragment shader
	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	
	{
	const auto& frag_src_str = frag_ss.str();
	const GLchar* frag_src = frag_src_str.c_str();
	glShaderSource(fragment_shader, 1, &frag_src, NULL);
	}

	}	// done generating fragment shader

	glCompileShader(fragment_shader);

	// check compile status of both shaders
	{
	GLint
		vert_compiled = false,
		frag_compiled = false;

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vert_compiled);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &frag_compiled);
	
	std::cout << "compilation: vert/frag: " << vert_compiled << '/' << frag_compiled << '\n';
	}

	// create program, attach shaders
	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	
	// link program, check link status
	glLinkProgram(program);
	GLint link_status;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	std::cout << "link: " << link_status << '\n';

	glUseProgram(program);

	// set uniforms
	for (unsigned int i = 0; i != sampler_count; ++i)
	{
		std::ostringstream ss;
		ss << "textures" << i;
		glUniform1i(glGetUniformLocation(program, ss.str().c_str()), g_texmap_start_index + i);
	}

	glUniform1i(glGetUniformLocation(program, "texture_fb"), g_framebuffer_index);

	// print log
	{
	GLchar infolog[10240] = {};
	glGetProgramInfoLog(program, 10240, NULL, infolog);
	std::cout << infolog;
	}

	// pause
	//std::cin.get();
}

#endif

#if TEV_EMULATION == USE_GLSL

void 	GX_LoadTexObj (GXTexObj *obj, u8 mapid)
{
	const GLTexObj& txobj = *(GLTexObj*)obj;

	glActiveTexture(GL_TEXTURE0 + g_texmap_start_index + mapid);
	glBindTexture(GL_TEXTURE_2D, txobj.tex);
}

#elif TEV_EMULATION == USE_TEXENV

GLuint g_texture_slots[8] = {};

void 	GX_LoadTexObj (GXTexObj *obj, u8 mapid)
{
	const GLTexObj& txobj = *(GLTexObj*)obj;

	g_texture_slots[mapid & 0x7] = txobj.tex;
}

#endif

void 	guMtxRotAxisRad (Mtx mt, guVector *axis, f32 rad)
{
	glRotatef(rad, axis->x, axis->y, axis->z);
}

inline void ActiveStage(u8 stage)
{
#if TEV_EMULATION == USE_GLSL
	
	g_active_stages.resize(std::max(g_active_stages.size(), (size_t)stage + 1));

#elif TEV_EMULATION == USE_TEXENV

	glActiveTexture(GL_TEXTURE0 + stage);

#endif
}

void 	GX_SetTevOrder (u8 tevstage, u8 texcoord, u32 texmap, u8 color)
{
	ActiveStage(tevstage);

#if TEV_EMULATION == USE_GLSL

	g_active_stages[tevstage].texmap = texmap;
	g_active_stages[tevstage].texcoord = texcoord;

	//glActiveTexture(GL_TEXTURE0 + g_texmap_start_index + tevstage);

#elif TEV_EMULATION == USE_TEXENV

	glBindTexture(GL_TEXTURE_2D, g_texture_slots[texmap & 7]);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

#endif
}

void 	GX_SetTevSwapMode (u8 tevstage, u8 ras_sel, u8 tex_sel)
{
	//ActiveStage(tevstage);

	// TODO:
}

void 	GX_SetTevIndirect (u8 tevstage, u8 indtexid, u8 format, u8 bias, u8 mtxid,
	u8 wrap_s, u8 wrap_t, u8 addprev, u8 utclod, u8 a)
{
	ActiveStage(tevstage);
}

void 	GX_SetTevColorS10 (u8 tev_regid, GXColorS10 color)
{
	for (unsigned int i = 0; i != 4; ++i)
		g_color_registers[tev_regid - 1][i] = (float)(&color.r)[i] / 255;
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
	ActiveStage(tevstage);
}

void 	GX_SetTevKColorSel (u8 tevstage, u8 sel)
{
	ActiveStage(tevstage);
}

void 	GX_SetTevAlphaIn (u8 tevstage, u8 a, u8 b, u8 c, u8 d)
{
	ActiveStage(tevstage);

#if TEV_EMULATION == USE_GLSL

	TevStageProps& ts = g_active_stages[tevstage];
	ts.alpha_a = a & 0x7;
	ts.alpha_b = b & 0x7;
	ts.alpha_c = c & 0x7;
	ts.alpha_d = d & 0x7;

#elif TEV_EMULATION == USE_TEXENV

	static const GLint combiner_inputs[] =
	{
		GL_PREVIOUS,
		GL_PREVIOUS,	// TODO: color/output register 0
		GL_PREVIOUS,	// TODO: color/output register 1
		GL_PREVIOUS,	// TODO: color/output register 2
		GL_TEXTURE,
		GL_PRIMARY_COLOR,
		GL_CONSTANT,
		GL_ZERO,
	};

	//glColor4ub(0xff, 0xff, 0xff, 0xff);

	if (0x7 != (a & b & c))
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_INTERPOLATE);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, combiner_inputs[b & 0x7]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, combiner_inputs[a & 0x7]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA, combiner_inputs[c & 0x7]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA, GL_SRC_ALPHA);

		// hax
		//glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
		//glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
		//glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_COLOR);
		//glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_COLOR);
		//glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA, GL_SRC_COLOR);
	}
	else
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, combiner_inputs[d & 0x7]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

		//glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
		//glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
#endif
}

void 	GX_SetTevAlphaOp (u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid)
{
	ActiveStage(tevstage);

#if TEV_EMULATION == USE_GLSL

	TevStageProps& ts = g_active_stages[tevstage];
	ts.alpha_regid = tevregid;

#elif TEV_EMULATION == USE_TEXENV

	//glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, tevscale);

	//glPixelTransferf(GL_ALPHA_BIAS, bias);
	//glPixelTransferf(GL_ALPHA_SCALE, scale);

#endif
}

void 	GX_SetTevColorIn (u8 tevstage, u8 a, u8 b, u8 c, u8 d)
{
	ActiveStage(tevstage);

#if TEV_EMULATION == USE_GLSL

	TevStageProps& ts = g_active_stages[tevstage];
	ts.color_a = a & 0xf;
	ts.color_b = b & 0xf;
	ts.color_c = c & 0xf;
	ts.color_d = d & 0xf;

#elif TEV_EMULATION == USE_TEXENV

	static const GLint combiner_inputs[] =
	{
		GL_PREVIOUS,
		GL_PREVIOUS,
		GL_ZERO,	// TODO: color/output register 0 RGB
		GL_ZERO,	// TODO: color/output register 0 Alpha
		GL_PRIMARY_COLOR,	// TODO: color/output register 1 RGB
		GL_PRIMARY_COLOR,	// TODO: color/output register 1 Alpha
		GL_PRIMARY_COLOR,	// TODO: color/output register 2 RGB
		GL_PRIMARY_COLOR,	// TODO: color/output register 2 Alpha
		GL_TEXTURE,
		GL_TEXTURE,
		GL_PRIMARY_COLOR,
		GL_PRIMARY_COLOR,
		GL_ZERO,	// ONE
		GL_PRIMARY_COLOR,	// TODO: half
		GL_CONSTANT,
		GL_ZERO,
	};

	auto const get_operand = [](u8 input) -> GLint
	{
		if ((input < 12) && (input & 0x1))
			return GL_SRC_ALPHA;
		else if (12 == input)
			return GL_ONE_MINUS_SRC_COLOR;
		else
			return GL_SRC_COLOR;
	};

	if (0xf != (a & b & c))
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, combiner_inputs[b & 0xf]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, get_operand(b));

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, combiner_inputs[a & 0xf]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, get_operand(a));
		
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, combiner_inputs[c & 0xf]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, get_operand(c));
	}
	else
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, combiner_inputs[d & 0xf]);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, get_operand(d));
	}
#endif
}

void 	GX_SetTevColorOp (u8 tevstage, u8 tevop, u8 tevbias, u8 tevscale, u8 clamp, u8 tevregid)
{
	ActiveStage(tevstage);

#if TEV_EMULATION == USE_GLSL

	TevStageProps& ts = g_active_stages[tevstage];
	ts.color_regid = tevregid;

#elif TEV_EMULATION == USE_TEXENV

	//glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, tevscale);

	//glPixelTransferf(GL_RED_BIAS, bias);
	//glPixelTransferf(GL_BLUE_BIAS, bias);
	//glPixelTransferf(GL_GREEN_BIAS, bias);

	//glPixelTransferf(GL_RED_SCALE, scale);
	//glPixelTransferf(GL_BLUE_SCALE, scale);
	//glPixelTransferf(GL_GREEN_SCALE, scale);

#endif
}

void 	GX_SetNumTevStages (u8 num)
{
#if TEV_EMULATION == USE_GLSL

	g_active_stages.resize(num);
	CompiledTevStages& comptevs = g_compiled_tev_stages[g_active_stages];

	// compile program if needed
	if (!comptevs.program)
		comptevs.Compile(g_active_stages);

	// enable the program
	comptevs.Enable();

	// TODO: only do this when needed
	glActiveTexture(GL_TEXTURE0 + g_framebuffer_index);
	glBindTexture(GL_TEXTURE_2D, g_framebuffer_texture);
	//glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 608, 456, 0);
	//std::cout << glGetError() << '\n';

#elif TEV_EMULATION == USE_TEXENV

	static u8 current_num = 0;

	// enable stages
	while (current_num < num)
	{
		ActiveStage(current_num);
		glEnable(GL_TEXTURE_2D);

		++current_num;
	}

	// disable stages
	while (current_num > num)
	{
		--current_num;

		ActiveStage(current_num);
		glDisable(GL_TEXTURE_2D);
	}

#endif
}
