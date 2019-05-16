#include "asynctree.h"

#include <QtCore>
#include <QImage>
#include <QWidget>
#include <atomic>

#include "asynctree_qt_connector.h"

typedef unsigned int uint;

class Task;


class MainWindow : public QWidget
{
	Q_OBJECT

	AST::ScopedQtConnector connector_;

	enum EnumState
	{
		S_Before,
		S_InWork,
		S_After
	};

	std::atomic<EnumState> state_;

	AST::TaskW work_;

#ifdef ASYNCTREE_DEBUG
	static const uint blurSize = 100;
#else
	static const uint blurSize = 1000;
#endif

	static const uint blurArraySize = blurSize * 2 + 1;
	float blur[blurArraySize];

	QImage original_;
	QImage source_;
	QImage target_;

	std::atomic_bool needsUpdate_;

public:
	MainWindow(QWidget *parent = 0);
	~MainWindow();

	void mouseReleaseEvent(QMouseEvent* evt) override;
	void paintEvent(QPaintEvent* evt) override;
	void timerEvent(QTimerEvent* evt) override;

private:
	void initTarget();
	void blurPixel(uint x, uint y, bool hor);
	void blurRect(AST::EnumTaskWeight weight, uint depthLeft, QRect rect, bool hor);

public slots :
	

signals:
};
