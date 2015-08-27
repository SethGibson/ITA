#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Camera.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"
#include "CinderOpenCV.h"
#include "CiDSAPI.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace CinderDS;

struct Pt
{
	Pt(vec2 pPos) : PPos(pPos)
	{
		PAge = PLife = randInt(30, 120);
		PActive = false;
		PAlpha = 0.0f;
	}

	void Update()
	{
		if (PActive)
		{
			if (PAge > 0)
			{
				--PAge;
				PAlpha = PAge / (float)PLife;
			}
			else
			{
				PAge = PLife = randInt(30, 120);
				PActive = false;
				PAlpha = 0.0f;
			}
		}
	}

	int PLife,
		PAge;

	float PAlpha;
	vec2 PPos;
	bool PActive;
};

class ITA_GridApp : public App
{
public:
	void setup() override;
	void update() override;
	void draw() override;

private:
	void setupScene();
	void setupMesh();

	CinderDSRef	mDS;
	
	gl::VaoRef		mVao;
	gl::VboRef		mVbo;
	gl::BatchRef	mBatch;
	gl::GlslProgRef	mShader;
	vector<Pt>		mPoints;
};

void ITA_GridApp::setup()
{
	setupScene();
	setupMesh();
}

void ITA_GridApp::setupScene()
{
	setWindowSize(960, 720);
	gl::enable(GL_PROGRAM_POINT_SIZE);
}

void ITA_GridApp::setupMesh()
{
	for (int x = 0; x < 960; x += 4)
	{
		for (int y = 0; y < 720; y += 4)
		{
			mPoints.push_back(Pt(vec2(x, y)));
		}
	}

	mShader = gl::GlslProg::create(loadAsset("shaders/grid.vert"), loadAsset("shaders/grid.frag"));
	GLint vertLoc = mShader->getAttribLocation("v_Position");
	GLint alphaLoc = mShader->getAttribLocation("v_Alpha");

	mVao = gl::Vao::create();
	mVbo = gl::Vbo::create(GL_ARRAY_BUFFER, mPoints, GL_DYNAMIC_DRAW);
	mVao->bind();
	mVbo->bind();
	gl::enableVertexAttribArray(vertLoc);
	gl::vertexAttribPointer(vertLoc, 2, GL_FLOAT, false, sizeof(Pt), (const GLvoid *)offsetof(Pt, PPos));
	gl::enableVertexAttribArray(alphaLoc);
	gl::vertexAttribPointer(alphaLoc, 1, GL_FLOAT, false, sizeof(Pt), (const GLvoid *)offsetof(Pt, PAlpha));
	mVao->unbind();
	mVbo->unbind();
}

void ITA_GridApp::update()
{
	for (int i = 0; i < 100; ++i)
	{
		int id = randInt(0, mPoints.size());
		if (!mPoints[id].PActive)
			mPoints[id].PActive = true;
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

	gl::context()->setDefaultShaderVars();
	gl::drawArrays(GL_POINTS, 0, mPoints.size());
}

CINDER_APP( ITA_GridApp, RendererGl )
