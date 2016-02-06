#ifdef _DEBUG
#pragma comment(lib, "libpxc_d.lib")
#else
#pragma comment(lib, "libpxc.lib")
#endif

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"
#include "pxcsensemanager.h"
#include "pxchandmodule.h"
#include "pxchanddata.h"
#include "pxchandconfiguration.h"
#include "pxcprojection.h"

using namespace ci;
using namespace ci::app;
using namespace std;

const ivec2 NUM_PTS(1280,720);
const ivec2 RGB_SIZE(640, 360);
const ivec2 Z_SIZE(640, 480);
const ivec2 WIN_SIZE(1280, 720);

const float FORCE_A = 0.75f;
const float FORCE_R = -0.75f;
const float P_RADIUS = 1.5f;
const float P_MAG = 2.5f;
const float P_DAMP = 0.975f;

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
	void setupRS();

	void updateRS();
	void getRSCursorPos();
	void getRSGestureState();
	vec2 remapPos(const vec2 &pPos);

	bool	mIdle,
			mMouseInput,
			mIsRunning,
			mHasHand,
			mRepulsing;
	
	vector<vec3> mMousePos;

	gl::VaoRef					mVao[2];
	gl::TransformFeedbackObjRef	mTfo[2];
	gl::VboRef					mVboPos[2],
								mVboVel[2];
	gl::GlslProgRef	mShaderTF,
					mShaderDraw;

	int mId;

	vec2			mActiveX,
					mActiveY;

	vector<vec3>	mPositions,
					mVelocities;

	float	mForceMode,
			mNumInputs;

	params::InterfaceGlRef	mGUI;
	float	mParamRadius,
			mParamMagScale,
			mParamDamping;

	PXCSenseManager	*mRS;
	PXCHandData		*mHandData;
	PXCProjection	*mMapper;

	gl::Texture2dRef mTexRgb;
};

void ITA_ForcesApp::setup()
{
	setupGUI();
	setupScene();
	setupShaders();
	setupBuffers();

	setupRS();
}

void ITA_ForcesApp::setupRS()
{
	mHasHand = false;
	mIsRunning = false;

	mRS = PXCSenseManager::CreateInstance();
	if (mRS == nullptr)
	{
		console() << "Unable to Create SenseManager" << endl;
		return;
	}
	else
	{
		console() << "Created SenseManager" << endl;
		auto stat = mRS->EnableStream(PXCCapture::STREAM_TYPE_COLOR, RGB_SIZE.x, RGB_SIZE.y, 60);
		if (stat >= PXC_STATUS_NO_ERROR) {
			console() << "Color Stream Enabled" << endl;
		}
		stat = mRS->EnableStream(PXCCapture::STREAM_TYPE_DEPTH, Z_SIZE.x, Z_SIZE.y, 60);
		if (stat >= PXC_STATUS_NO_ERROR) {
			console() << "Depth Stream Enabled" << endl;
		}

		stat = mRS->EnableHand();

		if ((stat >= PXC_STATUS_NO_ERROR))//&& (handModule != nullptr))
		{
			stat = mRS->Init();
			auto handModule = mRS->QueryHand();

			if (stat >= PXC_STATUS_NO_ERROR)
			{
				auto depthSize = mRS->QueryCaptureManager()->QueryImageSize(PXCCapture::STREAM_TYPE_DEPTH);
				console() << "DepthSize: " << to_string(depthSize.width) << " " << to_string(depthSize.height) << endl;

				auto colorSize = mRS->QueryCaptureManager()->QueryImageSize(PXCCapture::STREAM_TYPE_COLOR);
				console() << "ColorSize: " << to_string(colorSize.width) << " " << to_string(colorSize.height) << endl;

				mHandData = handModule->CreateOutput();
				auto cfg = handModule->CreateActiveConfiguration();
				if (cfg != nullptr)
				{
					stat = cfg->SetTrackingMode(PXCHandData::TRACKING_MODE_CURSOR);
					if (stat >= PXC_STATUS_NO_ERROR)
						stat = cfg->EnableAllAlerts();
					if (stat >= PXC_STATUS_NO_ERROR)
						stat = cfg->EnableAllGestures(false);
					if (stat >= PXC_STATUS_NO_ERROR)
						stat = cfg->ApplyChanges();
					if (stat >= PXC_STATUS_NO_ERROR)
						cfg->Update();
					if (stat >= PXC_STATUS_NO_ERROR)
					{
						console() << "Hand Tracking Enabled" << endl;
						cfg->Release();
						mRS->QueryCaptureManager()->QueryDevice()->SetMirrorMode(PXCCapture::Device::MIRROR_MODE_HORIZONTAL);
						mIsRunning = true;
					}
				}
				else {
					console() << "Unable to Configure Hand Tracking" << endl;
				}
				mMapper = mRS->QueryCaptureManager()->QueryDevice()->CreateProjection();
				if (mMapper==nullptr) {
					CI_LOG_W("Unable to get coordinate mapper");
				}
			}
			else
				console() << "Unable to Start SenseManager" << endl;
		}
	}
}

void ITA_ForcesApp::setupGUI()
{
	mParamRadius = 0.25f;
	mParamMagScale = 1.5f;
	mParamDamping = 0.98f;
	mMouseInput = false;
	mNumInputs = 0.0f;

	mGUI = params::InterfaceGl::create("Settings", vec2(300,200));
	mGUI->addParam<bool>("paramMouseInput", &mMouseInput).optionsStr("label='Mouse Input'");
	mGUI->addSeparator();
	mGUI->addParam<float>("paramRadius", &mParamRadius).optionsStr("label='Radius Scale'");
	mGUI->addParam<float>("paramMagScale", &mParamMagScale).optionsStr("label='Magnitude Scale'");
	mGUI->addParam<float>("paramDamping", &mParamDamping).optionsStr("label='Damping'");
}

void ITA_ForcesApp::setupScene()
{
	mIdle = true;
	mForceMode = FORCE_A;
	mActiveX = vec2(20.0f, Z_SIZE.x - 20.0f);
	mActiveY = vec2(20.0f, Z_SIZE.y - 20.0f);
	mRepulsing = false;
	mMousePos.push_back(vec3(getWindowSize(), 0.0)*vec3(0.5));
	mMousePos.push_back(vec3(getWindowSize(), 0.0)*vec3(0.5));

	gl::enable(GL_PROGRAM_POINT_SIZE);
}

void ITA_ForcesApp::setupBuffers()
{
	mId = 1;
	auto bounds = vec2(getWindowSize());
	for (int x = 0; x < NUM_PTS.x; ++x)
	{
		for (int y = 0; y < NUM_PTS.y; ++y)
		{
			float xA = lmap<float>(x, 0, NUM_PTS.x, 0.0f, bounds.x);
			float yA = lmap<float>(y, 0, NUM_PTS.y, 0.0f, bounds.y);
			mPositions.push_back(vec3(xA, yA, 0));
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
	{
		mIdle = false;
		if (event.isLeftDown())
			mForceMode = FORCE_A;

		else if (event.isRightDown())
			mForceMode = FORCE_R;
		mouseDrag(event);
	}
}

void ITA_ForcesApp::mouseDrag( MouseEvent event )
{
	if (mMouseInput)
	{
		mMousePos.clear();
		mMousePos.push_back(vec3(event.getPos(), 0.0));
	}

}

void ITA_ForcesApp::mouseUp(MouseEvent event)
{
	if (mMouseInput)
		mIdle = true;
}
void ITA_ForcesApp::update()
{
	if (!mMouseInput)
	{
		if (mIsRunning)
			updateRS();
	}
	else
		mNumInputs = 1.0f;

	if (mIdle)
		mForceMode *= mParamDamping;
	mId = 1 - mId;

	gl::ScopedGlslProg tfShader(mShaderTF);
	//mShaderTF->uniform("u_ForcePos", vec3(mMousePos.x, mMousePos.y, 0.0f));
	mShaderTF->uniform("u_ForcePos", mMousePos.data(), 2);
	mShaderTF->uniform("u_Count", mNumInputs);
	mShaderTF->uniform("u_ForceScale", mForceMode);
	mShaderTF->uniform("u_Radius", P_RADIUS);
	mShaderTF->uniform("u_MagScale", P_MAG);
	mShaderTF->uniform("u_Damping", P_DAMP);
	mShaderTF->uniform("u_Bounds", vec2(getWindowSize()));

	gl::ScopedVao tfVao(mVao[mId]);
	gl::ScopedState tfState(GL_RASTERIZER_DISCARD, true);

	mTfo[1 - mId]->bind();
	gl::beginTransformFeedback(GL_POINTS);
	gl::drawArrays(GL_POINTS, 0, NUM_PTS.x*NUM_PTS.y);
	gl::endTransformFeedback();
}

void ITA_ForcesApp::updateRS()
{
	if (mIsRunning)
	{
		if (mRS->AcquireFrame(false,0) >= PXC_STATUS_NO_ERROR)
		{
			mHandData->Update();
			getRSCursorPos();
			getRSGestureState();

			auto sample = mRS->QuerySample();
			if (sample != nullptr)
			{
				auto rgb = sample->color;
				if (rgb != nullptr)
				{
					PXCImage::ImageData rgbData;
					if (rgb->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &rgbData) >= PXC_STATUS_NO_ERROR)
					{
						auto srf = Surface8u::create(rgbData.planes[0], RGB_SIZE.x, RGB_SIZE.y, rgbData.pitches[0], SurfaceChannelOrder::BGRA);
						auto chan = Channel8u::create(*srf);
						mTexRgb = gl::Texture2d::create(*chan);
						rgb->ReleaseAccess(&rgbData);
					}
				}
			}
			mRS->ReleaseFrame();
		}
	}
}

void ITA_ForcesApp::draw()
{

	gl::clear( Color( 0, 0, 0 ) ); 

	
	gl::setMatricesWindow(getWindowSize());
	if (mTexRgb)
	{
		gl::color(Color(0.15,0.45,1));
		gl::draw(mTexRgb, Rectf(0, 0, getWindowWidth(), getWindowHeight()));
	}
	gl::ScopedVao vao(mVao[1-mId]);
	mShaderDraw->uniform("u_Bounds", vec2(getWindowSize()));
	gl::ScopedGlslProg shader(mShaderDraw);

	gl::enableAdditiveBlending();
	gl::setDefaultShaderVars();
	gl::drawArrays(GL_POINTS, 0, NUM_PTS.x*NUM_PTS.y);

	gl::disableAlphaBlending();
	gl::color(Color::white());
	//mGUI->draw();
}

void ITA_ForcesApp::cleanup()
{
	if (mIsRunning) {
		mRS->Close();
		try {
			mMapper->Release();
		}
		catch (...) {
		
		}
 	}
}

void ITA_ForcesApp::getRSCursorPos()
{
	mHasHand = false;
	mMousePos.clear();
	auto numHands = mHandData->QueryNumberOfHands();
	pxcUID handId;

	mNumInputs = 0.0f;
	for (int i = 0; i < numHands; ++i)
	{
		if (mHandData->QueryHandId(PXCHandData::ACCESS_ORDER_BY_ID, i, handId) >= PXC_STATUS_NO_ERROR)
		{
			PXCHandData::IHand *hand;
			if (mHandData->QueryHandDataById(handId, hand) >= PXC_STATUS_NO_ERROR)
			{
				if (hand->HasCursor())
				{
					PXCHandData::ICursor *cursor;
					if (hand->QueryCursor(cursor) >= PXC_STATUS_NO_ERROR)
					{
						auto cursorPos = cursor->QueryPointImage();

						// Mapping block
						PXCPoint3DF32 uvz[1]{ cursorPos };
						uvz[0].z *= 1000.0f;
						PXCPointF32 ijj[1];
						auto stat = mMapper->MapDepthToColor(1, uvz, ijj);
						//
						if(!mMouseInput) 
							mMousePos.push_back( vec3(remapPos(vec2(ijj[0].x, ijj[0].y)), 0.0));

						mHasHand = true;
						mNumInputs += 1.0f;
					}
				}
			}
		}
	}

	if (numHands < 1)
		mIdle = true;
	else
		mIdle = false;

	mForceMode = mRepulsing ? FORCE_R : FORCE_A;
}

void ITA_ForcesApp::getRSGestureState()
{
	auto numGestures = mHandData->QueryFiredGesturesNumber();
	for (int j = 0; j < numGestures; ++j)
	{
		PXCHandData::GestureData gestData;
		if (mHandData->QueryFiredGestureData(j, gestData) >= PXC_STATUS_NO_ERROR)
		{
			PXCHandData::IHand *hand;
			if (mHandData->QueryHandDataById(gestData.handId, hand) >= PXC_STATUS_NO_ERROR)
			{
				std::wstring str(gestData.name);
				if (str.find(L"cursor_click") != std::string::npos)
				{
					if (mHasHand)
						mRepulsing = !mRepulsing;
				}
			}
		}
	}
}

vec2 ITA_ForcesApp::remapPos(const vec2 &pPos)
{
	auto bounds = getWindowSize();
	return vec2(lmap<float>(pPos.x, mActiveX.x, mActiveX.y, 0, bounds.x), lmap<float>(pPos.y, mActiveY.x, mActiveY.y, 0, bounds.y));
}

void prepareSettings(App::Settings *pSettings)
{
	pSettings->setWindowSize(WIN_SIZE);
	pSettings->setWindowPos(40, 40);
	pSettings->setFrameRate(60.0f);
}

CINDER_APP( ITA_ForcesApp, RendererGl, prepareSettings )
