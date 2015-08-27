#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Camera.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
#include "cinder/Rand.h"
#include "CiDSAPI.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace CinderDS;


class ITA_GridApp : public App
{
public:
	struct Pt
	{
		Pt(vec2 pPos, ivec2 pLife) : PPos(pPos), PLifeSpan(pLife)
		{
			PAge = PLife = randInt(pLife.x, pLife.y);
			PActive = false;
			PAlpha = 0.0f;
		}

		void Activate(float pAlpha)
		{
			PModAlpha = pAlpha;
			PActive = true;
		}

		void Update()
		{
			if (PActive)
			{
				if (PAge > 0)
				{
					--PAge;
					PAlpha = (PAge / (float)PLife)*PModAlpha;
				}
				else
				{
					int lMin = randInt(PLifeSpan.x*0.5, PLifeSpan.x*1.5);
					int lMax = randInt(lMin, PLifeSpan.y * 2);
					PAge = PLife = randInt(lMin, lMax);
					PActive = false;
					PAlpha = 0.0f;
				}
			}
		}

		int PLife,
			PAge;

		float PAlpha, PModAlpha;
		vec2 PPos;
		bool PActive;
		ivec2 PLifeSpan;
	};

	void setup() override;
	void update() override;
	void draw() override;
	void cleanup() override;

private:
	void setupDS();
	void setupGUI();
	void setupScene();
	void setupMesh();

	CinderDSRef	mDS;
	
	gl::VaoRef		mVao;
	gl::VboRef		mVbo;
	gl::BatchRef	mBatch;
	gl::GlslProgRef	mShader;
	vector<Pt>		mPoints;

	params::InterfaceGlRef	mGUI;
	int					mParamSpawnCount,
						mParamMinLife,
						mParamMaxLife;

	float				mParamMinDepth,
						mParamMaxDepth,
						mParamMinAlpha,
						mParamMaxAlpha,
						mParamPointSize;
};

void ITA_GridApp::setup()
{
	setupDS();
	setupGUI();
	setupScene();
	setupMesh();
}

void ITA_GridApp::setupDS()
{
	mDS = CinderDSAPI::create();
	mDS->init();
	mDS->initDepth(FrameSize::DEPTHSD, 60);
	mDS->start();
}

void ITA_GridApp::setupGUI()
{
	mParamSpawnCount = 200;
	mParamMinLife = 90;
	mParamMaxLife = 180;
	mParamMinAlpha = 0.1f;
	mParamMaxAlpha = 1.0f;
	mParamMinDepth = 100.0f;
	mParamMaxDepth = 1000.0f;
	mParamPointSize = 4.0f;

	mGUI = params::InterfaceGl::create("Settings", vec2(200, 400));
	mGUI->addParam<int>("paramSpawnCount", &mParamSpawnCount).optionsStr("label='Spawn Count'");
	mGUI->addParam<float>("paramMinAlpha", &mParamMinAlpha).optionsStr("label='Min Alpha'");
	mGUI->addParam<float>("paramMaxAlpha", &mParamMaxAlpha).optionsStr("label='Max Alpha'");
	mGUI->addParam<float>("paramMinDepth", &mParamMinDepth).optionsStr("label='Min Depth'");
	mGUI->addParam<float>("paramMaxDepth", &mParamMaxDepth).optionsStr("label='Max Depth'");
	mGUI->addParam<float>("paramPointSize", &mParamPointSize).optionsStr("label='Point Size'");
}

void ITA_GridApp::setupScene()
{
	setWindowSize(960, 720);
	gl::enable(GL_PROGRAM_POINT_SIZE);
	setFrameRate(60.0);
}

void ITA_GridApp::setupMesh()
{
	for (int x = 0; x < 960; x+=4)
	{
		for (int y = 0; y < 720; y+=4)
		{
			mPoints.push_back(Pt(vec2(x, y), ivec2(mParamMinLife,mParamMaxLife)));
		}
	}

	mShader = gl::GlslProg::create(loadAsset("shaders/grid.vert"), loadAsset("shaders/grid.frag"));
	GLint vertLoc = mShader->getAttribLocation("v_Position");
	GLint alphaLoc = mShader->getAttribLocation("v_Alpha");

	mVao = gl::Vao::create();
	mVbo = gl::Vbo::create(GL_ARRAY_BUFFER, mPoints, GL_DYNAMIC_DRAW);

	gl::ScopedVao vao(mVao);
	gl::ScopedBuffer vbo(mVbo);
	gl::enableVertexAttribArray(vertLoc);
	gl::vertexAttribPointer(vertLoc, 2, GL_FLOAT, false, sizeof(Pt), (const GLvoid *)offsetof(Pt, PPos));
	gl::enableVertexAttribArray(alphaLoc);
	gl::vertexAttribPointer(alphaLoc, 1, GL_FLOAT, false, sizeof(Pt), (const GLvoid *)offsetof(Pt, PAlpha));
}

void ITA_GridApp::update()
{
	mDS->update();
	auto depthChan = mDS->getDepthFrame();

	for (int i = 0; i < mParamSpawnCount; ++i)
	{
		int id = randInt(0, mPoints.size());
		int x = (int)(mPoints[id].PPos.x*0.5f);
		int y = (int)(mPoints[id].PPos.y*0.5f);
		float depth = (float)depthChan->getValue(ivec2(x, y));
		float modAlpha = lmap<float>(depth, mParamMaxDepth, mParamMinDepth, mParamMinAlpha, mParamMaxAlpha);
		if (!mPoints[id].PActive)
		{

			if (depth>mParamMinDepth&&depth < mParamMaxDepth)
				mPoints[id].Activate(modAlpha);
		}
		else
			mPoints[id].PModAlpha = modAlpha;
	}
	for (auto p = begin(mPoints); p != end(mPoints); ++p)
	{
		p->Update();
	}

	mVbo->bufferData(mPoints.size()*sizeof(Pt), mPoints.data(), GL_DYNAMIC_DRAW);
}

void ITA_GridApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
	gl::setMatricesWindow(getWindowSize());
	gl::enableAlphaBlending();
	gl::ScopedVao vao(mVao);
	gl::ScopedGlslProg shader(mShader);
	mShader->uniform("u_PointSize", mParamPointSize);
	gl::setDefaultShaderVars();
	gl::drawArrays(GL_POINTS, 0, mPoints.size());

	mGUI->draw();
}

void ITA_GridApp::cleanup()
{
	mDS->stop();
}

CINDER_APP( ITA_GridApp, RendererGl )
