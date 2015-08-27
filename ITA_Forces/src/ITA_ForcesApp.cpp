#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct Pt
{
	vec3 PPosition;
	vec3 PVelocity;
	float PAlpha;
	float PInvMass;

	Pt(vec3 pPos, float pAlpha) : PPosition(pPos), PAlpha(pAlpha), PVelocity(vec3(0))
	{
		PInvMass = randFloat(0.0f, 1.0f);
	}
};

class ITA_ForcesApp : public App
{
public:
	void setup() override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;
	void update() override;
	void draw() override;

private:
	void setupScene();
	void setupBuffers();
	void setupShaders();
	void setupGUI();

	gl::VaoRef					mVao[2];
	gl::TransformFeedbackObjRef	mTfo[2];
	gl::VboRef					mVboPos[2],
								mVboVel[2];
	gl::GlslProgRef	mShaderTF,
					mShaderDraw;

	int mId;

	vector<vec3>	mPositions,
					mVelocities;

	float mForceMode;

	params::InterfaceGlRef	mGUI;
	float	mParamRadius,
			mParamMagScale,
			mParamDamping;
};

void ITA_ForcesApp::setup()
{
	setupGUI();
	setupScene();
	setupShaders();
	setupBuffers();
}

void ITA_ForcesApp::setupGUI()
{
	mParamRadius = 0.15f;
	mParamMagScale = 0.35f;
	mParamDamping = 0.995;

	mGUI = params::InterfaceGl::create("Settings", vec2(300,200));
	mGUI->addParam<float>("paramRadius", &mParamRadius).optionsStr("label='Radius Scale'");
	mGUI->addParam<float>("paramMagScale", &mParamMagScale).optionsStr("label='Magnitude Scale'");
	mGUI->addParam<float>("paramDamping", &mParamDamping).optionsStr("label='Damping'");
}

void ITA_ForcesApp::setupScene()
{
	getWindow()->setFullScreen(true);
	setFrameRate(60.0f);
	mForceMode = 1.0f;
	gl::enable(GL_PROGRAM_POINT_SIZE);
}

void ITA_ForcesApp::setupBuffers()
{
	mId = 1;
	for (int x = 0; x < 960; ++x)
	{
		for (int y = 0; y < 720; ++y)
		{
			float xA = lmap<float>(x, 0, 480, 0.0f, 1.0f);
			float yA = lmap<float>(y, 0, 360, 0.0f, 1.0f);
			mPositions.push_back(vec3(x, y, 0));
			mVelocities.push_back(vec3());
		}
	}

	mVboPos[0] = gl::Vbo::create(GL_ARRAY_BUFFER, mPositions, GL_STATIC_DRAW);
	mVboPos[1] = gl::Vbo::create(GL_ARRAY_BUFFER, mPositions.size()*sizeof(vec3), nullptr, GL_STATIC_DRAW);
	mVboVel[0] = gl::Vbo::create(GL_ARRAY_BUFFER, mVelocities, GL_STATIC_DRAW);
	mVboVel[1] = gl::Vbo::create(GL_ARRAY_BUFFER, mVelocities.size()*sizeof(vec3), nullptr, GL_STATIC_DRAW);

	for (int i = 0; i < 2; ++i)
	{
		mVao[i] = gl::Vao::create();
		
		mVao[i]->bind();
		mVboPos[i]->bind();
		gl::vertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
		gl::enableVertexAttribArray(0);

		mVboVel[i]->bind();
		gl::vertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
		gl::enableVertexAttribArray(1);

		mTfo[i] = gl::TransformFeedbackObj::create();
		mTfo[i]->bind();
		gl::bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mVboPos[i]);
		gl::bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, mVboVel[i]);
		mTfo[i]->unbind();
	}
}

void ITA_ForcesApp::setupShaders()
{
	gl::GlslProg::Format tfFormat;
	tfFormat.vertex(loadAsset("shaders/update.vert"))
		.feedbackFormat(GL_SEPARATE_ATTRIBS)
		.feedbackVaryings({ "o_Position", "o_Velocity" })
		.attribLocation("v_Position", 0)
		.attribLocation("v_Velocity", 1);
	mShaderTF = gl::GlslProg::create(tfFormat);
	
	gl::GlslProg::Format drawFormat;
	drawFormat.vertex(loadAsset("shaders/render.vert"))
		.fragment(loadAsset("shaders/render.frag"))
		.attribLocation("v_Position", 0)
		.attribLocation("v_Velocity", 1)
		.attribLocation("v_Alpha", 3);
	mShaderDraw = gl::GlslProg::create(drawFormat);
}

void ITA_ForcesApp::mouseDown(MouseEvent event)
{
	mouseDrag(event);
}

void ITA_ForcesApp::mouseDrag( MouseEvent event )
{
	mForceMode = -0.25f;
}

void ITA_ForcesApp::mouseUp(MouseEvent event)
{
	mForceMode = 1.0f;
}
void ITA_ForcesApp::update()
{
	mId = 1 - mId;
	auto mousePos = getMousePos();
	console() << mousePos.x << ", " << mousePos.y << endl;
	gl::ScopedGlslProg tfShader(mShaderTF);
	mShaderTF->uniform("u_ForcePos", vec3(mousePos.x, mousePos.y, 0.0f));
	mShaderTF->uniform("u_ForceScale", mForceMode);
	mShaderTF->uniform("u_Radius", mParamRadius);
	mShaderTF->uniform("u_MagScale", mParamMagScale);
	mShaderTF->uniform("u_Damping", mParamDamping);

	gl::ScopedVao tfVao(mVao[mId]);
	gl::ScopedState tfState(GL_RASTERIZER_DISCARD, true);

	mTfo[1 - mId]->bind();
	gl::beginTransformFeedback(GL_POINTS);
	gl::drawArrays(GL_POINTS, 0, 960*720);
	gl::endTransformFeedback();
}

void ITA_ForcesApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
	gl::setMatricesWindow(getWindowSize());

	gl::ScopedVao vao(mVao[1-mId]);
	gl::ScopedGlslProg shader(mShaderDraw);
	gl::setDefaultShaderVars();
	gl::drawArrays(GL_POINTS, 0, 960*720);

	mGUI->draw();
	
}

CINDER_APP( ITA_ForcesApp, RendererGl )
