#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"
#include "CiDSAPI.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace CinderDS;

static int NUM_X = 1000;
static int NUM_Y = 1000;
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
	void cleanup() override;

private:
	void setupScene();
	void setupBuffers();
	void setupShaders();
	void setupGUI();
	void setupDS();

	void updateDS();

	bool	mIdle,
			mMouseInput;
	
	ivec2 mMousePos;

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
			mParamDamping,
			mParamMinDepth,
			mParamMaxDepth;

	CinderDSRef	mDS;
	Channel8uRef	mChanCv;

	
};

void ITA_ForcesApp::setup()
{
	setupGUI();
	setupDS();
	setupScene();
	setupShaders();
	setupBuffers();
}

void ITA_ForcesApp::setupDS()
{
	mDS = CinderDSAPI::create();
	mDS->init();
	mDS->initDepth(FrameSize::DEPTHSD, 60);
	mDS->start();
}

void ITA_ForcesApp::setupGUI()
{
	mParamRadius = 0.2f;
	mParamMagScale = 0.2f;
	mParamDamping = 0.99f;
	mParamMinDepth = 740.0f;
	mParamMaxDepth = 810.0f;
	mMouseInput = true;

	mGUI = params::InterfaceGl::create("Settings", vec2(300,200));
	mGUI->addParam<bool>("paramMouseInput", &mMouseInput).optionsStr("label='Mouse Input'");
	mGUI->addSeparator();
	mGUI->addParam<float>("paramRadius", &mParamRadius).optionsStr("label='Radius Scale'");
	mGUI->addParam<float>("paramMagScale", &mParamMagScale).optionsStr("label='Magnitude Scale'");
	mGUI->addParam<float>("paramDamping", &mParamDamping).optionsStr("label='Damping'");
	mGUI->addSeparator();
	mGUI->addParam<float>("paramMinDepth", &mParamMinDepth).optionsStr("label='Min Depth'");
	mGUI->addParam<float>("paramMaxDepth", &mParamMaxDepth).optionsStr("label='Max Depth'");
}

void ITA_ForcesApp::setupScene()
{
	mIdle = true;
	getWindow()->setSize(1440, 900);
	getWindow()->setPos(40, 40);
	setFrameRate(60.0f);
	mForceMode = 0.0f;
	gl::enable(GL_PROGRAM_POINT_SIZE);
	mMousePos = vec2(getWindowSize())*vec2(0.5);
}

void ITA_ForcesApp::setupBuffers()
{
	mId = 1;
	for (int x = 0; x < NUM_X; ++x)
	{
		for (int y = 0; y < NUM_Y; ++y)
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
	if (mMouseInput)
		mouseDrag(event);
}

void ITA_ForcesApp::mouseDrag( MouseEvent event )
{
	if (mMouseInput)
	{
		mIdle = false;
		mMousePos = event.getPos();
		if (event.isLeftDown())
			mForceMode = 0.75f;

		else if (event.isRightDown())
			mForceMode = -0.25f;
	}

}

void ITA_ForcesApp::mouseUp(MouseEvent event)
{
	if (mMouseInput)
		mIdle = true;
}
void ITA_ForcesApp::update()
{
	updateDS();
	if (mIdle)
		mForceMode *= mParamDamping;
	mId = 1 - mId;

	gl::ScopedGlslProg tfShader(mShaderTF);
	mShaderTF->uniform("u_ForcePos", vec3(mMousePos.x, mMousePos.y, 0.0f));
	mShaderTF->uniform("u_ForceScale", mForceMode);
	mShaderTF->uniform("u_Radius", mParamRadius);
	mShaderTF->uniform("u_MagScale", mParamMagScale);
	mShaderTF->uniform("u_Damping", mParamDamping);
	mShaderTF->uniform("u_Bounds", vec2(getWindowSize()));

	gl::ScopedVao tfVao(mVao[mId]);
	gl::ScopedState tfState(GL_RASTERIZER_DISCARD, true);

	mTfo[1 - mId]->bind();
	gl::beginTransformFeedback(GL_POINTS);
	gl::drawArrays(GL_POINTS, 0, NUM_X*NUM_Y);
	gl::endTransformFeedback();
}

void ITA_ForcesApp::updateDS()
{
	mDS->update();
	auto depthChan = mDS->getDepthFrame();
	mChanCv = Channel8u::create(depthChan->getWidth(), depthChan->getHeight());

	auto iter = mChanCv->getIter();

	float closest = 1000.0f;
	ivec2 closestPt = ivec2();
	while (iter.line())
	{
		while (iter.pixel())
		{
			iter.v() = 64;
			float v = (float)depthChan->getValue(ivec2(iter.x(), iter.y()));
			if (v > mParamMinDepth && v < mParamMaxDepth)
			{
				mChanCv->setValue(ivec2(iter.x(), iter.y()), 255);
				if (v < closest)
				{
					closest = v;
					closestPt.x = iter.x();
					closestPt.y = iter.y();
				}
			}
		}
	}

	if (!mMouseInput)
	{
		if (closest < 1000.0f)
		{
			mIdle = false;
			mForceMode = 0.75f;
			vec2 ws = (vec2)getWindowSize();
			float scaleX = ws.x / 480.0f;
			float scaleY = ws.y / 360.0f;
			mMousePos = vec2(closestPt)*vec2(scaleX, scaleY);
		}
		else
		{
			mIdle = true;
		}
	}
}

void ITA_ForcesApp::draw()
{

	gl::clear( Color( 0, 0, 0 ) ); 
	gl::enableAlphaBlending();
	gl::color(Color::white());
	gl::setMatricesWindow(getWindowSize());

	gl::ScopedVao vao(mVao[1-mId]);
	gl::ScopedGlslProg shader(mShaderDraw);
	gl::setDefaultShaderVars();
	gl::drawArrays(GL_POINTS, 0, NUM_X*NUM_Y);

	gl::color(ColorA(1, 1, 1, 0.5));

	ivec2 ws = getWindowSize();

	auto debugTex = gl::Texture2d::create(*mChanCv);
	gl::draw(debugTex, Rectf(ws.x-240,ws.y-180,ws.x,ws.y));

	gl::disableAlphaBlending();
	gl::color(Color::white());
	mGUI->draw();
}

void ITA_ForcesApp::cleanup()
{
	mDS->stop();
}

CINDER_APP( ITA_ForcesApp, RendererGl )
