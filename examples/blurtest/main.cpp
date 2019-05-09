#include <iostream>

#include <QApplication>
#include <QPainter>

#include "main.h"

AST::Service service;


MainWindow::MainWindow(QWidget* parent)
: QWidget(parent) 
, connector_(this)
, needsUpdate_(false)
{
	original_ = QImage("image.png");
	original_.convertToFormat(QImage::Format_RGB888);

	setAutoFillBackground(false);

	float sum = 0.f;
	for (int i = 0; i < blurArraySize; ++i)
	{
		const float x = float(i) / (float)(blurArraySize - 1) * 6.f - 3.f;
		blur[i] = 1.f / sqrt(2.f * (float)M_PI) * exp(-0.5f * x * x) / blurArraySize;
		sum += blur[i];
	}

	for (int i = 0; i < blurArraySize; ++i)
	{
		blur[i] *= 1.f / sum;
	}

	source_ = original_;
	state_ = S_Before;
	initTarget();

	startTimer(100);
}

MainWindow::~MainWindow()
{
}

void MainWindow::mouseReleaseEvent(QMouseEvent* evt)
{
	if (state_ == S_Before)
	{
		const QRect rect = source_.rect();
		const uint halfWidth = rect.width() / 2;
		const uint halfHeight = rect.height() / 2;

		QRect rect1 = QRect(rect.left(), rect.top(), halfWidth, halfHeight);
		QRect rect2 = QRect(rect1.right() + 1, rect.top(), rect.right() - rect1.right(), halfHeight);
		QRect rect3 = QRect(rect.left(), rect1.bottom() + 1, halfWidth, rect.bottom() - rect1.bottom());
		QRect rect4 = QRect(rect1.right() + 1, rect1.bottom() + 1, rect2.width(), rect3.height());

		state_ = S_InWork;

		work_ = service.startAutoTask(AST::TW_Light, [&, rect1, rect2, rect3, rect4]()
		{
			blurRect(AST::TW_Light, 6, rect1, true);
			blurRect(AST::TW_Middle, 4, rect2, true);
			blurRect(AST::TW_Heavy, 2, rect3, true);
			blurRect(AST::TW_Heavy, 2, rect4, true);
		}, AST::TaskCallbacks()
			.succeeded([&, rect1, rect2, rect3, rect4, connector { *connector_ }]()
		{
			if (!connector->postFunc([&, rect1, rect2, rect3, rect4]
			{
				source_ = target_;
				initTarget();

				work_ = service.startAutoTask(AST::TW_Light, [&, rect1, rect2, rect3, rect4]()
				{
					blurRect(AST::TW_Light, 6, rect1, false);
					blurRect(AST::TW_Middle, 4, rect2, false);
					blurRect(AST::TW_Heavy, 2, rect3, false);
					blurRect(AST::TW_Heavy, 2, rect4, false);
				}, AST::TaskCallbacks().finished([&]()
				{
					state_ = S_After;
					work_.reset();
				}));
			}))
			{
				// window is killed
			}
		})
			.interrupted([&]()
		{
			state_ = S_After;
			work_.reset();
		}));
	}
	else if (state_ == S_InWork)
	{
		if (auto workP = work_.lock())
			workP->interrupt();
	}
	else if (state_ == S_After)
	{
		source_ = original_;
		state_ = S_Before;
		initTarget();
		update();
	}
}

void MainWindow::blurRect(AST::EnumTaskWeight weight, uint depthLeft, QRect rect, bool hor)
{
	auto task = service.currentTask();
	if (task->isInterrupted())
		return;

	if (depthLeft == 0 || rect.width() <= 2 || rect.height() <= 2)
	{
		service.startAutoTask(weight, [&, rect, hor]()
		{
			auto task = service.currentTask();

			for (uint y = rect.top(); y <= (uint)rect.bottom(); ++y)
			{
				if (task->isInterrupted())
					return;

				for (uint x = rect.left(); x <= (uint)rect.right(); ++x)
				{
					blurPixel(x, y, hor);
				}
			}
		}, AST::TaskCallbacks().finished([&]()
		{
			needsUpdate_.store(true);
		}));
	}
	else
	{
		const uint halfWidth = rect.width() / 2;
		const uint halfHeight = rect.height() / 2;

		QRect rect1 = QRect(rect.left(), rect.top(), halfWidth, halfHeight);
		QRect rect2 = QRect(rect1.right() + 1, rect.top(), rect.right() - rect1.right(), halfHeight);
		QRect rect3 = QRect(rect.left(), rect1.bottom() + 1, halfWidth, rect.bottom() - rect1.bottom());
		QRect rect4 = QRect(rect1.right() + 1, rect1.bottom() + 1, rect2.width(), rect3.height());

		service.startAutoTask(AST::TW_Light, [&, weight, depthLeft, rect1, hor]()
		{
			blurRect(weight, depthLeft - 1, rect1, hor);
		});

		service.startAutoTask(AST::TW_Light, [&, weight, depthLeft, rect2, hor]()
		{
			blurRect(weight, depthLeft - 1, rect2, hor);
		});

		service.startAutoTask(AST::TW_Light, [&, weight, depthLeft, rect3, hor]()
		{
			blurRect(weight, depthLeft - 1, rect3, hor);
		});

		service.startAutoTask(AST::TW_Light, [&, weight, depthLeft, rect4, hor]()
		{
			blurRect(weight, depthLeft - 1, rect4, hor);
		});
	}
};

void MainWindow::initTarget()
{
	target_ = QImage(source_.width(), source_.height(), QImage::Format_ARGB32);
	memset(target_.bits(), 0, source_.width() * source_.height() * 4);
}

void MainWindow::blurPixel(uint x, uint y, bool hor)
{
	float r = 0.f;
	float g = 0.f;
	float b = 0.f;

	if (hor)
	{
		for (uint i = 0; i < blurArraySize; ++i)
		{
			int inrealxsigned = (int(x) - int(blurSize) + int(i)) % source_.width();
			uint inrealx = inrealxsigned > 0.f ? inrealxsigned : source_.width() - 1 + inrealxsigned;

			auto rgb = source_.pixel(inrealx, y);
			r += ((rgb >> 16) & 0xFF) * blur[i];
			g += ((rgb >> 8) & 0xFF) * blur[i];
			b += (rgb & 0xFF) * blur[i];
		}
	}
	else
	{
		for (uint i = 0; i < blurArraySize; ++i)
		{
			int inrealysigned = (int(y) - int(blurSize) + int(i)) % source_.height();
			uint inrealy = inrealysigned > 0.f ? inrealysigned : source_.height() - 1 + inrealysigned;

			auto rgb = source_.pixel(x, inrealy);
			r += ((rgb >> 16) & 0xFF) * blur[i];
			g += ((rgb >> 8) & 0xFF) * blur[i];
			b += (rgb & 0xFF) * blur[i];
		}
	}


	QRgb rgb =
		(0xFF << 24) |
		(uint(r) << 16) |
		(uint(g) << 8) |
		uint(b);

	target_.setPixel(x, y, rgb);
};

void MainWindow::paintEvent(QPaintEvent* evt)
{
	QPainter p(this);

	auto sourceRect = source_.rect();
	float sourceAspect = sourceRect.width() / (float)sourceRect.height();
	float windowAspect = rect().width() / (float)rect().height();
	
	float x, y;
	float resultWidth, resultHeight;

	if (windowAspect > sourceAspect)
	{
		resultHeight = rect().height();
		resultWidth = resultHeight * sourceAspect;
		y = 0;
		x = 0.5f * (rect().width() - resultWidth);
	}
	else
	{
		resultWidth = rect().width();
		resultHeight = resultWidth / sourceAspect;
		x = 0;
		y = 0.5f * (rect().height() - resultHeight);
	}
	
	auto resultRect = QRectF(x, y, resultWidth, resultHeight);
	p.drawImage(resultRect, source_);
	p.drawImage(resultRect, target_);
}

void MainWindow::timerEvent(QTimerEvent* evt)
{
	if (needsUpdate_.exchange(false))
	{
		update();
	}
}

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	// TaskImpl parented to the application so that it
	// will be deleted by the application.
	MainWindow *window = new MainWindow();
	window->showMaximized();

	// This will cause the application to exit when
	// the task signals finished.    
	//QObject::connect(task, SIGNAL(finished()), &a, SLOT(quit()));

	return a.exec();
}